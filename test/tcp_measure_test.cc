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
#include "net/tcp/TcpServer.h"
#include "net/EventLoopThread.h"
#include "metric/server.h"
#include "metric/var.h"
#include "metric/builtin/common.h"

using namespace var;
using namespace var::net;

class TcpMeasureServer {
public:
    inline TcpMeasureServer(EventLoop* loop, const InetAddress& listenAddr) 
    : server_(loop, listenAddr, "TcpMeasureServer")
    , latency_("tcp")
    , timestamp_(gettimeofday_us()) {
        server_.setConnectionCallback(
            std::bind(&TcpMeasureServer::onConnection, this, _1));
        server_.setMessageCallback(
            std::bind(&TcpMeasureServer::onMessage, this, _1, _2, _3));
    }

    inline void start() {
        server_.start();
    }

private:
    inline void onConnection(const TcpConnectionPtr& conn) {
        LOG_INFO << "TcpMeasureServer - " << conn->peerAddress().toIpPort() << " -> "
                 << conn->localAddress().toIpPort() << " is "
                 << (conn->connected() ? "UP" : "DOWN");
    }

    inline void onMessage(const TcpConnectionPtr& conn,
                 Buffer* buf,
                 Timestamp time) {
        int64_t cur_time = gettimeofday_us();
        int64_t time_interval = cur_time - timestamp_;
        latency_ << time_interval;
        timestamp_ = cur_time;
    }

    var::net::TcpServer server_;
    var::LatencyRecorder latency_;
    int64_t timestamp_;
};

TEST(TcpServer, echo_test) 
{
    StartDummyServerAt(8511);
    EventLoop loop;
    InetAddress listenAddr("0.0.0.0", 1234);
    TcpMeasureServer server(&loop, listenAddr);
    server.start();
    loop.loop();
}