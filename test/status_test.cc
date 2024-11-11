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

// Date Thur Sep 12 14:19:33 CST 2024.

#include <gtest/gtest.h>
#include "metric/status.h"

TEST(StatusTest, status)
{
    var::Status<std::string> st1;
    st1.set_value("hello %d", 9);
    ASSERT_EQ(0, st1.expose("var1"));
    ASSERT_EQ("hello 9", var::Variable::describe_exposed("var1"));
    ASSERT_EQ("\"hello 9\"", var::Variable::describe_exposed("var1", true));
    std::vector<std::string> vars;
    var::Variable::list_exposed(&vars);
    ASSERT_EQ(1UL, vars.size());
    ASSERT_EQ("var1", vars[0]);
    ASSERT_EQ(1UL, var::Variable::count_exposed());

    var::Status<std::string> st2;
    st2.set_value("world %d", 10);
    ASSERT_EQ(-1, st2.expose("var1"));
    ASSERT_EQ(1UL, var::Variable::count_exposed());
    ASSERT_EQ("world 10", st2.get_description());
    ASSERT_EQ("hello 9", var::Variable::describe_exposed("var1"));
    ASSERT_EQ(1UL, var::Variable::count_exposed());

    ASSERT_TRUE(st1.hide());
    ASSERT_EQ(0UL, var::Variable::count_exposed());
    ASSERT_EQ("", var::Variable::describe_exposed("var1"));
    ASSERT_EQ(0, st1.expose("var1"));
    ASSERT_EQ(1UL, var::Variable::count_exposed());
    ASSERT_EQ("hello 9",
              var::Variable::describe_exposed("var1"));
    
    ASSERT_EQ(0, st2.expose("var2"));
    ASSERT_EQ(2UL, var::Variable::count_exposed());
    ASSERT_EQ("hello 9", var::Variable::describe_exposed("var1"));
    ASSERT_EQ("world 10", var::Variable::describe_exposed("var2"));
    var::Variable::list_exposed(&vars);
    ASSERT_EQ(2UL, vars.size());
    ASSERT_EQ("var1", vars[0]);
    ASSERT_EQ("var2", vars[1]);

    ASSERT_TRUE(st2.hide());
    ASSERT_EQ(1UL, var::Variable::count_exposed());
    ASSERT_EQ("", var::Variable::describe_exposed("var2"));
    var::Variable::list_exposed(&vars);
    ASSERT_EQ(1UL, vars.size());
    ASSERT_EQ("var1", vars[0]);

    st2.expose("var2 again");
    ASSERT_EQ("world 10", var::Variable::describe_exposed("var2_again"));
    var::Variable::list_exposed(&vars);
    ASSERT_EQ(2UL, vars.size());
    ASSERT_EQ("var1", vars[0]);
    ASSERT_EQ("var2_again", vars[1]);
    ASSERT_EQ(2UL, var::Variable::count_exposed());        

    var::Status<std::string> st3("var3", "FooStatusbar");
    ASSERT_EQ("var3", st3.name());
    ASSERT_EQ(3UL, var::Variable::count_exposed());
    ASSERT_EQ("FooStatusbar", var::Variable::describe_exposed("var3"));
    var::Variable::list_exposed(&vars);
    ASSERT_EQ(3UL, vars.size());
    ASSERT_EQ("var1", vars[0]);
    ASSERT_EQ("var3", vars[1]);
    ASSERT_EQ("var2_again", vars[2]);
    ASSERT_EQ(3UL, var::Variable::count_exposed());

    var::Status<int> st4("var4", 9);
    ASSERT_EQ("var4", st4.name());
    ASSERT_EQ(4UL, var::Variable::count_exposed());
    ASSERT_EQ("9", var::Variable::describe_exposed("var4"));
    var::Variable::list_exposed(&vars);
    ASSERT_EQ(4UL, vars.size());
    ASSERT_EQ("var1", vars[0]);
    ASSERT_EQ("var3", vars[1]);
    ASSERT_EQ("var4", vars[2]);
    ASSERT_EQ("var2_again", vars[3]);

    var::Status<void*> st5((void*)19UL);
    ASSERT_EQ("0x13", st5.get_description());
}

struct FooStatus {
    int x;
    FooStatus() : x(0) {}
    explicit FooStatus(int x2) : x(x2) {}
    FooStatus& operator+=(const FooStatus& rhs) {
        this->x += rhs.x;
        return *this;
    }
};

std::ostream& operator<<(std::ostream& os, const FooStatus& f) {
    return os << "FooStatus{" << f.x << "}";
}

TEST(StatusTest, non_primitive) {
    var::Status<FooStatus> st;
    ASSERT_EQ(0, st.get_value().x);
    st.set_value(FooStatus(1));
    ASSERT_EQ(1, st.get_value().x);
}