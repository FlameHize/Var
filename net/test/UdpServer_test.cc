#include "base/Logging.h"
#include "base/Timestamp.h"
// #include "base/TimeZone.h"
#include "EventLoop.h"
#include "udp/UdpServer.h"

#include <gtest/gtest.h>

using namespace var;
using namespace var::net;

class TestUdpServer {
public:
    inline TestUdpServer(const InetAddress& listenAddr) 
    : server_(listenAddr, "TestUdpServer") {
        server_.setDatagramCallback(
            std::bind(&TestUdpServer::OnDatagram, this , 
            std::placeholders::_1,
            std::placeholders::_2));
    }

    inline void start() {
        server_.start();
    }
private:
    inline void OnDatagram(const NetworkDatagram& datagram, Timestamp time) {
        LOG_INFO << "data received " << datagram.size() << " bytes "
                 << " at " << time.toFormattedString()
                 << " from " << datagram.senderAddress().toIpPort();
        return;
    }

    var::net::UdpServer server_;
};

TEST(UdpServer, run_test)
{
    // TimeZone tz = TimeZone::loadZoneFile("/usr/share/zoneinfo/Asia/Hong_Kong");
    LOG_INFO << "pid = " << getpid();
    InetAddress listenAddr(2008);
    TestUdpServer server(listenAddr);
    EventLoop loop;
    server.start();
    loop.loop();
}