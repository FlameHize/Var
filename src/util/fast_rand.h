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

// Date Thur Sep 26 14:05:20 CST 2024.

#ifndef VAR_UTIL_FAST_RAND_H
#define VAR_UTIL_FAST_RAND_H

#include <cstddef>
#include <stdint.h>
#include <string>

namespace var {
// Generate random values fast without global contentions.
// All functions in this header are thread-safe.
struct FastRandSeed {
    uint64_t s[2];
};

// Initialize the seed.
void init_fast_rand_seed(FastRandSeed* seed);

// Generate an unsigned 64-bit random number inside [0, range) from
// thread-local seed.
// Returns 0 when range is 0.
// Cost: ~30ns
// Note that this can be used as an adapter for std::random_shuffle():
//   std::random_shuffle(myvector.begin(), myvector.end(), butil::fast_rand_less_than);
uint64_t fast_rand_less_than(uint64_t range);

// Generate a random double in [0, 1) from thread-local seed.
// Cost: ~15ns
double fast_rand_double();

} // end namespace var


#endif // VAR_UTIL_FAST_RAND_H