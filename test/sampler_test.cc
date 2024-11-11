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

// Date Tue Sep 10 11:22:36 CST 2024.

#include <gtest/gtest.h>
#include "metric/detail/sampler.h"
#include "metric/reducer.h"

TEST(SamplerTest, linked_list)
{
    var::LinkNode<var::detail::Sampler> n1, n2;
    n1.InsertBeforeAsList(&n2);
    ASSERT_EQ(n1.next(), &n2);
    ASSERT_EQ(n1.previous(), &n2);
    ASSERT_EQ(n2.next(), &n1);
    ASSERT_EQ(n2.previous(), &n1);

    var::LinkNode<var::detail::Sampler> n3, n4;
    n3.InsertBeforeAsList(&n4);
    ASSERT_EQ(n3.next(), &n4);
    ASSERT_EQ(n3.previous(), &n4);
    ASSERT_EQ(n4.next(), &n3);
    ASSERT_EQ(n4.previous(), &n3);

    n1.InsertBeforeAsList(&n3);
    ASSERT_EQ(n1.next(), &n2);
    ASSERT_EQ(n2.next(), &n3);
    ASSERT_EQ(n3.next(), &n4);
    ASSERT_EQ(n4.next(), &n1);
    ASSERT_EQ(&n1, n2.previous());
    ASSERT_EQ(&n2, n3.previous());
    ASSERT_EQ(&n3, n4.previous());
    ASSERT_EQ(&n4, n1.previous());
}

class DebugSampler : public var::detail::Sampler {
public:
    DebugSampler() : _ncalled(0) {}
    ~DebugSampler() {
        ++_s_ndestroy;
    }
    void take_sample() {
        ++_ncalled;
    }
    int called_count() const { return _ncalled; }

    int _ncalled;
    static int _s_ndestroy;
};
int DebugSampler::_s_ndestroy = 0;

TEST(SamplerTest, single_threaded) {
    const int N = 100;
    DebugSampler* s[N];
    for (int i = 0; i < N; ++i) {
        s[i] = new DebugSampler;
        s[i]->schedule();
    }
    usleep(1010000);
    for (int i = 0; i < N; ++i) {
        // LE: called once every second, may be called more than once
        ASSERT_LE(1, s[i]->called_count()) << "i=" << i;
    }
    EXPECT_EQ(0, DebugSampler::_s_ndestroy);
    for (int i = 0; i < N; ++i) {
        s[i]->destroy();
    }
    usleep(1010000);
    EXPECT_EQ(N, DebugSampler::_s_ndestroy);
}

static void* check(void*) {
    const int N = 100;
    DebugSampler* s[N];
    for (int i = 0; i < N; ++i) {
        s[i] = new DebugSampler;
        s[i]->schedule();
    }
    usleep(1010000);
    for (int i = 0; i < N; ++i) {
        EXPECT_LE(1, s[i]->called_count()) << "i=" << i;
    }
    for (int i = 0; i < N; ++i) {
        s[i]->destroy();
    }
    return NULL;
}

TEST(SamplerTest, multi_threaded) 
{
    pthread_t th[8];
    DebugSampler::_s_ndestroy = 0;
    for (size_t i = 0; i < 8; ++i) {
        ASSERT_EQ(0, pthread_create(&th[i], NULL, check, NULL));
    }
    for (size_t i = 0; i < 8; ++i) {
        ASSERT_EQ(0, pthread_join(th[i], NULL));
    }
    sleep(1);
    EXPECT_EQ(100 * 8, (size_t)DebugSampler::_s_ndestroy);
}

TEST(SamplerTest, reducer_sampler_within_invop)
{
    const time_t window_size = 3;
    var::Adder<int> adder;
    var::Adder<int>::sampler_type* sampler = adder.get_sampler();
    sampler->set_window_size(window_size);
    var::detail::Sample<int> result;
    EXPECT_EQ(false, sampler->get_value(window_size, &result));

    // wait for SamplerCollector take_sample() 1s once loop.
    adder << 5;
    sleep(1);   
    EXPECT_EQ(true, sampler->get_value(window_size, &result));
    EXPECT_EQ(5, result.data);
    
    adder << 4;
    sleep(1);
    EXPECT_EQ(true, sampler->get_value(window_size, &result));
    EXPECT_EQ(9, result.data);

    adder << 3;
    sleep(1);
    EXPECT_EQ(true, sampler->get_value(window_size, &result));
    EXPECT_EQ(12, result.data);

    adder << 2;
    sleep(1);
    EXPECT_EQ(true, sampler->get_value(window_size, &result));
    EXPECT_EQ(9, result.data);

    adder << 1;
    sleep(1);
    EXPECT_EQ(true, sampler->get_value(window_size, &result));
    EXPECT_EQ(6, result.data);

    std::vector<int> samples;
    sampler->get_samples(window_size, &samples);
    EXPECT_EQ(samples.size(), window_size);
    EXPECT_EQ(samples.at(0), 15);
    EXPECT_EQ(samples.at(1), 14);
    EXPECT_EQ(samples.at(2), 12);
}

TEST(SamplerTest, reducer_sampler_within_voidop)
{
    const time_t window_size = 3;
    var::Maxer<int> maxer;
    var::Maxer<int>::sampler_type* sampler = maxer.get_sampler();
    sampler->set_window_size(window_size);
    var::detail::Sample<int> result;
    EXPECT_EQ(false, sampler->get_value(window_size, &result));

    // wait for SamplerCollector take_sample() 1s once loop.
    maxer << 5;
    sleep(1);   
    EXPECT_EQ(true, sampler->get_value(window_size, &result));
    EXPECT_EQ(5, result.data);
    
    maxer << 4;
    sleep(1);
    EXPECT_EQ(true, sampler->get_value(window_size, &result));
    EXPECT_EQ(5, result.data);

    maxer << 3;
    sleep(1);
    EXPECT_EQ(true, sampler->get_value(window_size, &result));
    EXPECT_EQ(5, result.data);

    maxer << 2;
    sleep(1);
    EXPECT_EQ(true, sampler->get_value(window_size, &result));
    EXPECT_EQ(4, result.data);

    maxer << 1;
    sleep(1);
    EXPECT_EQ(true, sampler->get_value(window_size, &result));
    EXPECT_EQ(3, result.data);

    std::vector<int> samples;
    sampler->get_samples(window_size, &samples);
    EXPECT_EQ(samples.size(), window_size);
    EXPECT_EQ(samples.at(0), 1);
    EXPECT_EQ(samples.at(1), 2);
    EXPECT_EQ(samples.at(2), 3);
}

TEST(SamplerTest, window_degradation)
{
    const time_t window_size = 1;
    var::Adder<int> adder;
    var::Adder<int>::sampler_type* add_sampler = adder.get_sampler();
    add_sampler->set_window_size(window_size);
    var::detail::Sample<int> adder_result;
    EXPECT_EQ(false, add_sampler->get_value(window_size, &adder_result));

    adder << 3;
    sleep(1);
    EXPECT_EQ(true, add_sampler->get_value(window_size, &adder_result));
    EXPECT_EQ(3, adder_result.data);

    adder << 2;
    sleep(1);
    EXPECT_EQ(true, add_sampler->get_value(window_size, &adder_result));
    EXPECT_EQ(2, adder_result.data);

    adder << 1;
    sleep(1);
    EXPECT_EQ(true, add_sampler->get_value(window_size, &adder_result));
    EXPECT_EQ(1, adder_result.data);

    var::Maxer<int> maxer;
    var::Maxer<int>::sampler_type* max_sampler = maxer.get_sampler();
    max_sampler->set_window_size(window_size);
    var::detail::Sample<int> maxer_result;
    EXPECT_EQ(false, max_sampler->get_value(window_size, &maxer_result));

    maxer << 3;
    sleep(1);   
    EXPECT_EQ(true, max_sampler->get_value(window_size, &maxer_result));
    EXPECT_EQ(3, maxer_result.data);

    maxer << 2;
    sleep(1);   
    EXPECT_EQ(true, max_sampler->get_value(window_size, &maxer_result));
    EXPECT_EQ(2, maxer_result.data);

    maxer << 1;
    sleep(1);   
    EXPECT_EQ(true, max_sampler->get_value(window_size, &maxer_result));
    EXPECT_EQ(1, maxer_result.data);
}