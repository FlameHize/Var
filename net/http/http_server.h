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

#ifndef VAR_HTTP_SERVER_H
#define VAR_HTTP_SERVER_H

#include "http_context.h"
#include "tcp/TcpServer.h"
#include "base/Logging.h"

namespace var {
namespace net {

class HttpServer : noncopyable {
public:
    explicit HttpServer(EventLoop* loop, 
                        const InetAddress& addr, 
                        const std::string& name);

    void Start();

private:
    void OnConnection(const TcpConnectionPtr& conn);
    void OnMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time);

private:
    TcpServer _server;
};

} // end namespace net
} // end namespace var

#endif // VAR_HTTP_SERVER_H