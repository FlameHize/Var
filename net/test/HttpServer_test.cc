#include <gtest/gtest.h>
#include "http/http_server.h"
#include "EventLoop.h"

using namespace var;
using namespace var::net;

TEST(HttpServerTest, resolved_request)
{
    EventLoop loop;
    InetAddress addr(2008);
    HttpServer server(&loop, addr, "httpserver");
    server.Start();
    loop.loop();
}