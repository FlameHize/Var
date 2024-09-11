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

// Date Wed Sep 11 14:52:23 CST 2024.

#include <gtest/gtest.h>
#include "src/window.h"
#include "src/reducer.h"

TEST(WindowTest, window)
{
    const int window_size = 2;

    var::Adder<int> adder;
    var::Window<var::Adder<int>> window_adder(&adder, window_size);
    
    var::Maxer<int> maxer;
    var::Window<var::Maxer<int>> window_maxer(&maxer, window_size);

    var::Miner<int> miner;
    var::Window<var::Miner<int>> window_miner(&miner, window_size);
    
    adder << 10;
    maxer << 10;
    miner << 10;

    sleep(1);
    adder << 2;
    maxer << 2;
    miner << 2;
    
    sleep(1);
    ASSERT_EQ(window_adder.get_value(), 12);
    ASSERT_EQ(window_maxer.get_value(), 10);
    ASSERT_EQ(window_miner.get_value(), 2);
}