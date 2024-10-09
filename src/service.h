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

#ifndef VAR_SERVICE_H
#define VAR_SERVICE_H

#include "src/builtin/tabbed.h"
#include "net/http/http_context.h"
#include "net/base/Logging.h"
#include <memory>
#include <functional>

namespace var {

// Put commonly used std::strings (or other constants that need memory
// allocations) in this struct to avoid memory allocations for each request.
struct CommonStrings {
    std::string DEFAULT_PATH;
    std::string DEFAULT_METHOD;
    CommonStrings();
};

// Dispatches requests from web browser url.
// Abstract, Inherit this class to implement different types services.
class Service : public Tabbed {
public:
    typedef std::function<void(net::HttpRequest*, net::HttpResponse*)> Method;
    typedef std::unordered_map<std::string, Method> MethodMap;

    explicit Service();
    virtual ~Service();
    void AddMethod(const std::string& method_name, const Method& method);
    Method* FindMethodByName(const std::string& method_name) const;
    
    virtual void default_method(net::HttpRequest* request,
                                net::HttpResponse* response);

public:
    void* _owner;
    std::string _name;

private:
    MethodMap _method_map;
};

} // end namespace var

#endif // VAR_SERVICE_H