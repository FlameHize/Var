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

#include <gtest/gtest.h>
#include "src/reducer.h"
#include "src/util/time.h"
#include <unordered_map>

TEST(ReducerTest, adder) 
{
    var::Adder<int> reducer1;
    ASSERT_TRUE(reducer1.valid());
    reducer1 << 2 << 4;
    ASSERT_EQ(6, reducer1.get_value());

    var::Adder<double> reducer2;
    ASSERT_TRUE(reducer2.valid());
    reducer2 << 2.5 << 3.5;
    ASSERT_EQ(6.0, reducer2.get_value());

    var::Adder<int> reducer3;
    ASSERT_TRUE(reducer3.valid());
    reducer3 << -9 << 1 << 0 << 3;
    ASSERT_EQ(-5, reducer3.get_value());
}

const size_t OPS_PER_THREAD = 500000;

void* thread_counter(void* arg) {
    var::Adder<uint64_t>* reducer = (var::Adder<uint64_t>*)arg;
    var::Timer timer;
    timer.start();
    for(size_t i = 0; i < OPS_PER_THREAD; ++i) {
        (*reducer) << 2;
    }
    timer.stop();
    return (void*)(timer.n_elapsed());
}

void* add_atomic(void* arg) {
    std::atomic<uint64_t>* counter = (std::atomic<uint64_t>*)arg;
    var::Timer timer;
    timer.start();
    for(size_t i = 0; i < OPS_PER_THREAD; ++i) {
        counter->fetch_add(2, std::memory_order_relaxed);
    }
    timer.stop();
    return (void*)(timer.n_elapsed());
}

static long start_perf_test_with_atomic(size_t num_thread) {
    std::atomic<uint64_t> counter(0);
    pthread_t threads[num_thread];
    for (size_t i = 0; i < num_thread; ++i) {
        pthread_create(&threads[i], NULL, &add_atomic, (void *)&counter);
    }
    long totol_time = 0;
    for (size_t i = 0; i < num_thread; ++i) {
        void *ret; 
        pthread_join(threads[i], &ret);
        totol_time += (long)ret;
    }
    long avg_time = totol_time / (OPS_PER_THREAD * num_thread);
    EXPECT_EQ(2ul * num_thread * OPS_PER_THREAD, counter.load());
    return avg_time;
}

static long start_perf_test_with_adder(size_t num_thread) {
    var::Adder<uint64_t> reducer;
    EXPECT_TRUE(reducer.valid());
    pthread_t threads[num_thread];
    for (size_t i = 0; i < num_thread; ++i) {
        pthread_create(&threads[i], NULL, &thread_counter, (void *)&reducer);
    }
    long totol_time = 0;
    for (size_t i = 0; i < num_thread; ++i) {
        void *ret = NULL; 
        pthread_join(threads[i], &ret);
        totol_time += (long)ret;
    }
    long avg_time = totol_time / (OPS_PER_THREAD * num_thread);
    EXPECT_EQ(2ul * num_thread * OPS_PER_THREAD, reducer.get_value());
    return avg_time;
}

TEST(ReducerTest, perf) {
    std::ostringstream oss;
    for (size_t i = 1; i <= 8; ++i) {
        oss << i << '\t' << start_perf_test_with_adder(i) << '\n';
    }
    std::cout << "Adder performance:\n" << oss.str() << std::endl;
    oss.str("");
    for (size_t i = 1; i <= 8; ++i) {
        oss << i << '\t' << start_perf_test_with_atomic(i) << '\n';
    }
    std::cout << "Atomic performance:\n" << oss.str() << std::endl;
}

TEST(ReducerTest, min) 
{
    var::Miner<uint64_t> reducer;
    ASSERT_TRUE(reducer.valid());
    ASSERT_EQ(std::numeric_limits<uint64_t>::max(), reducer.get_value());
    reducer << 10 << 20;
    ASSERT_EQ(10ul, reducer.get_value());
    reducer << 5;
    ASSERT_EQ(5ul, reducer.get_value());
    reducer << std::numeric_limits<uint64_t>::max();
    ASSERT_EQ(5ul, reducer.get_value());
    reducer << 0;
    ASSERT_EQ(0ul, reducer.get_value());

    var::Miner<int> reducer2;
    ASSERT_EQ(std::numeric_limits<int>::max(), reducer2.get_value());
    reducer2 << 10 << 20;
    ASSERT_EQ(10, reducer2.get_value());
    reducer2 << -5;
    ASSERT_EQ(-5, reducer2.get_value());
    reducer2 << std::numeric_limits<int>::max();
    ASSERT_EQ(-5, reducer2.get_value());
    reducer2 << 0;
    ASSERT_EQ(-5, reducer2.get_value());
    reducer2 << std::numeric_limits<int>::min();
    ASSERT_EQ(std::numeric_limits<int>::min(), reducer2.get_value());
}

TEST(ReducerTest, max) 
{
    var::Maxer<uint64_t> reducer;
    ASSERT_TRUE(reducer.valid());
    ASSERT_EQ(std::numeric_limits<uint64_t>::min(), reducer.get_value());
    reducer << 10 << 20;
    ASSERT_EQ(20ul, reducer.get_value());
    reducer << 30;
    ASSERT_EQ(30ul, reducer.get_value());
    reducer << 0;
    ASSERT_EQ(30ul, reducer.get_value());

    var::Maxer<int> reducer2;
    ASSERT_EQ(std::numeric_limits<int>::min(), reducer2.get_value());
    ASSERT_TRUE(reducer2.valid());
    reducer2 << 20 << 10;
    ASSERT_EQ(20, reducer2.get_value());
    reducer2 << 30;
    ASSERT_EQ(30, reducer2.get_value());
    reducer2 << 0;
    ASSERT_EQ(30, reducer2.get_value());
    reducer2 << std::numeric_limits<int>::max();
    ASSERT_EQ(std::numeric_limits<int>::max(), reducer2.get_value());
}

var::Adder<int> global_adder;
TEST(ReducerTest, global)
{
    ASSERT_TRUE(global_adder.valid());
    global_adder.get_value();
}

struct Foo {
    int x;
    Foo() : x(0) {}
    explicit Foo(int x2) : x(x2) {}
    void operator+=(const Foo& rhs) {
        x += rhs.x;
    }
};

std::ostream& operator<<(std::ostream& os, const Foo& f) {
    return os << "Foo{" << f.x << "}";
}

TEST(ReducerTest, non_primitive) {
    var::Adder<Foo> adder;
    adder << Foo(2) << Foo(3) << Foo(4);
    ASSERT_EQ(9, adder.get_value().x);
}

bool g_stop = false;
struct StringAppenderResult {
    int count;
};

static void* string_appender(void* arg) {
    var::Adder<std::string>* cater = (var::Adder<std::string>*)arg;
    int count = 0;
    std::string id = std::to_string(pthread_self());
    std::string tmp = "a";
    for (count = 0; !count || !g_stop; ++count) {
        *cater << id << ":";
        for (char c = 'a'; c <= 'z'; ++c) {
            tmp[0] = c;
            *cater << tmp;
        }
        *cater << ".";
    }
    StringAppenderResult* res = new StringAppenderResult;
    res->count = count;
    std::cout << "Appended " << count << std::endl;
    return res;
}

TEST(ReducerTest, non_primitive_mt) {
    var::Adder<std::string> cater;
    pthread_t th[8];
    g_stop = false;
    for (size_t i = 0; i < 8; ++i) {
        pthread_create(&th[i], NULL, string_appender, &cater);
    }
    usleep(50000);
    g_stop = true;
    std::unordered_map<pthread_t, int> appended_count;
    for (size_t i = 0; i < 8; ++i) {
        StringAppenderResult* res = NULL;
        pthread_join(th[i], (void**)&res);
        appended_count[th[i]] = res->count;
        delete res;
    }
    std::unordered_map<pthread_t, int> got_count;
    std::string res = cater.get_value();
    for (var::StringSplitter sp(res.c_str(), '.'); sp; ++sp) {
        char* endptr = NULL;
        ++got_count[(pthread_t)strtoll(sp.field(), &endptr, 10)];
        ASSERT_EQ(27LL, sp.field() + sp.length() - endptr)
            << std::string(sp.field(), sp.length());
        ASSERT_EQ(0, memcmp(":abcdefghijklmnopqrstuvwxyz", endptr, 27));
    }
    ASSERT_EQ(appended_count.size(), got_count.size());
    for (size_t i = 0; i < 8; ++i) {
        ASSERT_EQ(appended_count[th[i]], got_count[th[i]]);
    }
}