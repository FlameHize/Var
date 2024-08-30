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
#include "base/StringSplitter.h"

namespace var {
namespace net {

class HttpServer : noncopyable {
public:
    typedef std::function<void(HttpRequest*, HttpResponse*)> HttpCallback;
    explicit HttpServer(EventLoop* loop, 
                        const InetAddress& addr, 
                        const std::string& name);

    void Start() { _server.start(); }
    void SetVerbose() { _verbose = true; }
    void SetHttpCallback(const HttpCallback& cb) { _http_callback = cb; }

    static std::string MakeHttpRequestStr(HttpHeader* header, Buffer* content);
    static std::string MakeHttpReponseStr(HttpHeader* header, Buffer* content);

    static void FillUnresolvedPath(std::string* unresolved_path,
                                   const std::string& url_path,
                                   StringSplitter& splitter);

private:
    void ResetConnContext(const TcpConnectionPtr& conn);
    void OnConnection(const TcpConnectionPtr& conn);
    void OnMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time);
    void OnHttpMessage(const TcpConnectionPtr& conn, HttpMessage* http_message);
    void OnVerboseHttpMessage(HttpHeader* header, Buffer* content, std::string remote_side, bool request_or_response);

private:
    bool _verbose;
    TcpServer _server;
    HttpCallback _http_callback;
};

} // end namespace net
} // end namespace var

#endif // VAR_HTTP_SERVER_H