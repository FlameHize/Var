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

#ifndef VAR_SERVER_H
#define VAR_SERVER_H

#include "src/service.h"
#include "src/builtin/tabbed.h"
#include "net/http/http_server.h"
#include "net/base/Thread.h"
#include "net/EventLoop.h"

namespace var {

// Server dispatches requests from web browser clients to registered
// services and sends responses back to clients.
class Server : public noncopyable {
public:
    typedef std::unordered_map<std::string, Service*> ServiceMap;

    explicit Server(const net::InetAddress& addr);
    virtual ~Server();
    
    void Start();

    bool AddBuiltinService(const std::string& service_name, Service* service);

    bool RemoveService(Service* service);

    Service* FindServiceByName(const std::string& service_name) const;

    Service::Method* FindMethodByUrl(const std::string& url_path, 
                                     std::string* unresolved_path) const;

    void PrintTabsBody(std::ostream& os, const char* current_tab_name) const;

private:
    void ProcessRequest(net::HttpRequest* request, 
                        net::HttpResponse* response);

private:    
    ServiceMap _service_map;
    net::InetAddress _addr;
    net::HttpServer _server;
    net::EventLoop _loop;

    // Store TabInfo of services inheriting Tabbed.
    TabInfoList* _tab_info_list;
};

// Test if a dummy server was already started.
bool IsDummyServerRunning();

// Start a dummy server listening at 'port'. If a dummy server was already
// running, this function does noting and fails.
// NOTE: The second parameter(ProfilerLinker) is for linking a profiling
// functions when corresponding macros are defined, just ignore it.
// Return 0 on success, -1 otherwise.
bool StartDummyServerAt(int port);

// *Used to update inside status data in builtin services.
void UpdateInsideStatusData(const char* data, size_t len);

// *Used to send inside cmd data in builtin services.
typedef std::function<void(const char*, size_t, size_t)> InsideCmdCallback;
void RegisterInsideCmdCallback(const InsideCmdCallback& cb);

} // end namespace var

#endif // VAR_SERVER_H