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

// Date Nov Thur 14 15:34:12 CST 2024.

#ifndef VAR_BUILTIN_PROFILER_SERVICE_H
#define VAR_BUILTIN_PROFILER_SERVICE_H

#include "metric/builtin/service.h"

namespace var {

enum ProfilingType {
    PROFILING_CPU = 0,
    PROFILING_HEAP = 1,
    PROFILING_GROWTH = 2,
    PROFILING_CONTENTION = 3,
    PROFILING_IOBUF = 4,
};

class ProfilerService : public Service {
public:
    ProfilerService();
    ~ProfilerService();

    void heap(net::HttpRequest* request,
              net::HttpResponse* response);

    void heap_internal(net::HttpRequest* request,
                       net::HttpResponse* response);

    void cpu(net::HttpRequest* request,
             net::HttpResponse* response);

    void cpu_internal(net::HttpRequest* request,
                      net::HttpResponse* response);

    void GetTabInfo(TabInfoList*) const override;

private:
    void StartProfiling(net::HttpRequest* request,
                        net::HttpResponse* response,
                        ProfilingType type);

    void DoProfiling(net::HttpRequest* request,
                     net::HttpResponse* response,
                     ProfilingType type);

    void DisplayProfiling(net::HttpRequest* request,
                          net::HttpResponse* response,
                          const char* prof_name,
                          net::Buffer& result_prefix,
                          ProfilingType type);
};

} // end namespace var

#endif // VAR_BUILTIN_PROFILER_SERVICE_H