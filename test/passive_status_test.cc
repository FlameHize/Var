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

// Date Thur Sep 12 16:57:00 CST 2024.

#include <gtest/gtest.h>
#include "metric/passive_status.h"
#include "net/base/Logging.h"
#include "net/Buffer.h"

void print1(std::ostream& os, void* arg) {
    os << arg;
}

int64_t print2(void* arg) {
    return *(int64_t*)arg;
}

TEST(PassiveStatusTest, passive_status) 
{
    var::PassiveStatus<std::string> st1("var11", print1, (void*)9UL);
    var::net::BufferStream stream;
    st1.describe(stream, false);
    ASSERT_EQ("0x9", stream.buf().retrieveAllAsString());

    std::ostringstream ss;
    ASSERT_EQ(0, var::Variable::describe_exposed("var11", ss));
    ASSERT_EQ("0x9", ss.str());
    std::vector<std::string> vars;
    var::Variable::list_exposed(&vars);
    ASSERT_EQ(1UL, vars.size());
    ASSERT_EQ("var11", vars[0]);
    ASSERT_EQ(1UL, var::Variable::count_exposed());

    int64_t tmp2 = 9;
    var::PassiveStatus<int64_t> st2("var12", print2, &tmp2);
    ss.str("");
    ASSERT_EQ(0, var::Variable::describe_exposed("var12", ss));
    ASSERT_EQ("9", ss.str());
    var::Variable::list_exposed(&vars);
    ASSERT_EQ(2UL, vars.size());
    ASSERT_EQ("var11", vars[0]);
    ASSERT_EQ("var12", vars[1]);
    ASSERT_EQ(2UL, var::Variable::count_exposed());
}