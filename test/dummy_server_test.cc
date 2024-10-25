// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include <gtest/gtest.h>
#include "net/tcp/TcpClient.h"
#include "net/EventLoopThread.h"
#include "src/server.h"
#include "src/var.h"

using namespace var;

#define SERVER_ONLY true

TEST(DummyServerTest, StartDummyServer) {
    net::InetAddress addr(2008);
    StartDummyServerAt(addr.port());

    var::Maxer<int> maxer("FpgaMax");
    var::Miner<int> miner("FpgaMin");
    var::Status<int> status("FpgaStatus", 0);
    var::Adder<int> adder;
    var::Window<var::Adder<int>> window_adder("FpgaWindow", &adder, 10);
    var::LatencyRecorder recorder("DSP");
    
    var::net::Buffer buf;
    char c = 1;
    for(size_t i = 0; i < 60 * 256; ++i) {
        buf.append((char*)&c, sizeof(c));
    }
    
#if SERVER_ONLY
    while(true) {
        var::Timer timer;
        timer.start();
        sleep(1);

        var::UpdateInsideStatusData(buf.peek(), buf.readableBytes());
        buf.retrieveAll();
        ++c;
        for(size_t i = 0; i < 60 * 256; ++i) {
            buf.append((char*)&c, sizeof(c));
        }

        timer.stop();
        int64_t interval = timer.u_elapsed();
        maxer << interval;
        miner << interval;
        status.set_value(interval);
        adder << 1;
        recorder << interval;
    }
#else
    net::EventLoop loop;
    net::EventLoopThread loop_thread;
    net::TcpClient client(loop_thread.startLoop(), addr, "httpclient");
    auto on_message_callback = [](const net::TcpConnectionPtr& conn, net::Buffer* buf, Timestamp time) {
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
        net::TcpConnectionPtr conn = client.connection();
        conn->send(http_request, strlen(http_request));
    });
    loop.loop();
#endif
}