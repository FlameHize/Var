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

#include "net/http/http_server.h"
#include <unordered_map>

class Service;

namespace var {

// Server dispatches requests from web browser clients to registered
// services and sends responses back to clients.
class Server : public noncopyable {
public:
    explicit Server();
    ~Server();

    struct ServiceProperty {
        Service* service;
        std::string service_name;
    };
    typedef std::unordered_map<std::string, ServiceProperty> ServiceMap;

    typedef std::function<void()> Method;
    struct MethodProperty {
        Service* service;
        std::string http_url;
        Method* method;
    };
    typedef std::unordered_map<std::string, MethodProperty> MethodMap;

private:
    const ServiceProperty*
    FindServicePropertyByFullName(const std::string& fullname) const;

    const ServiceProperty*
    FindServicePropertyByName(const std::string& name) const;

    const MethodProperty*
    FindMethodPropertyByFullName(const std::string& fullname) const;

    const MethodProperty*
    FindMethodPropertyByFullName(const std::string& full_service_name,
                                 const std::string& method_name) const;

    const MethodProperty*
    FindMethodPropertyByURL(const std::string& url_path,
                            std::string* unresolved_path) const;

private:    
    // Use method->full_name() as key.
    MethodMap _method_map;

    // Use service->full_name() as key.
    ServiceMap _fullname_service_map;

    // Use service->name() as key.
    ServiceMap _service_map;

    net::HttpServer _server;
};

// Test if a dummy server was already started.
bool IsDummyServerRunning();

// Start a dummy server listening at 'port'. If a dummy server was already
// running, this function does noting and fails.
// NOTE: The second parameter(ProfilerLinker) is for linking a profiling
// functions when corresponding macros are defined, just ignore it.
// Return 0 on success, -1 otherwise.
int StartDummyServerAt(int port);

} // end namespace var

#endif // VAR_SERVER_H