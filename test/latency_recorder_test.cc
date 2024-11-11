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
#include "metric/latency_recorder.h"

TEST(LatencyRecorderTest, latency)
{
    const int N = 100;
    var::LatencyRecorder recorder(3);

    for(int i = 1; i < N; ++i) {
        recorder << (5 + i);
    }
    sleep(1);

    for(int i = 1; i < N; ++i) {
        recorder << (4 + i);
    }
    sleep(1);

    for(int i = 1; i < N; ++i) {
        recorder << (3 + i);
    }
    sleep(1);

    int base = (N + 1) / 2;
    EXPECT_EQ(recorder.latency(1), base + 3);
    EXPECT_EQ(recorder.latency(), base + 4);
    EXPECT_EQ(recorder.max_latency(), N + 5 - 1);

    for(int i = 1; i < N; ++i) {
        recorder << (2 + i);
    }
    sleep(1);
    EXPECT_EQ(recorder.latency(1), base + 2);
    EXPECT_EQ(recorder.latency(), base + 3);
    EXPECT_EQ(recorder.max_latency(), N + 4 - 1);

    for(int i = 1; i < N; ++i) {
        recorder << (1 + i);
    }
    sleep(1);
    EXPECT_EQ(recorder.latency(1), base + 1);
    EXPECT_EQ(recorder.latency(), base + 2);
    EXPECT_EQ(recorder.max_latency(), N + 3 - 1);

    var::Vector<int64_t, 4> latency_percentiles = recorder.latency_percentiles();
    LOG_INFO << latency_percentiles[0];
    LOG_INFO << latency_percentiles[1];
    LOG_INFO << latency_percentiles[2];
    LOG_INFO << latency_percentiles[3];
}

TEST(LatencyRecorderTest, latency_window)
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

    // bug PercentileWindow.get_value() has no data.
    // fixed, bug problem is Percentile::get_sampler() does not return _sampler pointer to window. 
    var::detail::PercentileWindow* lp_window = recorder.get_latency_percentile_window();
    var::detail::GlobalPercentileSamples g_window = lp_window->get_value();
    total = 0;
    for(size_t i = 0; i < var::detail::NUM_INTERVALS; ++i) {
        total += g_window.get_interval_at(i).added_count();
    }
    EXPECT_EQ(total, N);
}

TEST(LatencyRecorderTest, latency_recorder_qps_accuracy) {
    var::LatencyRecorder lr1(2);
    var::LatencyRecorder lr2(2);
    var::LatencyRecorder lr3(2);
    var::LatencyRecorder lr4(2);
    
    // wait sampler to sample 3 times
    sleep(3);

    auto write = [](var::LatencyRecorder& lr, int times) {   
        for (int i = 0; i < times; ++i) {
            lr << 1;
        }
    };
    write(lr1, 10);
    write(lr2, 11);
    write(lr3, 3);
    write(lr4, 1);
    sleep(1);

    ASSERT_EQ(lr1.count(), 10);
    ASSERT_EQ(lr2.count(), 11);
    ASSERT_EQ(lr3.count(), 3);
    ASSERT_EQ(lr4.count(), 1);

    auto read = [](var::LatencyRecorder& lr, double exp_qps, int window_size = 0) {
        int64_t qps_sum = 0;
        int64_t exp_qps_int = (int64_t)exp_qps;
        for (int i = 0; i < 1000; ++i) {
            int64_t qps = window_size ? lr.qps(window_size): lr.qps();
            EXPECT_GE(qps, exp_qps_int - 1);
            EXPECT_LE(qps, exp_qps_int + 1);
            qps_sum += qps;
        }
        double err = fabs(qps_sum / 1000.0 - exp_qps);
        return err;
    };
    ASSERT_GT(0.1, read(lr1, 10/2.0));
    ASSERT_GT(0.1, read(lr2, 11/2.0));
    ASSERT_GT(0.1, read(lr3, 3/2.0));
    ASSERT_GT(0.1, read(lr4, 1/2.0));

    ASSERT_GT(0.1, read(lr1, 10/3.0, 3));
    ASSERT_GT(0.2, read(lr2, 11/3.0, 3));
    ASSERT_GT(0.1, read(lr3, 3/3.0, 3));
    ASSERT_GT(0.1, read(lr4, 1/3.0, 3));
}