#include "base/Logging.h"
#include "EventLoop.h"
#include "EventLoopThread.h"
#include "tcp/TcpServer.h"

#include <gtest/gtest.h>

using namespace var;
using namespace var::net;

class TestEchoServer {
public:
    inline TestEchoServer(EventLoop* loop, const InetAddress& listenAddr) 
    : server_(loop, listenAddr, "TestEchoServer") {
        server_.setConnectionCallback(
            std::bind(&TestEchoServer::onConnection, this, _1));
        server_.setMessageCallback(
            std::bind(&TestEchoServer::onMessage, this, _1, _2, _3));
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
        var::string msg(buf->retrieveAllAsString());
        LOG_INFO << conn->name() << " echo " << msg.size() << " bytes, "
                 << "data received at " << time.toFormattedString();
        conn->send(msg);
    }
    var::net::TcpServer server_;
};

TEST(TcpServer, echo_test) 
{
    LOG_INFO << "pid = " << getpid();
    EventLoop loop;
    InetAddress listenAddr(2007);
    TestEchoServer server(&loop, listenAddr);
    server.start();
    loop.loop();
}