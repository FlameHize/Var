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

// Date Thur Sep 26 16:02:05 CST 2024.

#include <gtest/gtest.h>
#include "src/detail/percentile.h"

// Merge 2 PercentileIntervals b1 and b2. b2 has double SAMPLE_SIZE
// and num_added. Remaining samples of b1 and b2 in merged result should
// be 1:2 approximately.
TEST(PercentileTest, merge1)
{
    const size_t SAMPLE_SIZE = 32;
    const size_t N = 1000;
    size_t belong_to_b1 = 0;
    size_t belong_to_b2 = 0;

    for(int repeat = 0; repeat < 100; ++repeat) {
        var::detail::PercentileInterval<SAMPLE_SIZE * 3> b0;
        var::detail::PercentileInterval<SAMPLE_SIZE> b1;
        for(size_t i = 0; i < N; ++i) {
            if(b1.full()) {
                b0.merge(b1);
                b1.clear();
            }
            ASSERT_TRUE(b1.add32(i));
        }
        b0.merge(b1);
        var::detail::PercentileInterval<SAMPLE_SIZE * 2> b2;
        for (size_t i = 0; i < N * 2; ++i) {
            if (b2.full()) {
                b0.merge(b2);
                b2.clear();
            }
            ASSERT_TRUE(b2.add32(i + N));
        }
        b0.merge(b2);
        for(size_t i = 0; i < b0.sample_count(); ++i) {
            if(b0.get_sample_at(i) < N) {
                ++belong_to_b1;
            }
            else {
                ++belong_to_b2;
            }
        }
    }
    // EXPECT_EQ(belong_to_b1 * 2, belong_to_b2);
    EXPECT_LT(fabs(belong_to_b1 / (double)belong_to_b2 - 0.5),
              0.1) << "belong_to_b1=" << belong_to_b1
                   << " belong_to_b2=" << belong_to_b2;
}

// Merge 2 PercentileIntervals b1 and b2 with same SAMPLE_SIZE. Add N1
// samples to b1 and add N2 samples to b2, Remaining samples of b1 and
// b2 in merged result should be N1:N2 approximately.
TEST(PercentileTest, merge2)
{
    const size_t N1 = 1000;
    const size_t N2 = 400;
    size_t belong_to_b1 = 0;
    size_t belong_to_b2 = 0;
    for(int repeat = 0;  repeat < 100; ++repeat) {
        var::detail::PercentileInterval<64> b0;
        var::detail::PercentileInterval<64> b1;
        for(size_t i = 0; i < N1; ++i) {
            if(b1.full()) {
                b0.merge(b1);
                b1.clear();
            }
            ASSERT_TRUE(b1.add32(i));
        }
        b0.merge(b1);
        var::detail::PercentileInterval<64> b2;
        for(size_t i = 0; i < N2; ++i) {
            if(b2.full()) {
                b0.merge(b2);
                b2.clear();
            }
            ASSERT_TRUE(b2.add32(i + N1));
        }
        b0.merge(b2);
        for(size_t i = 0; i < b0.sample_count(); ++i) {
            if(b0.get_sample_at(i) < N1) {
                ++belong_to_b1;
            }
            else {
                ++belong_to_b2;
            }
        }
    }
    // EXPECT_EQ(belong_to_b1 / (double)belong_to_b2, N1 / (double)N2);
    EXPECT_LT(fabs(belong_to_b1 / (double)belong_to_b2 - N1 / (double)N2),
              0.1) << "belong_to_b1=" << belong_to_b1
                   << " belong_to_b2=" << belong_to_b2;
}