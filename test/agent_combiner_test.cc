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
#include <thread>
#include "src/detail/combiner.h"

using namespace var;
using namespace var::detail;

TEST(AgentCombinerTest, element_container_pod)
{
    ElementContainer<int> pod_container;
    pod_container.store(10);
    int pod_value;
    pod_container.load(&pod_value);
    EXPECT_EQ(pod_value, 10);
    pod_container.exchange(&pod_value, 20);
    pod_container.load(&pod_value);
    EXPECT_EQ(pod_value, 20);
    pod_container.modify([](int& value, int value2){
        value += value2;
    }, 5);
    pod_container.load(&pod_value);
    EXPECT_EQ(pod_value, 25);
}

TEST(AgentCombinerTest, element_container_atomical)
{
    ASSERT_EQ(sizeof(int32_t), sizeof(ElementContainer<int32_t>));
    ASSERT_EQ(sizeof(int64_t), sizeof(ElementContainer<int64_t>));
    ASSERT_EQ(sizeof(float), sizeof(ElementContainer<float>));
    ASSERT_EQ(sizeof(double), sizeof(ElementContainer<double>));
}

template<typename T>
struct AddTo {
    void operator()(T& lhs, const T& rhs) const {
        lhs += rhs;
    }
};

template<typename T>
struct MinusFrom {
    void operator()(T& lhs, const T& rhs) const {
        lhs -= rhs;
    }
};

TEST(AgentCombinerTest, all_kinds_agent_group)
{
    // One kind of AgentCombiner has one kind AgentGroup
    AgentCombiner<int, int, AddTo<int>> addto_int_agent_combiner;
    AgentCombiner<int, int, AddTo<int>> addto_int_agent_combiner_2;
    AgentCombiner<int, int, MinusFrom<int>> minus_int_agent_combiner;
    AgentCombiner<double, double, MinusFrom<double>> minus_double_agent_combiner;
    EXPECT_EQ(addto_int_agent_combiner.id(), 0);
    EXPECT_EQ(addto_int_agent_combiner_2.id(), 1);
    EXPECT_EQ(minus_int_agent_combiner.id(),0);
    EXPECT_EQ(minus_double_agent_combiner.id(), 0);

    // int agent_id = 0;
    // std::thread thread([&agent_id](){
    //     AgentCombiner<int, int, AddTo<int>> addto_int_agent_combiner_3;
    //     agent_id = addto_int_agent_combiner_3.id();
    // });
    // EXPECT_EQ(agent_id, 2);
    // thread.join();
}

TEST(AgentCombinerTest, combine_and_merge)
{
    const size_t loop = 100;
    AgentCombiner<int, int, AddTo<int>> combiner;
    std::thread thread_1([&](){
        for(size_t i = 0; i < loop; ++i) {
            auto *agent = combiner.get_or_create_tls_agent();
            agent->element.modify(combiner.op(), 1);
        }
    });
    std::thread thread_2([&](){
        for(size_t i = 0; i < loop; ++i) {
            auto *agent = combiner.get_or_create_tls_agent();
            agent->element.modify(combiner.op(), 1);
        }
    });
    thread_1.join();
    thread_2.join();
    EXPECT_EQ(combiner.combine_agents(), 2 * loop);
    EXPECT_EQ(combiner.reset_all_agents(), 2  * loop);
    EXPECT_EQ(combiner.combine_agents(), 0);
}