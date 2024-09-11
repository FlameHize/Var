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

// Date Wed Sep 11 09:50:01 CST 2024.

#include <gtest/gtest.h>
#include "src/detail/series.h"
#include "net/Buffer.h"
#include "net/base/Logging.h"

template<typename T>
struct AddTo {
    void operator()(T& lhs, const T& rhs) const {
        lhs += rhs;
    }
};

template<typename T>
struct MaxTo {
    void operator()(T& lhs, const T& rhs) const {
        if(lhs < rhs) {
            lhs = rhs;
        }
    }
};

TEST(SeriesTest, append_within_addition)
{
    AddTo<int> add;
    var::detail::Series<int, AddTo<int>> series(add);
    EXPECT_EQ(series.data().second(0), 0);
    EXPECT_EQ(series.data().minute(0), 0);
    for(size_t i = 1; i <= 60; ++i) {
        series.append(1);
    }
    for(size_t i = 0; i < 60; ++i) {
        EXPECT_EQ(series.data().second(i), 1);
    }
    EXPECT_EQ(series.data().minute(0), 1);
    
    EXPECT_EQ(series.data().hour(0), 0);
    for(size_t i = 1; i <= 3600 - 60; ++i) {
        series.append(1);
    }
    for(size_t i = 0; i < 60; ++i) {
        EXPECT_EQ(series.data().minute(i), 1);
    }
    EXPECT_EQ(series.data().hour(0), 1);

    EXPECT_EQ(series.data().day(0), 0);
    for(size_t i = 1; i <= 24 * 3600 - 3600; ++i) {
        series.append(1);
    }
    for(size_t i = 0; i < 24; ++i) {
        EXPECT_EQ(series.data().hour(i), 1);
    }
    EXPECT_EQ(series.data().day(0), 1);

    var::net::BufferStream stream;
    series.describe(stream, nullptr);
    var::net::Buffer& buf = stream.buf();
    LOG_INFO << buf.retrieveAllAsString();
}

TEST(SeriesTest, append_without_addition)
{
    MaxTo<int> max;
    var::detail::Series<int, MaxTo<int>> series(max);
    EXPECT_EQ(series.data().second(0), 0);
    EXPECT_EQ(series.data().minute(0), 0);
    for(size_t i = 1; i <= 60; ++i) {
        series.append(1);
    }
    for(size_t i = 0; i < 60; ++i) {
        EXPECT_EQ(series.data().second(i), 1);
    }
    EXPECT_EQ(series.data().minute(0), 1);
    
    EXPECT_EQ(series.data().hour(0), 0);
    for(size_t i = 1; i <= 3600 - 60; ++i) {
        series.append(1);
    }
    for(size_t i = 0; i < 60; ++i) {
        EXPECT_EQ(series.data().minute(i), 1);
    }
    EXPECT_EQ(series.data().hour(0), 1);

    EXPECT_EQ(series.data().day(0), 0);
    for(size_t i = 1; i <= 24 * 3600 - 3600; ++i) {
        series.append(1);
    }
    for(size_t i = 0; i < 24; ++i) {
        EXPECT_EQ(series.data().hour(i), 1);
    }
    EXPECT_EQ(series.data().day(0), 1);

    var::net::BufferStream stream;
    series.describe(stream, nullptr);
    var::net::Buffer& buf = stream.buf();
    LOG_INFO << buf.retrieveAllAsString();
}