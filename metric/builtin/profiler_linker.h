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

// Date Nov Mon 25 09:21:08 CST 2024.

#ifndef VAR_BUILTIN_PROFILER_LINKER_H
#define VAR_BUILTIN_PROFILER_LINKER_H

#if defined(VAR_ENABLE_CPU_PROFILER)
#include "metric/builtin/gperftools_profiler.h"
#endif

namespace var {

extern bool cpu_profiler_enabled;

struct ProfilerLinker {
    // [ MUST be inlined ]
    // This function is included by user's compliation unit to force
    // linking of ProfilerStart() / ProfilerStop()
    // etc when corresponding macros are defined.
    inline ProfilerLinker() {
#if defined(VAR_ENABLE_CPU_PROFILER)
    cpu_profiler_enabled = true;
#endif
    }
};

} // end namespace var

#endif