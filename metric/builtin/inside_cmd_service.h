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

// Date Oct Fri 25 11:05:37 CST 2024.

#ifndef VAR_BUILTIN_INSIDE_CMD_SERVICE_H
#define VAR_BUILTIN_INSIDE_CMD_SERVICE_H

#include "metric/service.h"
#include "metric/builtin/inside_cmd_status_user.h"
#include <mutex>
#include <functional>

namespace var {

class InsideCmdService : public Service {
public:
    typedef std::function<void(const char*, size_t, size_t)> CmdRecvCallback;

    InsideCmdService();

    void register_cmd_recv_callback(const CmdRecvCallback& cb);

    void add_user(net::HttpRequest* request,
                  net::HttpResponse* response);

    void add_user_internal(net::HttpRequest* request,
                           net::HttpResponse* response);

    void delete_user(net::HttpRequest* request,
                     net::HttpResponse* response);

    void update_file(net::HttpRequest* request,
                     net::HttpResponse* response);

    void export_file(net::HttpRequest* request,
                     net::HttpResponse* response);

    void download_file(net::HttpRequest* request,
                       net::HttpResponse* response);

    void show_chip_info(net::HttpRequest* request,
                        net::HttpResponse* response);

    void update_chip_info(net::HttpRequest* request,
                          net::HttpResponse* response);

    void send(net::HttpRequest* request,
              net::HttpResponse* response);

    void default_method(net::HttpRequest* request,
                        net::HttpResponse* response) override;

    void GetTabInfo(TabInfoList*) const override;

private:
    std::vector<InsideCmdStatusUser*>   _user_list;
    CmdRecvCallback                     _cmd_recv_cb;
};

} // end namespace var

#endif // VAR_BUILTIN_INSIDE_CMD_SERVICE_H