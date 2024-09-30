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

// Date Sun Sep 29 19:25:30 CST 2024.

#include <gtest/gtest.h>
#include "src/latency_recorder.h"

TEST(LatencyRecorder, latency)
{
    const int N = 100;
    var::LatencyRecorder recorder(3);

    for(int i = 0; i < N; ++i) {
        recorder << i;
    }
    var::detail::Percentile* latency_percentile = recorder.get_latency_percentile();
    var::detail::GlobalPercentileSamples g = latency_percentile->get_value();
    uint32_t total = 0;
    for(size_t i = 0; i < var::detail::NUM_INTERVALS; ++i) {
        total += g.get_interval_at(i).added_count();
    }
    EXPECT_EQ(total, N);

    // PercentileWindow has reset() the precentile.
    sleep(1);
    g = latency_percentile->get_value();
    total = 0;
    for(size_t i = 0; i < var::detail::NUM_INTERVALS; ++i) {
        total += g.get_interval_at(i).added_count();
    }
    EXPECT_EQ(total, 0);

    ///@bug PercentileWindow.get_value() has no data.
    ///@cite fixed, bug problem is Percentile::get_sampler() does not return _sampler pointer to window. 
    var::detail::PercentileWindow* lp_window = recorder.get_latency_percentile_window();
    var::detail::GlobalPercentileSamples g_window = lp_window->get_value();
    total = 0;
    for(size_t i = 0; i < var::detail::NUM_INTERVALS; ++i) {
        total += g_window.get_interval_at(i).added_count();
    }
    EXPECT_EQ(total, N);

    // var::Vector<int64_t, 4> latency_percentiles = recorder.latency_percentiles();
    // LOG_INFO << latency_percentiles[0];
    // LOG_INFO << latency_percentiles[2];
    // LOG_INFO << latency_percentiles[3];
    // LOG_INFO << latency_percentiles[4];

    // for(int i = 1; i < N; ++i) {
    //     recorder << (5 + i);
    // }
    // sleep(1);

    // for(int i = 1; i < N; ++i) {
    //     recorder << (4 + i);
    // }
    // sleep(1);

    // for(int i = 1; i < N; ++i) {
    //     recorder << (3 + i);
    // }
    // sleep(1);

    // int base = (N + 1) / 2;
    // EXPECT_EQ(recorder.latency(1), base + 3);
    // EXPECT_EQ(recorder.latency(), base + 4);
    // EXPECT_EQ(recorder.max_latency(), N + 5 - 1);

    // for(int i = 1; i < N; ++i) {
    //     recorder << (2 + i);
    // }
    // sleep(1);
    // EXPECT_EQ(recorder.latency(1), base + 2);
    // EXPECT_EQ(recorder.latency(), base + 3);
    // EXPECT_EQ(recorder.max_latency(), N + 4 - 1);

    // for(int i = 1; i < N; ++i) {
    //     recorder << (1 + i);
    // }
    // sleep(1);
    // EXPECT_EQ(recorder.latency(1), base + 1);
    // EXPECT_EQ(recorder.latency(), base + 2);
    // EXPECT_EQ(recorder.max_latency(), N + 3 - 1);
}