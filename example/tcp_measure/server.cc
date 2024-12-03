#include "net/tcp/TcpServer.h"
#include "net/base/Logging.h"
#include "net/EventLoop.h"

using namespace var;
using namespace var::net;

class PingPongServer {
public:
    inline PingPongServer(EventLoop* loop, const InetAddress& listenAddr) 
    : server_(loop, listenAddr, "PingPongServer") {
        server_.setConnectionCallback(
            std::bind(&PingPongServer::onConnection, this, _1));
        server_.setMessageCallback(
            std::bind(&PingPongServer::onMessage, this, _1, _2, _3));
    }

    inline void start() {
        server_.start();
    }

private:
    inline void onConnection(const TcpConnectionPtr& conn) {
        LOG_INFO << "EchoServer - " << conn->peerAddress().toIpPort() << " -> "
                 << conn->localAddress().toIpPort() << " is "
                 << (conn->connected() ? "UP" : "DOWN");
    }

    inline void onMessage(const TcpConnectionPtr& conn,
                 Buffer* buf,
                 Timestamp time) {
        conn->send(buf);
    }
    var::net::TcpServer server_;
};

int main() {
    EventLoop loop;
    InetAddress addr("0.0.0.0", 1234);
    PingPongServer server(&loop, addr);
    server.start();
    loop.loop();
}