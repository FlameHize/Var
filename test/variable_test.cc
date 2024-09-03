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
#include "src/variable.h"

using namespace var;

template<typename T>
class Status : public Variable {
public:
    Status() {}
    Status(const T& value) : _value(value) {}
    Status(const std::string& name, const T& value) 
        : _value(value) {
        this->expose(name);
    }
    Status(const std::string& prefix,
           const std::string& name, const T& value)
        : _value(value) {
        this->expose_as(prefix, name);
    }
    ~Status() { hide(); }
    void describe(std::ostream& os, bool /*quote_string*/) const override {
        os << get_value();
    }
    T get_value() const { return _value; }
    void set_value(const T& value) { _value = value; }

private:
    T _value;
};

TEST(VariableTest, status)
{
    Status<int> st1;
    ASSERT_TRUE(st1.is_hidden());
    ASSERT_EQ(0UL, Variable::count_exposed());

    ASSERT_EQ(0, st1.expose("var1"));
    ASSERT_FALSE(st1.is_hidden());
    st1.set_value(9);
    ASSERT_EQ("9", Variable::describe_exposed("var1"));
    std::vector<std::string> vars;
    Variable::list_exposed(&vars);
    ASSERT_EQ(1UL, vars.size());
    ASSERT_EQ("var1", vars[0]);
    ASSERT_EQ(1UL, Variable::count_exposed());

    Status<int> st2;
    st2.set_value(10);
    ASSERT_TRUE(st2.is_hidden());
    ASSERT_EQ(-1, st2.expose("var1"));
    ASSERT_TRUE(st2.is_hidden());
    ASSERT_EQ(1UL, Variable::count_exposed());
    ASSERT_EQ("10", st2.get_description());
    ASSERT_EQ("9", Variable::describe_exposed("var1"));
    ASSERT_EQ(1UL, Variable::count_exposed());

    ASSERT_TRUE(st1.hide());
    ASSERT_TRUE(st1.is_hidden());
    ASSERT_EQ(0UL, Variable::count_exposed());
    ASSERT_EQ("", Variable::describe_exposed("var1"));
    ASSERT_EQ(0, st1.expose("var1"));
    ASSERT_EQ(1UL, Variable::count_exposed());
    ASSERT_EQ("9", Variable::describe_exposed("var1"));

    ASSERT_EQ(0, st2.expose("var2"));
    ASSERT_EQ(2UL, Variable::count_exposed());
    ASSERT_EQ("9", Variable::describe_exposed("var1"));
    ASSERT_EQ("10", Variable::describe_exposed("var2"));
    Variable::list_exposed(&vars);
    ASSERT_EQ(2UL, vars.size());
    ASSERT_EQ("var1", vars[0]);
    ASSERT_EQ("var2", vars[1]);

    ASSERT_TRUE(st2.hide());
    ASSERT_EQ(1UL, Variable::count_exposed());
    ASSERT_EQ("", Variable::describe_exposed("var2"));
    Variable::list_exposed(&vars);
    ASSERT_EQ(1UL, vars.size());
    ASSERT_EQ("var1", vars[0]);

    ASSERT_EQ(0, st2.expose("Var2 Again"));
    ASSERT_EQ("", Variable::describe_exposed("Var2 Again"));
    ASSERT_EQ("10", Variable::describe_exposed("var2_again"));
    Variable::list_exposed(&vars);
    ASSERT_EQ(2UL, vars.size());
    ASSERT_EQ("var1", vars[0]);
    ASSERT_EQ("var2_again", vars[1]);
    ASSERT_EQ(2UL, Variable::count_exposed());  

    Status<int> st3("var3", 11);
    ASSERT_EQ("var3", st3.name());
    ASSERT_EQ(3UL, Variable::count_exposed());
    ASSERT_EQ("11", Variable::describe_exposed("var3"));
    Variable::list_exposed(&vars);
    ASSERT_EQ(3UL, vars.size());
    ASSERT_EQ("var1", vars[0]);
    ASSERT_EQ("var3", vars[1]);
    ASSERT_EQ("var2_again", vars[2]);
    ASSERT_EQ(3UL, Variable::count_exposed()); 

    Status<int> st4("var4", 12);
    ASSERT_EQ("var4", st4.name());
    ASSERT_EQ(4UL, Variable::count_exposed());
    ASSERT_EQ("12", Variable::describe_exposed("var4"));
    Variable::list_exposed(&vars);
    ASSERT_EQ(4UL, vars.size());
    ASSERT_EQ("var1", vars[0]);
    ASSERT_EQ("var3", vars[1]);
    ASSERT_EQ("var4", vars[2]);
    ASSERT_EQ("var2_again", vars[3]);

    Status<void*> st5((void*)19UL);
    ASSERT_EQ("0x13", st5.get_description());
}

TEST(VariableTest, expose)
{
    Status<int> c1;
    ASSERT_EQ(0, c1.expose_as("foo::bar::Apple", "c1"));
    ASSERT_EQ("foo_bar_apple_c1", c1.name());
    ASSERT_EQ(1UL, Variable::count_exposed());

    ASSERT_EQ(0, c1.expose_as("foo.bar::BaNaNa", "c1"));
    ASSERT_EQ("foo_bar_ba_na_na_c1", c1.name());
    ASSERT_EQ(1UL, Variable::count_exposed());

    ASSERT_EQ(0, c1.expose_as("foo::bar.Car_Rot", "c1"));
    ASSERT_EQ("foo_bar_car_rot_c1", c1.name());
    ASSERT_EQ(1UL, Variable::count_exposed());

    ASSERT_EQ(0, c1.expose_as("foo-bar-RPCTest", "c1"));
    ASSERT_EQ("foo_bar_rpctest_c1", c1.name());
    ASSERT_EQ(1UL, Variable::count_exposed());

    ASSERT_EQ(0, c1.expose_as("foo-bar-HELLO", "c1"));
    ASSERT_EQ("foo_bar_hello_c1", c1.name());
    ASSERT_EQ(1UL, Variable::count_exposed());
    
    ASSERT_EQ(0, c1.expose("c1"));
    ASSERT_EQ("c1", c1.name());
    ASSERT_EQ(1UL, Variable::count_exposed());
}