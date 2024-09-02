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
    st1.set_value(9);
    ASSERT_TRUE(st1.is_hidden());
}