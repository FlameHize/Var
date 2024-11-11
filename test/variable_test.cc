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
#include "metric/variable.h"

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

class MyDumper : public Dumper {
public:
    bool dump(const std::string& name, 
              const std::string& description) override {
        _list.push_back(std::make_pair(name, description));
        return true;
    }
    std::vector<std::pair<std::string, std::string>> _list;
};

TEST(VariableTest, dump)
{
    MyDumper dumper;

    // Nothing to dump yet.
    ASSERT_EQ(0, Variable::dump_exposed(&dumper, nullptr));
    ASSERT_TRUE(dumper._list.empty());

    Status<int> v1("var1", 1);
    Status<int> v1_2("var1", 12);
    Status<int> v2("var2", 2);
    Status<int> v3("foo.bar.Apple", "var3", 3);
    Status<int> v4("foo.bar.BaNaNa", "var4", 4);
    Status<int> v5("foo::bar::Car_Rot", "var5", 5);
    ASSERT_EQ(5, Variable::dump_exposed(&dumper, NULL));
    ASSERT_EQ(5UL, dumper._list.size());

    ASSERT_EQ("foo_bar_apple_var3", dumper._list[0].first);
    ASSERT_EQ("3", dumper._list[0].second);
    ASSERT_EQ("foo_bar_ba_na_na_var4", dumper._list[1].first);
    ASSERT_EQ("4", dumper._list[1].second);
    ASSERT_EQ("foo_bar_car_rot_var5", dumper._list[2].first);
    ASSERT_EQ("5", dumper._list[2].second);
    ASSERT_EQ("var1", dumper._list[3].first);
    ASSERT_EQ("1", dumper._list[3].second);
    ASSERT_EQ("var2", dumper._list[4].first);
    ASSERT_EQ("2", dumper._list[4].second);

    dumper._list.clear();
    DumpOptions opts;
    opts.white_wildcards = "foo_bar_*";
    opts.black_wildcards = "*var5";
    ASSERT_EQ(2, Variable::dump_exposed(&dumper, &opts));
    ASSERT_EQ(2UL, dumper._list.size());
    ASSERT_EQ("foo_bar_apple_var3", dumper._list[0].first);
    ASSERT_EQ("3", dumper._list[0].second);
    ASSERT_EQ("foo_bar_ba_na_na_var4", dumper._list[1].first);
    ASSERT_EQ("4", dumper._list[1].second);

    dumper._list.clear();
    opts = DumpOptions();
    opts.white_wildcards = "*?rot*";
    ASSERT_EQ(1, Variable::dump_exposed(&dumper, &opts));
    ASSERT_EQ(1UL, dumper._list.size());
    ASSERT_EQ("foo_bar_car_rot_var5", dumper._list[0].first);
    ASSERT_EQ("5", dumper._list[0].second);

    dumper._list.clear();
    opts = DumpOptions();
    opts.white_wildcards = "";
    opts.black_wildcards = "var1;var2";
    ASSERT_EQ(3, Variable::dump_exposed(&dumper, &opts));
    ASSERT_EQ(3UL, dumper._list.size());
    ASSERT_EQ("foo_bar_apple_var3", dumper._list[0].first);
    ASSERT_EQ("3", dumper._list[0].second);
    ASSERT_EQ("foo_bar_ba_na_na_var4", dumper._list[1].first);
    ASSERT_EQ("4", dumper._list[1].second);
    ASSERT_EQ("foo_bar_car_rot_var5", dumper._list[2].first);
    ASSERT_EQ("5", dumper._list[2].second);

    dumper._list.clear();
    opts = DumpOptions();
    opts.white_wildcards = "";
    opts.black_wildcards = "f?o_b?r_*;not_exist";
    ASSERT_EQ(2, Variable::dump_exposed(&dumper, &opts));
    ASSERT_EQ(2UL, dumper._list.size());
    ASSERT_EQ("var1", dumper._list[0].first);
    ASSERT_EQ("1", dumper._list[0].second);
    ASSERT_EQ("var2", dumper._list[1].first);
    ASSERT_EQ("2", dumper._list[1].second);

    dumper._list.clear();
    opts = DumpOptions();
    opts.question_mark = '$';
    opts.white_wildcards = "";
    opts.black_wildcards = "f$o_b$r_*;not_exist";
    ASSERT_EQ(2, Variable::dump_exposed(&dumper, &opts));
    ASSERT_EQ(2UL, dumper._list.size());
    ASSERT_EQ("var1", dumper._list[0].first);
    ASSERT_EQ("1", dumper._list[0].second);
    ASSERT_EQ("var2", dumper._list[1].first);
    ASSERT_EQ("2", dumper._list[1].second);

    dumper._list.clear();
    opts = DumpOptions();
    opts.white_wildcards = "not_exist";
    ASSERT_EQ(0, Variable::dump_exposed(&dumper, &opts));
    ASSERT_EQ(0UL, dumper._list.size());

    dumper._list.clear();
    opts = DumpOptions();
    opts.white_wildcards = "not_exist;f??o_bar*";
    ASSERT_EQ(0,Variable::dump_exposed(&dumper, &opts));
    ASSERT_EQ(0UL, dumper._list.size());
}