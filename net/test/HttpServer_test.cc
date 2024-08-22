#include <gtest/gtest.h>
#include "http/http_server.h"
#include "tcp/TcpClient.h"
#include "EventLoop.h"
#include "EventLoopThread.h"

using namespace var;
using namespace var::net;

TEST(HttpServerTest, resolved_request)
{
    InetAddress addr(2008);

    EventLoop loop;
    HttpServer server(&loop, addr, "httpserver");
    server.Start();

    EventLoopThread loop_thread;
    TcpClient client(loop_thread.startLoop(), addr, "httpclient");
    auto on_message_callback = [](const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) {
        LOG_INFO << buf->retrieveAllAsString();
        return;
    };
    client.setMessageCallback(on_message_callback);
    client.connect();

    const char* http_request = 
        "GET /vars/bthread_count?series HTTP/1.1\r\n"
        "Host: 0.0.0.0:8000\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux aarch64; rv:78.0) Gecko/20100101 Firefox/78.0\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"
        "Accept-Language: zh-CN,zh;q=0.8,zh-TW;q=0.7,zh-HK;q=0.5,en-US;q=0.3,en;q=0.2\r\n"
        "Accept-Encoding: gzip, deflate\r\n"
        "X-Requested-With: XMLHttpRequest\r\n"
        "Connection: keep-alive\r\n"
        "Referer: http://0.0.0.0:8000/vars/\r\n"
        "Cookie: key=%24argon2i%24v%3D19%24m%3D4096%2Ct%3D3%2Cp%3D1%24UFk%2BiH2ZXvkDSdoqlycaVg%24%2FgdItxoX8GduV0PhrM28cALrDUjqHkGH488OQZ8KvZY\r\n"
        "Upgrade-Insecure-Requests: 1\r\n"
        "\r\n"
    ;
    while(!client.connection().get()) {
    }
    LOG_INFO << "http request size: " << strlen(http_request);
    loop.runEvery(1, [&](){
        TcpConnectionPtr conn = client.connection();
        conn->send(http_request, strlen(http_request));
    });
    loop.loop();
}