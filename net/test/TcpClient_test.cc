// TcpClient destructs in a different thread.

#include "base/Logging.h"
#include "EventLoopThread.h"
#include "tcp/TcpClient.h"

#include <gtest/gtest.h>

using namespace var;
using namespace var::net;

TEST(TcpClient, wait_for_connect)
{
  Logger::setLogLevel(Logger::DEBUG);

  EventLoopThread loopThread;
  {
    InetAddress serverAddr("127.0.0.1", 1234); // should succeed
    TcpClient client(loopThread.startLoop(), serverAddr, "TcpClient");
    client.connect();
    CurrentThread::sleepUsec(500 * 1000);  // wait for connect
    client.disconnect();
  }

  CurrentThread::sleepUsec(1000 * 1000);
}
