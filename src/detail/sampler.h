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

// Date: Mon Sep 09 09:58:33 CST 2024

#ifndef VAR_DETAIL_SAMPLER_H
#define VAR_DETAIL_SAMPLER_H

#include "src/util/linked_list.h"
#include "src/util/type_traits.h"
#include "src/util/time.h"

namespace var {
namespace detail {

template<typename T>
struct Sample {
    T data;
    int64_t time_us;

    Sample() : data(), time_us(0) {}
    Sample(const T& data2, int64_t time2) : data(data2), time_us(time2) {}
};

// The base class for all samplers whose take_sample() all called periodically.
class Sampler : public LinkNode<Sampler> {
public:
    Sampler();

    // This function will be called every second(approximately) in a
    // dedicated thread if schedule() is called.
    virtual void take_sample() = 0;

    // Register this sampler globally so that take_sample() will be called
    // periodically.
    void schedule();

    // Call this function instead of delete to destroy the sampler. Deletion
    // of the sampler may be delayed for seconds.
    void destroy();

protected:
    // Prevent direct instantiation.
    // Ensure that all resource cleaning behaviors only occur in derived classes.
    virtual ~Sampler();
};


} // end namespace detail
} // end namespace var


#endif // VAR_DETAIL_SAMPLER_H