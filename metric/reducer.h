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

// Date Fri Sep 06 11:28 CST 2024.

#ifndef VAR_REDUCER_H
#define VAR_REDUCER_H

#include "metric/variable.h"
#include "metric/detail/combiner.h"
#include "metric/detail/sampler.h"
#include "metric/detail/series.h"
#include "metric/util/type_traits.h"
// #include "metric/util/class_name.h"

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

template<typename T, typename Op, typename InvOp = detail::VoidOp>
class Reducer : public Variable {
public:
    typedef typename detail::AgentCombiner<T, T, Op> combine_type;
    typedef typename combine_type::Agent agent_type;
    typedef detail::ReducerSampler<Reducer, T, Op, InvOp> sampler_type;

    class SeriesSampler : public detail::Sampler {
    public:
        SeriesSampler(Reducer* owner, const Op& op)
            : _owner(owner), _series(op) {}
        ~SeriesSampler() {}
        void take_sample() override {
            _series.append(_owner->get_value());
        }
        void describe(std::ostream& os) {
            _series.describe(os, nullptr);
        }
    private:
        Reducer* _owner;
        detail::Series<T, Op> _series;
    };

    // The 'identify' must satisfy: identity Op a == a.
    Reducer(typename var::add_cr_non_integral<T>::type identity = T(),
            const Op& op = Op(),
            const InvOp& inv_op = InvOp())
        : _combiner(identity, identity, op)
        , _sampler(nullptr)
        , _series_sampler(nullptr)
        , _inv_op(inv_op) {
    }

    ~Reducer() {
        // Calling hide() manually is a MUST required by Variable.
        hide();
        if(_sampler) {
            _sampler->destroy();
            _sampler = nullptr;
        }
        if(_series_sampler) {
            _series_sampler->destroy();
            _series_sampler = nullptr;
        }
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
        // Sampler will reset var's which does not have inverse op.
        if(std::is_same<InvOp, detail::VoidOp>::value && _sampler != nullptr) {
            LOG_ERROR << "You should not call Reducer::get_value() when a Window<> "
                      << "is used because the operator does not have inverse op";
            // LOG_ERROR << "You should not call Reducer<" << var::class_name_str<T>()
            // << ", " << var::class_name_str<Op>() << ">::get_value() when a "
            // << "Window<> is used because the operator does not have inverse op.";
        }
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
    const InvOp& inv_op() const { return _inv_op; }

    sampler_type* get_sampler() {
        if(!_sampler) {
            _sampler = new sampler_type(this);
            _sampler->schedule();
        }
        return _sampler;
    }

    int describe_series(std::ostream& os) const override {
        if(!_series_sampler) {
            return 1;
        }
        _series_sampler->describe(os);
        return 0;
    }

protected:
    int expose_impl(const std::string& prefix,
                    const std::string& name,
                    DisplayFilter display_filter) override {
        const int rc = Variable::expose_impl(prefix, name, display_filter);
        ///@todo fix invop.
        if(rc == 0 &&
           _series_sampler == nullptr &&
           !std::is_same<T, std::string>::value)  {
           _series_sampler = new SeriesSampler(this, _combiner.op());
           _series_sampler->schedule(); 
        }
        return rc;
    }

private:
    combine_type    _combiner;
    sampler_type*   _sampler;
    SeriesSampler*  _series_sampler;
    InvOp           _inv_op;
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
class Adder : public Reducer<T, detail::AddTo<T>, detail::MinusFrom<T>> {
public:
    typedef Reducer<T, detail::AddTo<T>, detail::MinusFrom<T>> Base;
    typedef T value_type;
    typedef typename Base::sampler_type sampler_type;
    
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
    typedef typename Base::sampler_type sampler_type;

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
    typedef typename Base::sampler_type sampler_type;

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