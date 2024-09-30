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
#include "src/window.h"
#include <fstream>

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
        // b2 added probability is N2/(N1 + N2)
        // b1 remain probability is 1 - N2/(N1 + N2)
        // b1 / b2 = N1 / N2.
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

TEST(PercentileTest, combine_of)
{
    // Combine multiple percentile samplers into one.
    const int num_samplers = 10;
    // Define a base time to make all samples are in the same interval.
    const uint32_t base = (1 << 30) + 1;

    const int N = 1000;
    size_t belong[num_samplers] = {0};
    size_t total = 0;
    for(int repeat = 0; repeat < 100; ++repeat) {
        var::detail::Percentile p[num_samplers];
        for(int i = 0; i < num_samplers; ++i) {
            // Set base = 0.
            // p[0] [0 * 1000, 0 * 1000 + 1000 * 1)
            // p[1] [1 * 1000, 1 * 1000 + 1000 * 2)
            // p[2] [3 * 1000, 3 * 1000 + 1000 * 3)
            // p[3] [6 * 1000, 6 * 1000 + 1000 * 4)
            // ...
            // p[9] [45 * 1000, 45 * 1000 + 1000 * 10]
            // N * (i + 1) is the input count of each p[i].
            // Expect ratio is 1:2:3:...:10
            for(int j = 0; j < N * (i + 1); ++j) {
                p[i] << base + i * (i + 1) / 2 * N + j;
            }
        }
        std::vector<var::detail::GlobalPercentileSamples> result;
        result.reserve(num_samplers);
        for(int i = 0; i < num_samplers; ++i) {
            result.push_back(p[i].get_value());
        }
        var::detail::PercentileSamples<510>g;
        g.combine_of(result.begin(), result.end());
        for(size_t i = 0; i < var::detail::NUM_INTERVALS; ++i) {
            if(!g.get_interval_at(i).added_count()) {
                continue;
            }
            var::detail::PercentileInterval<510>& p = g.get_interval_at(i);
            total += p.sample_count();
            for(size_t j = 0; j < p.sample_count(); ++j) {
                for(int k = 0; k < num_samplers; ++k) {
                    if((p.get_sample_at(j) - base) / N < (k + 1) * (k + 2) / 2u) {
                        belong[k]++;
                        break;
                    }
                }
            }
        }
        for(int i = 0; i < num_samplers; ++i) {
            double expect_ratio = (double)(i + 1) * 2 /
                                    (num_samplers * (num_samplers + 1));
            double actual_ratio = (double)(belong[i]) / total;
            EXPECT_LT(fabs(expect_ratio / actual_ratio) - 1, 0.2)
                << "expect_ratio=" << expect_ratio
                << " actual_ratio=" << actual_ratio;
        }
    }
}

TEST(PercentileTest, add)
{
    var::detail::Percentile p;
    for (int j = 0; j < 10; ++j) {
        for (int i = 0; i < 10000; ++i) {
            p << (i + 1);
        }
        var::detail::GlobalPercentileSamples b = p.reset();
        uint32_t last_value = 0;
        for (uint32_t k = 1; k <= 10u; ++k) {
            uint32_t value = b.get_number(k / 10.0);
            EXPECT_GE(value, last_value);
            last_value = value;
            EXPECT_GT(value, (k * 1000 - 500)) << "k=" << k;
            EXPECT_LT(value, (k * 1000 + 500)) << "k=" << k;
        }
        LOG_INFO << "99%: " << b.get_number(0.99) << ' '
                 << "99.9%: " << b.get_number(0.999) << ' '
                 << "99.99%: " << b.get_number(0.9999);

        std::ofstream out("out.txt");
        b.describe(out);
    }
}