#include "http_server.h"

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

void HttpServer::OnConnection(const TcpConnectionPtr& conn) {
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
    // Http header has already resolved data success, continue parse http body.
    if(http_context->GetResolvedStage()) {
        if(rc >= 0) {
            buf->retrieve(rc);
            if(http_context->Completed()) {
                // Already return the message before, do not return again.
                delete http_context;
                http_context = nullptr;
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
            ///@todo callback
            conn->send("Resolved Http completed message success");
            delete http_context;
            http_context = nullptr;
            return;
        }
        else if(http_context->stage() >= HTTP_ON_HEADERS_COMPLETE) {
            // Returned content contains the complete header but does not 
            // include any content, which can be parsed in advance. 
            ///@todo callback
            conn->send("Resolved Http Header success");
            http_context->SetStageToResolved();
            return;
        }
        else {
            // Not enough data to parse, just return and wait next process.
            return;
        }
    }
}

} // end namespace net
} // end namespace var