#include "http_server.h"
#include <ostream>

namespace var {
namespace net {

void defaultHttpCallback(HttpRequest* request, HttpResponse* response) {
    response->header().set_status_code(HTTP_STATUS_OK);
    response->header().SetHeader("Connection", "keep-alive");
}

HttpServer::HttpServer(EventLoop* loop, 
                       const InetAddress& addr,
                       const std::string& name)
    : _verbose(false)
    , _server(loop, addr, name)
    , _http_callback(defaultHttpCallback) {
    _server.setConnectionCallback(std::bind(&HttpServer::OnConnection, this, _1));
    _server.setMessageCallback(std::bind(&HttpServer::OnMessage, this, _1, _2, _3));
    SetVerbose();
}

void HttpServer::ResetConnContext(const TcpConnectionPtr& conn) {
    if(!conn) {
        return;
    }
    HttpContext* http_context = static_cast<HttpContext*>(conn->getMutableContext());
    if(http_context) {
        delete http_context;
        http_context = nullptr;
    }
    conn->setContext(nullptr);
}

void HttpServer::OnConnection(const TcpConnectionPtr& conn) {
    if(conn->connected()) {
        conn->setContext(nullptr);
    }
    else {
        ResetConnContext(conn);
    }
    conn->setContext(nullptr);
    LOG_TRACE << conn->localAddress().toIpPort() << " -> "
              << conn->peerAddress().toIpPort() << " is "
              << (conn->connected() ? "UP" : "DOWN");
}

void HttpServer::OnMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) {
    HttpContext* http_context = static_cast<HttpContext*>(conn->getMutableContext());
    if(!http_context) {
        http_context = new HttpContext;
        conn->setContext(http_context);
    }
    // Resolved all buf's data.
    size_t rc = http_context->ParseFromBytes(buf->peek(), buf->readableBytes());
    // Http header has already resolved data success, 
    // continue parse http body.
    if(http_context->GetResolvedStage()) {
        if(rc >= 0) {
            buf->retrieve(rc);
            if(http_context->Completed()) {
                // Already return the message before, do not return again.
                ResetConnContext(conn);
                return;
            }
        }
        else {
            // Failed to parse the body, Since header were parsed successfully.
            ResetConnContext(conn);
            buf->retrieveAll();
            LOG_ERROR << "Http body parsed error";
            return;
        }
    }
    else if(rc >= 0) {
        // In HTTP protocol parsing, even if the source does not contain 
        // a complete HTTP message, it will still be consumed by the http parser 
        // to avoid repeated parsing in the next time.
        if(buf->readableBytes() >= rc) {
            buf->retrieve(rc);
        }
        else {
            // Failed to parse the whole message.
            ResetConnContext(conn);
            buf->retrieveAll();
            LOG_ERROR << "Http message parsed error";
            return;
        }
        if(http_context->Completed()) {
            OnHttpMessage(conn, static_cast<HttpMessage*>(http_context));
            ResetConnContext(conn);
            return;
        }
        else if(http_context->stage() >= HTTP_ON_HEADERS_COMPLETE) {
            // Returned content contains the complete header but does not 
            // include any content, which can be parsed in advance. 
            OnHttpMessage(conn, static_cast<HttpMessage*>(http_context));
            http_context->SetStageToResolved();
            return;
        }
        else {
            // Not enough data to parse, just return and wait next process.
            return;
        }
    }
}

void HttpServer::OnHttpMessage(const TcpConnectionPtr& conn, HttpMessage* http_message) {
    HttpHeader* resquest_header = &http_message->header();
    Buffer* request_content = &http_message->body();
    std::string remote_side = conn->peerAddress().toIpPort();
    if(_verbose) {
        OnVerboseHttpMessage(resquest_header, request_content, remote_side, true);
    }
    HttpMessage response;
    _http_callback(http_message, &response);
    
    HttpHeader* response_header = &response.header();
    Buffer* response_content = &response.body();
    response_header->set_version(resquest_header->major_version(), resquest_header->minor_version());
    const std::string* content_type = &response_header->content_type();
    if(content_type->empty()) {
        // Use request's content type if response's is not set.
        content_type = &resquest_header->content_type();
        response_header->set_content_type(*content_type);
    }
    // In HTTP 0.9, the server always closes the connection after sending the
    // response. The client must close its end of the connection after
    // receiving the response.
    // In HTTP 1.0, the server always closes the connection after sending the
    // response UNLESS the client sent a Connection: keep-alive request header
    // and the server sent a Connection: keep-alive response header. If no
    // such response header exists, the client must close its end of the
    // connection after receiving the response.
    // In HTTP 1.1, the server does not close the connection after sending
    // the response UNLESS the client sent a Connection: close request header,
    // or the server sent a Connection: close response header. If such a
    // response header exists, the client must close its end of the connection
    // after receiving the response.
    const std::string* response_conn = response_header->GetHeader("Connection");
    if(!response_conn || strcasecmp(response_conn->c_str(), "close") != 0) {
        const std::string* request_conn = resquest_header->GetHeader("Connection");
        // Before Http 1.1.
        if((resquest_header->major_version() * 10000 + 
            resquest_header->minor_version() * 10000) <= 10000) {
            if(request_conn && strcasecmp(request_conn->c_str(), "keep-alive") == 0) {
                response_header->SetHeader("Connection", "keep-alive");
            }
        }
        else {
            if(request_conn && strcasecmp(request_conn->c_str(), "close") == 0) {
                response_header->SetHeader("Connection", "close");
            }
        }
    }

    std::string response_str = MakeHttpReponseStr(response_header, response_content);
    conn->send(response_str);
    response_conn = response_header->GetHeader("Connection");
    if(response_conn && strcasecmp(response_conn->c_str(), "close") == 0) {
        conn->shutdown();
    }
    if(_verbose) {
        OnVerboseHttpMessage(response_header, response_content, remote_side, false);
    }
}

void HttpServer::OnVerboseHttpMessage(HttpHeader* header, 
                                      Buffer* content, 
                                      std::string remote_side, 
                                      bool request_or_response) {
    std::string verbose_str;
    std::string request_or_response_str;
    Buffer buf;
    if(request_or_response) {
        verbose_str = std::string("[ HTTP REQUEST @");
        request_or_response_str = MakeHttpRequestStr(header, content);
    }
    else {
        verbose_str = std::string("[ HTTP RESPONSE @");
        request_or_response_str = MakeHttpReponseStr(header, content);
    }
    verbose_str.append(remote_side);
    verbose_str.append(" ]");
    buf.append(request_or_response_str);

    std::string prefix("\r\n> ");
    size_t last_pos = 0;
    size_t cur_pos = 0;
    while(buf.findCRLF(buf.peek() + cur_pos)) {
        verbose_str.append(prefix);
        last_pos = cur_pos;
        const char* line = buf.findCRLF(buf.peek() + cur_pos);
        cur_pos = line - buf.peek();
        verbose_str.append(buf.peek() + last_pos, cur_pos - last_pos);
        cur_pos += 2;
    }
    verbose_str.pop_back();
    verbose_str.pop_back();
    if(cur_pos != buf.readableBytes()) {
        // For http response.
        verbose_str.append(prefix);
        verbose_str.append("Body: ");
        verbose_str.append(buf.peek() + cur_pos, buf.readableBytes() - cur_pos);
        verbose_str.append("\r\n");
    }
    LOG_INFO << verbose_str;
}

std::string HttpServer::MakeHttpRequestStr(HttpHeader* header, Buffer* content) {
    HttpUrl& url = header->url();

    BufferStream os;
    os << HttpMethod2Str(header->method()) << ' ';
    url.PrintWithoutHost(os);
    os << " HTTP/" << header->major_version() << '.'
       << header->minor_version() << "\r\n";
    // Never use "Content-Length" set by user.
    header->RemoveHeader("Content-Length");
    const std::string* transfer_encoding = header->GetHeader("Transfer-Encoding");
    if(header->method() == HTTP_METHOD_GET) {
        header->RemoveHeader("Transfer-Encoding");
    }
    else if(!transfer_encoding) {
        // A sender MUST NOT send a Content-Length header field in any message
        // that contains a Transfer-Encoding header field.
        os << "Content-Length: " << (content->readableBytes() != 0 ?
              content->readableBytes() : 0) << "\r\n";
    }
    if(!header->GetHeader("Host")) {
        os << "Host: ";
        if(url.host().empty()) {
            os << url.host();
            if(url.port() >= 0) {
                os << ':' << url.port();
            }
        }
        os << "\r\n";
    }
    if(!header->content_type().empty()) {
        os << "Content-Type: " << header->content_type() << "\r\n";
    }
    for(HttpHeader::HeaderIterator it = header->HeaderBegin();
        it != header->HeaderEnd(); ++it) {
        os << it->first << ": " << it->second << "\r\n";
    }
    if(!header->GetHeader("Accept")) {
        os << "Accept: */*\r\n";
    }
    if(!header->GetHeader("User-Agent")) {
        os << "User-Agent: var/1.0 curl/7.0\r\n";
    }
    const std::string& user_info = url.user_info();
    if(!user_info.empty() && !header->GetHeader("Authorization")) {
        // NOTE: just assume user_info is well formatted namely
        // "<user_name><password>". Users are very unlikely to add extra
        // characters in this part and even if users did, most of them are
        // invaild and rejected by http_parser_parse_url().
        os << "Authorization: " << user_info << "\r\n";
    }
    os << "\r\n";
    if(header->method() != HTTP_METHOD_GET && content->readableBytes() != 0) {
        Buffer temp = *content;
        os << temp.retrieveAllAsString(); 
    }
    
    Buffer buf;
    os.moveTo(buf);
    return buf.retrieveAllAsString();
}

std::string HttpServer::MakeHttpReponseStr(HttpHeader* header, Buffer* content) {
    BufferStream os;
    os << "HTTP/" << header->major_version() << '.'
       << header->minor_version() << ' '
       << header->status_code() << ' '
       << header->reason_phrase() << "\r\n";
    bool is_invaild_content = header->status_code() < HTTP_STATUS_OK ||
                              header->status_code() == HTTP_STATUS_NO_CONTENT;
    // Just request http header not contains http body.
    bool is_head_req = header->method() == HTTP_METHOD_HEAD;
    if(is_invaild_content) {
        // A server MUST NOT send a Transfer-Encoding header field in any
        // response with a status code of 1xx(Informational) or 204(No Content).
        header->RemoveHeader("Transfer-Encoding");
        // A server MUST NOT send a Content-Length header field in any
        // response with a status code of 1xx(Informational) or 204(No Content).
        header->RemoveHeader("Content-Length");
    }
    else {
        const std::string* transfer_encoding = header->GetHeader("Transfer-Encoding");
        if(transfer_encoding) {
            header->RemoveHeader("Content-Length");
        }
        if(content) {
            const std::string* content_length = header->GetHeader("Content-Length");
            if(is_head_req) {
                if(!content_length && !transfer_encoding) {
                    // Prioritize "Content-Length" set by user.
                    // If "Content-Length" is not set, set it to the length of content.
                    os << "Content-Length: " << content->readableBytes() << "\r\n";
                }
            }
            else {
                if(!transfer_encoding) {
                    if(content_length) {
                        header->RemoveHeader("Content-Length");
                    }
                    // Never use "Content-Length" set by user.
                    // Always set Content-Length size lighttpd requires 
                    // the header set to 0 for empty content.
                    os << "Content-Length: " << content->readableBytes() << "\r\n";
                }
            }
        }
    }
    if(!is_invaild_content && !header->content_type().empty()) {
        os << "Content-Type: " << header->content_type() << "\r\n";
    }
    for(HttpHeader::HeaderIterator it = header->HeaderBegin(); 
        it != header->HeaderEnd(); ++it) {
        os << it->first << ": " << it->second << "\r\n";
    }
    os << "\r\n";
    
    Buffer result;
    os.moveTo(result);
    if(!is_invaild_content && !is_head_req && content) {
        result.append(*content);
    }
    return result.retrieveAllAsString();
}

void HttpServer::FillUnresolvedPath(std::string* unresolved_path,
                                    const std::string& url_path,
                                    StringSplitter& splitter) {
    if(!unresolved_path) {
        return;
    }
    if(!splitter) {
        unresolved_path->clear();
        return;
    }
    const size_t path_len = url_path.c_str() + url_path.size() - splitter.field();
    unresolved_path->reserve(path_len);
    unresolved_path->clear();
    for(StringSplitter sp(splitter.field(), splitter.field() + path_len, '/');
            sp != nullptr; ++sp) {
        if(!unresolved_path->empty()) {
            unresolved_path->push_back('/');
        }
        unresolved_path->append(sp.field(), sp.length());
    }
}

} // end namespace net
} // end namespace var