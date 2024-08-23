#include "http_server.h"
#include <ostream>

namespace var {
namespace net {

HttpServer::HttpServer(EventLoop* loop, 
                       const InetAddress& addr,
                       const std::string& name)
    : _server(loop, addr, name) {
    _server.setConnectionCallback(std::bind(&HttpServer::OnConnection, this, _1));
    _server.setMessageCallback(std::bind(&HttpServer::OnMessage, this, _1, _2, _3));
}

void HttpServer::Start() {
    _server.start();
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
            LOG_ERROR << "Http body parsed error";
            buf->retrieveAll();
            return;
        }
    }
    else if(rc >= 0) {
        // In HTTP protocol parsing, even if the source does not contain 
        // a complete HTTP message, it will still be consumed by the http parser 
        // to avoid repeated parsing in the next time.
        buf->retrieve(rc);
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

void HttpServer::OnHttpMessage(const TcpConnectionPtr& conn, const HttpMessage* http_message) {
    ///@todo
    if(true) {
        std::string verbose_str("[ HTTP REQUEST @");
        std::string remote_side = conn->peerAddress().toIpPort();
        verbose_str.append(remote_side);
        verbose_str.append(" ]");

        std::string request_str = MakeHttpRequestStr(http_message);
        std::string prefix("\r\n> ");
        Buffer buf;
        buf.append(request_str);
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
        LOG_INFO << verbose_str;
    }
}

std::string HttpServer::MakeHttpRequestStr(const HttpMessage* http_message) {
    const HttpHeader& header = http_message->header();
    const Buffer& body = http_message->body();
    const HttpUrl& url = header.url();

    BufferStream os;
    os << HttpMethod2Str(header.method()) << ' ';
    url.PrintWithoutHost(os);
    os << "HTTP/" << header.major_version() << '.'
       << header.minor_version() << "\r\n";
    if(!header.GetHeader("Transfer-Encoding")) {
        os << "Content-Length: " << (body.readableBytes() != 0 ?
            body.readableBytes() : 0) << "\r\n";
    }
    if(!header.GetHeader("Host")) {
        os << "Host: ";
        if(url.host().empty()) {
            os << url.host();
            if(url.port() >= 0) {
                os << ':' << url.port();
            }
        }
        os << "\r\n";
    }
    if(!header.content_type().empty()) {
        os << "Content-Type: " << header.content_type() << "\r\n";
    }
    for(HttpHeader::HeaderIterator it = header.HeaderBegin();
        it != header.HeaderEnd(); ++it) {
        os << it->first << ": " << it->second << "\r\n";
    }
    if(!header.GetHeader("Accept")) {
        os << "Accept: */*\r\n";
    }
    if(!header.GetHeader("User-Agent")) {
        os << "User-Agent: var/1.0 curl/7.0\r\n";
    }
    os << "\r\n";
    if(body.readableBytes() != 0) {
        Buffer temp = body;
        os << "Body: " << temp.retrieveAllAsString() << "\r\n"; 
    }
    
    Buffer buf;
    os.moveTo(buf);
    return buf.retrieveAllAsString();
}

} // end namespace net
} // end namespace var