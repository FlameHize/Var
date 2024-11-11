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

// Date Fri Sep 13 09:06:10 CST 2024.

#include <gtest/gtest.h>
#include "metric/average_recorder.h"
#include "metric/window.h"
#include "metric/util/time.h"

TEST(AverageRecorderTest, average)
{
    var::AverageRecorder recorder;
    recorder << 1;
    recorder << 3;
    recorder << 5;
    ASSERT_EQ(3l, (int64_t)recorder.average());

    var::AverageRecorder recorder2;
    recorder2 << -1;
    recorder2 << -3;
    recorder2 << -5;
    ASSERT_EQ(-3l, (int64_t)recorder2.average());
}

TEST(AverageRecorderTest, sanity)
{
    {
        var::AverageRecorder recorder;
        ASSERT_TRUE(recorder.valid());
        ASSERT_EQ(0, recorder.expose("var1"));
        for (size_t i = 0; i < 100; ++i) {
            recorder << 2;
        }
        ASSERT_EQ(2l, (int64_t)recorder.average());
        ASSERT_EQ("2", var::Variable::describe_exposed("var1"));
        std::vector<std::string> vars;
        var::Variable::list_exposed(&vars);
        ASSERT_EQ(1UL, vars.size());
        ASSERT_EQ("var1", vars[0]);
        ASSERT_EQ(1UL, var::Variable::count_exposed());
    }
    ASSERT_EQ(0UL, var::Variable::count_exposed());

    var::AverageRecorder recorder;
    for(int i = -10000000; i < 10000000; ++i) {
        recorder << i;
    }
    ASSERT_EQ(0ul, recorder.average());
}

TEST(AverageRecorderTest, positive_overflow) 
{
    var::AverageRecorder recorder1;
    ASSERT_TRUE(recorder1.valid());
    for (int i = 0; i < 5; ++i) {
        recorder1 << std::numeric_limits<int64_t>::max();
    }
    ASSERT_EQ(std::numeric_limits<int>::max(), recorder1.average());

    var::AverageRecorder recorder2;
    ASSERT_TRUE(recorder2.valid());
    recorder2.set_debug_name("recorder2");
    for (int i = 0; i < 5; ++i) {
        recorder2 << std::numeric_limits<int64_t>::max();
    }
    ASSERT_EQ(std::numeric_limits<int>::max(), recorder2.average());

    var::AverageRecorder recorder3;
    ASSERT_TRUE(recorder3.valid());
    recorder3.expose("recorder3");
    for (int i = 0; i < 5; ++i) {
        recorder3 << std::numeric_limits<int64_t>::max();
    }
    ASSERT_EQ(std::numeric_limits<int>::max(), recorder3.average());
}

TEST(AverageRecorderTest, negtive_overflow) {
    var::AverageRecorder recorder1;
    ASSERT_TRUE(recorder1.valid());
    for (int i = 0; i < 5; ++i) {
        recorder1 << std::numeric_limits<int64_t>::min();
    }
    ASSERT_EQ(std::numeric_limits<int>::min(), recorder1.average());

    var::AverageRecorder recorder2;
    ASSERT_TRUE(recorder2.valid());
    recorder2.set_debug_name("recorder2");
    for (int i = 0; i < 5; ++i) {
        recorder2 << std::numeric_limits<int64_t>::min();
    }
    ASSERT_EQ(std::numeric_limits<int>::min(), recorder2.average());

    var::AverageRecorder recorder3;
    ASSERT_TRUE(recorder3.valid());
    recorder3.expose("recorder3");
    for (int i = 0; i < 5; ++i) {
        recorder3 << std::numeric_limits<int64_t>::min();
    }
    ASSERT_EQ(std::numeric_limits<int>::min(), recorder3.average());
}

TEST(AverageRecorderTest, window)
{
    var::AverageRecorder recorder;
    var::Window<var::AverageRecorder> w1(&recorder, 1);
    var::Window<var::AverageRecorder> w2(&recorder, 2);
    var::Window<var::AverageRecorder> w3(&recorder, 3);
    const int N = 10000;
    int64_t last_time = var::gettimeofday_us();
    for(int i = 1; i <= N; ++i) {
        recorder << i;
        int64_t now = var::gettimeofday_us();
        if(now - last_time >= 1000000L) {
            last_time = now;
            LOG_INFO << "recorder = " << recorder.average() 
                     << ", w1 = " << w1.get_value().get_average_int() 
                     << ", w2 = " << w2.get_value().get_average_int() 
                     << ", w3 = " << w3.get_value().get_average_int();
        }
        else {
            usleep(950);
        }
    }
}

const size_t OPS_PER_THREAD = 20000000;

static void *thread_counter(void *arg) {
    var::AverageRecorder *recorder = (var::AverageRecorder *)arg;
    var::Timer timer;
    timer.start();
    for (int i = 0; i < (int)OPS_PER_THREAD; ++i) {
        *recorder << i;
    }
    timer.stop();
    return (void *)(timer.n_elapsed());
}

TEST(RecorderTest, perf) {
    var::AverageRecorder recorder;
    ASSERT_TRUE(recorder.valid());
    pthread_t threads[8];
    for (size_t i = 0; i < 8; ++i) {
        pthread_create(&threads[i], NULL, &thread_counter, (void *)&recorder);
    }
    long totol_time = 0;
    for (size_t i = 0; i < 8; ++i) {
        void *ret; 
        pthread_join(threads[i], &ret);
        totol_time += (long)ret;
    }
    ASSERT_EQ(((int64_t)OPS_PER_THREAD - 1) / 2, recorder.average());
    LOG_INFO << "Recorder takes " << totol_time / (OPS_PER_THREAD * 8) 
             << "ns per sample with " << 8 
             << " threads";
}