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
    var::LatencyRecorder recorder(3);

    recorder << 5;
    sleep(1);

    recorder << 4;
    sleep(1);

    recorder << 3;
    sleep(1);

    EXPECT_EQ(recorder.latency(1), 3);
    EXPECT_EQ(recorder.latency(), 4);
    EXPECT_EQ(recorder.max_latency(), 5);

    recorder << 2;
    sleep(1);
    EXPECT_EQ(recorder.latency(1), 2);
    EXPECT_EQ(recorder.latency(), 3);
    EXPECT_EQ(recorder.max_latency(), 4);

    recorder << 1;
    sleep(1);
    EXPECT_EQ(recorder.latency(1), 1);
    EXPECT_EQ(recorder.latency(), 2);
    EXPECT_EQ(recorder.max_latency(), 3);
}