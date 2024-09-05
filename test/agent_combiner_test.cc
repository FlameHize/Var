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
    int global_value = 0;
    pod_container.merge_global([](int& value, int value2){
        value += value2;
    }, global_value);
    EXPECT_EQ(global_value, 25);
}