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
        return;
    };
    client.setMessageCallback(on_message_callback);
    client.connect();

    const char* http_request = 
        "GET /vars/bthread_count?series HTTP/1.1\r\n"
        "Host: 0.0.0.0:8000\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 16\r\n"
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
        "Body [1,2,3,4,5]\r\n"
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

TEST(HttpServerTest, serialize_http_request)
{
    HttpHeader header;
    ASSERT_EQ(0u, header.HeaderCount());
    header.SetHeader("Foo", "Bar");
    ASSERT_EQ(1u, header.HeaderCount());
    header.set_method(HTTP_METHOD_POST);
    header.SetHeader("Host", "127.0.0.1:1234");
    Buffer content;
    content.append("data");
    std::string request = HttpServer::MakeHttpRequestStr(&header, &content);
    ASSERT_EQ("POST / HTTP/1.1\r\nContent-Length: 4\r\nHost: 127.0.0.1:1234\r\nFoo: Bar\r\nAccept: */*\r\nUser-Agent: var/1.0 curl/7.0\r\n\r\ndata", request);

    // user-set content-length is ignored.
    header.SetHeader("Content-Length", "100");
    request = HttpServer::MakeHttpRequestStr(&header, &content);
    ASSERT_EQ("POST / HTTP/1.1\r\nContent-Length: 4\r\nHost: 127.0.0.1:1234\r\nFoo: Bar\r\nAccept: */*\r\nUser-Agent: var/1.0 curl/7.0\r\n\r\ndata", request);

    // user-host overwrites passed-in remote_side
    header.SetHeader("Host", "MyHost: 4321");
    request = HttpServer::MakeHttpRequestStr(&header, &content);
    ASSERT_EQ("POST / HTTP/1.1\r\nContent-Length: 4\r\nHost: MyHost: 4321\r\nFoo: Bar\r\nAccept: */*\r\nUser-Agent: var/1.0 curl/7.0\r\n\r\ndata", request);

    // user-set accept
    header.SetHeader("accePT"/*intended uppercase*/, "blahblah");
    request = HttpServer::MakeHttpRequestStr(&header, &content);
    ASSERT_EQ("POST / HTTP/1.1\r\nContent-Length: 4\r\naccePT: blahblah\r\nHost: MyHost: 4321\r\nFoo: Bar\r\nUser-Agent: var/1.0 curl/7.0\r\n\r\ndata", request);

    // user-set UA
    header.SetHeader("user-AGENT", "myUA");
    request = HttpServer::MakeHttpRequestStr(&header, &content);
    ASSERT_EQ("POST / HTTP/1.1\r\nContent-Length: 4\r\naccePT: blahblah\r\nuser-AGENT: myUA\r\nHost: MyHost: 4321\r\nFoo: Bar\r\n\r\ndata", request);

    // user-set Authorization
    header.SetHeader("authorization", "myAuthString");
    request = HttpServer::MakeHttpRequestStr(&header, &content);
    ASSERT_EQ("POST / HTTP/1.1\r\nContent-Length: 4\r\nauthorization: myAuthString\r\naccePT: blahblah\r\nuser-AGENT: myUA\r\nHost: MyHost: 4321\r\nFoo: Bar\r\n\r\ndata", request);

    header.SetHeader("Transfer-Encoding", "chunked");
    request = HttpServer::MakeHttpRequestStr(&header, &content);
    ASSERT_EQ("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\nauthorization: myAuthString\r\naccePT: blahblah\r\nuser-AGENT: myUA\r\nHost: MyHost: 4321\r\nFoo: Bar\r\n\r\ndata", request);

    // GET does not serialize content and user-set content-length is ignored.
    header.set_method(HTTP_METHOD_GET);
    header.SetHeader("Content-Length", "100");
    request = HttpServer::MakeHttpRequestStr(&header, &content);
    ASSERT_EQ("GET / HTTP/1.1\r\nauthorization: myAuthString\r\naccePT: blahblah\r\nuser-AGENT: myUA\r\nHost: MyHost: 4321\r\nFoo: Bar\r\n\r\n", request);
}

TEST(HttpServerTest, serialize_http_response) 
{
    HttpHeader header;
    header.SetHeader("Foo", "Bar");
    header.set_method(HTTP_METHOD_POST);
    Buffer content;
    content.append("data");
    std::string response = HttpServer::MakeHttpReponseStr(&header, &content);
    ASSERT_EQ("HTTP/1.1 200 OK\r\nContent-Length: 4\r\nFoo: Bar\r\n\r\ndata", response);

    // NULL content
    header.SetHeader("Content-Length", "100");
    response = HttpServer::MakeHttpReponseStr(&header, nullptr);
    ASSERT_EQ("HTTP/1.1 200 OK\r\nContent-Length: 100\r\nFoo: Bar\r\n\r\n", response);

    header.SetHeader("Transfer-Encoding", "chunked");
    response = HttpServer::MakeHttpReponseStr(&header, nullptr);
    ASSERT_EQ("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nFoo: Bar\r\n\r\n", response);
    header.RemoveHeader("Transfer-Encoding");

    // User-set content-length is ignored.
    content.retrieveAll();
    content.append("data2");
    response = HttpServer::MakeHttpReponseStr(&header, &content);
    ASSERT_EQ("HTTP/1.1 200 OK\r\nContent-Length: 5\r\nFoo: Bar\r\n\r\ndata2", response);

    content.retrieveAll();
    header.SetHeader("Content-Length", "100");
    header.SetHeader("Transfer-Encoding", "chunked");
    response = HttpServer::MakeHttpReponseStr(&header, nullptr);
    ASSERT_EQ("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nFoo: Bar\r\n\r\n", response);
    header.RemoveHeader("Transfer-Encoding");

    // User-set content-length and transfer-encoding is ignored when status code is 204 or 1xx.
    // 204 No Content.
    content.retrieveAll();
    header.SetHeader("Content-Length", "100");
    header.SetHeader("Transfer-Encoding", "chunked");
    header.set_status_code(HTTP_STATUS_NO_CONTENT);
    response = HttpServer::MakeHttpReponseStr(&header, &content);
    ASSERT_EQ("HTTP/1.1 204 No Content\r\nFoo: Bar\r\n\r\n", response);
    // 101 Continue
    content.retrieveAll();
    header.SetHeader("Content-Length", "100");
    header.SetHeader("Transfer-Encoding", "chunked");
    header.set_status_code(HTTP_STATUS_CONTINUE);
    response = HttpServer::MakeHttpReponseStr(&header, &content);
    ASSERT_EQ("HTTP/1.1 100 Continue\r\nFoo: Bar\r\n\r\n", response);

    // when request method is HEAD:
    // 1. There isn't user-set content-length, length of content is used.
    header.set_method(HTTP_METHOD_HEAD);
    header.set_status_code(HTTP_STATUS_OK);
    content.retrieveAll();
    content.append("data2");
    response = HttpServer::MakeHttpReponseStr(&header, &content);
    ASSERT_EQ("HTTP/1.1 200 OK\r\nContent-Length: 5\r\nFoo: Bar\r\n\r\n", response);
    // 2. User-set content-length is not ignored .
    content.retrieveAll();
    header.SetHeader("Content-Length", "100");
    response = HttpServer::MakeHttpReponseStr(&header, &content);
    ASSERT_EQ("HTTP/1.1 200 OK\r\nContent-Length: 100\r\nFoo: Bar\r\n\r\n", response);
}