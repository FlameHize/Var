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

// Date Nov Tues 05 14:30:00 CST 2024.

#ifndef VAR_BUILTIN_XVC_SERVICE_H
#define VAR_BUILTIN_XVC_SERVICE_H

#include "src/service.h"

namespace var {

class XvcService : public Service {
public:
    void default_method(net::HttpRequest* request,
                        net::HttpResponse* response) override;

    void GetTabInfo(TabInfoList*) const override;
};

} // end namespace var

#endif // VAR_BUILTIN_XVC_SERVICE_H