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

// Date 2024 Sep 06 11:28 Fri Author zgx.

#ifndef VAR_REDUCER_H
#define VAR_REDUCER_H

#include "src/variable.h"
#include "src/detail/combiner.h"
#include "src/util/type_traits.h"

namespace var {
// Reduce multiple values into one with `Op': e1 Op e2 Op e3 ...
// `Op' shall satisfy:
//   - associative:     a Op (b Op c) == (a Op b) Op c
//   - commutative:     a Op b == b Op a;
//   - no side effects: a Op b never changes if a and b are fixed. 
// otherwise the result is undefined.
//
// For performance issues, we don't let Op return value, instead it shall
// set the result to the first parameter in-place. Namely to add two values,
// "+=" should be implemented rather than "+".
//
// Reducer works for non-primitive T which satisfies:
//   - T() should be the identity of Op.
//   - stream << v should compile and put description of v into the stream
// Example:
// class MyType {
// friend std::ostream& operator<<(std::ostream& os, const MyType&);
// public:
//     MyType() : _x(0) {}
//     explicit MyType(int x) : _x(x) {}
//     void operator+=(const MyType& rhs) const {
//         _x += rhs._x;
//     }
// private:
//     int _x;
// };
// std::ostream& operator<<(std::ostream& os, const MyType& value) {
//     return os << "MyType{" << value._x << "}";
// }
// bvar::Adder<MyType> my_type_sum;
// my_type_sum << MyType(1) << MyType(2) << MyType(3);
// LOG(INFO) << my_type_sum;  // "MyType{6}"

template<typename T, typename Op>
class Reducer : public Variable {
public:
    typedef typename detail::AgentCombiner<T, T, Op> combine_type;
    typedef typename combine_type::Agent agent_type;

    // The 'identify' must satisfy: identity Op a == a.
    Reducer(typename var::add_cr_non_integral<T>::type identity = T(),
            const Op& op = Op())
        : _combiner(identity, identity, op) {
    }

    ~Reducer() {
        // Calling hide() manually is a MUST required by Variable.
        hide();
    }

    // Add a value.
    // Returns self reference for chaining.
    // It's wait-free for most time.
    Reducer& operator<<(typename var::add_cr_non_integral<T>::type value) {
        agent_type* agent = _combiner.get_or_create_tls_agent();
        if(VAR_UNLIKELY(!agent)) {
            LOG_ERROR << "Fail to create agent";
            return *this;
        }
        agent->element.modify(_combiner.op(), value);
        return *this;
    }

    // Get reducerd value.
    // Notice that this function walks through all threads ever add values
    // into this reducer. You should avoid calling it frequently.
    T get_value() const {
        return _combiner.combine_agents();
    }

    // Reset the reduced value to T().
    // Returns the reduced value before reset.
    T reset() {
        return _combiner.reset_all_agents();
    }

    void describe(std::ostream& os, bool quote_string) const override {
        if(std::is_same<T, std::string>::value && quote_string) {
            os << '"' << get_value() << '"';
        }
        else {
            // NOTE: T MUST have operator<<() func.
            os << get_value();
        }
    }

    // True if this reducer is constructed successfully.
    bool valid() const { return _combiner.vaild(); }

    // Get instance of Op.
    const Op& op() const { return _combiner.op(); }

protected:
    int expose_impl(const std::string& prefix,
                    const std::string& name,
                    DisplayFilter display_filter) override {
        const int rc = Variable::expose_impl(prefix, name, display_filter);
        return rc;
    }

private:
    combine_type _combiner;
};

// =================== Common reducers ===================

// var::Adder<int> sum;
// sum << 1 << 2 << 3 << 4;
// LOG(INFO) << sum.get_value(); // 10
// Commonly used functors.
namespace detail {
template<typename T>
struct AddTo {
    void operator()(T& lhs, typename var::add_cr_non_integral<T>::type rhs) const {
        lhs += rhs;
    }
};
template<typename T>
struct MinusFrom {
    void operator()(T& lhs, typename var::add_cr_non_integral<T>::type rhs) const {
        lhs -= rhs;
    }
};
} // end namespace detail

template<typename T>
class Adder : public Reducer<T, detail::AddTo<T>> {
public:
    typedef Reducer<T, detail::AddTo<T>> Base;
    typedef T value_type;
    
    Adder() : Base() {}
    explicit Adder(const std::string& name) : Base() {
        this->expose(name);
    }
    explicit Adder(const std::string& prefix,
                   const std::string& name) : Base() {
        this->expose_as(prefix, name);
    }
    ~Adder() {
        Variable::hide();
    }
};

namespace detail {
template<typename T>
struct MaxTo {
    void operator()(T& lhs, typename var::add_cr_non_integral<T>::type rhs) const {
        if(lhs < rhs) { lhs = rhs; }
    }
};
template<typename T>
struct MinTo {
    void operator()(T& lhs, typename var::add_cr_non_integral<T>::type rhs) const {
        if(rhs < lhs) { lhs = rhs; }
    }
};
} // end namespace detail

template<typename T>
class Maxer : public Reducer<T, detail::MaxTo<T>> {
public:
    typedef Reducer<T, detail::MaxTo<T>> Base;
    typedef T value_type;

    Maxer() : Base(std::numeric_limits<T>::min()) {}
    explicit Maxer(const std::string& name) 
        : Base(std::numeric_limits<T>::min()) {
        this->expose(name);
    }
    explicit Maxer(const std::string& prefix,
                   const std::string& name) 
        : Base(std::numeric_limits<T>::min()) {
        this->expose_as(prefix, name);
    }
    ~Maxer() {
        Variable::hide();
    }
};

template<typename T>
class Miner : public Reducer<T, detail::MinTo<T>> {
public:
    typedef Reducer<T, detail::MinTo<T>> Base;
    typedef T value_type;

    Miner() : Base(std::numeric_limits<T>::max()) {}
    explicit Miner(const std::string name)
        : Base(std::numeric_limits<T>::max()) {
        this->expose(name);
    }
    explicit Miner(const std::string& prefix,
                   const std::string& name) 
        : Base(std::numeric_limits<T>::max()) {
        this->expose_as(prefix, name);
    }
    ~Miner() {
        Variable::hide();
    }
};

} // end namespace var

#endif // VAR_REDUCER_H