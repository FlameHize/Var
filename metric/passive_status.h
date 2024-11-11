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

// Date Thur Sep 12 16:11:20 CST 2024.

#ifndef VAR_PASSIVE_STATUS_H
#define VAR_PASSIVE_STATUS_H

#include "metric/variable.h"
#include "metric/reducer.h"

namespace var {

// Display a updated-by-need value. This is done by passing in an user callback
// which is called to produce the value.
// Example:
//   int print_number(void* arg) {
//      ...
//      return 5;
//   }
//
//   // number1 : 5
//   bvar::PassiveStatus<int> status1("number1", print_number, arg);
//
//   // foo_number2 : 5
//   bvar::PassiveStatus<int> status2("Foo", "number2", print_number, arg);
template<typename T>
class PassiveStatus : public Variable {
public:
    typedef T value_type;
    typedef detail::ReducerSampler<PassiveStatus, T, detail::AddTo<T>,
                                   detail::MinusFrom<T>> sampler_type;
    struct PlaceHolderOp {
        void operator()(T&, const T&) const {}
    };
    static const bool ADDITIVE = (std::is_integral<T>::value ||
                                  std::is_floating_point<T>::value ||
                                  is_vector<T>::value);
    
    // Less use vector.
    class SeriesSampler : public detail::Sampler {
    public:
        typedef typename var::conditional<ADDITIVE, detail::AddTo<T>, PlaceHolderOp>::type Op;
        explicit SeriesSampler(PassiveStatus* owner)
            : _owner(owner), _vector_names(nullptr), _series(Op()) {}
        ~SeriesSampler() {
            delete _vector_names;
            _vector_names = nullptr;
        }
        void take_sample() override {
            _series.append(_owner->get_value());
        }
        void describe(std::ostream& os) {
            _series.describe(os, _vector_names);
        }
        void set_vector_names(const std::string& names) {
            if(!_vector_names) {
                _vector_names = new std::string();
            }
            *_vector_names = names;
        }

    private:
        PassiveStatus*        _owner;
        std::string*          _vector_names;
        detail::Series<T, Op> _series;
    };

public:
    // NOTE: You must be very careful about lifetime of `arg' which should be
    // valid during lifetime of PassiveStatus.
    PassiveStatus(const std::string& name, T (*fn)(void*), void* arg)
        : _fn(fn)
        , _arg(arg)
        , _sampler(nullptr)
        , _series_sampler(nullptr) {
        expose(name);
    }

    PassiveStatus(const std::string& prefix,
                  const std::string& name,
                  T (*fn)(void*), void* arg)
        : _fn(fn)
        , _arg(arg)
        , _sampler(nullptr)
        , _series_sampler(nullptr) {
        expose_as(prefix, name);
    }

    PassiveStatus(T (*fn)(void*), void* arg) 
        : _fn(fn)
        , _arg(arg)
        , _sampler(nullptr)
        , _series_sampler(nullptr) {
    }

    ~PassiveStatus() {
        hide();
        if (_sampler) {
            _sampler->destroy();
            _sampler = nullptr;
        }
        if (_series_sampler) {
            _series_sampler->destroy();
            _series_sampler = nullptr;
        }
    }

    int set_vector_names(const std::string& names) {
        if (_series_sampler) {
            _series_sampler->set_vector_names(names);
            return 0;
        }
        return -1;
    }

    T get_value() const {
        return (_fn ? _fn(_arg) : T());
    }

    sampler_type* get_sampler() {
        if(!_sampler) {
            _sampler = new sampler_type(this);
            _sampler->schedule();
        }
        return _sampler;
    }

    detail::AddTo<T> op() const { return detail::AddTo<T>(); }
    detail::MinusFrom<T> inv_op() const { return detail::MinusFrom<T>(); }

    void describe(std::ostream& os, bool quote_string) const override {
        os << get_value();
    }

    int describe_series(std::ostream& os) const override {
        if(!_series_sampler) {
            return 1;
        }
        _series_sampler->describe(os);
        return 0;
    }

    // Adapt for Window->Reducer->reset().
    T reset() {
        LOG_ERROR << "PassiveStatus::reset() should never be called, abort";
        abort();
    }

protected:
    int expose_impl(const std::string& prefix,
                    const std::string& name,
                    DisplayFilter display_filter) override {
        const int rc = Variable::expose_impl(prefix, name, display_filter);
        if (ADDITIVE &&
            rc == 0 &&
            _series_sampler == nullptr) {
            _series_sampler = new SeriesSampler(this);
            _series_sampler->schedule();
        }
        return rc;
    }

private:
    T (*_fn)(void*);
    void* _arg;
    sampler_type* _sampler;
    SeriesSampler* _series_sampler;
};

// ccover g++ may complain about ADDITIVE is undefined unless it's
// explicitly declared here.

// Specialize std::string for using std::ostream& as a more friendly
// interface for user's callback.
template<>
class PassiveStatus<std::string> : public Variable {
public:
    PassiveStatus(const std::string& name,
                  void (*print)(std::ostream&, void*), void* arg)
        : _print(print), _arg(arg) {
        expose(name);
    }

    PassiveStatus(const std::string& prefix,
                  const std::string& name,
                  void (*print)(std::ostream&, void*), void* arg)
        : _print(print), _arg(arg) {
        expose_as(prefix, name);
    }

    PassiveStatus(void (*print)(std::ostream&, void*), void* arg) 
        : _print(print), _arg(arg) {}

    ~PassiveStatus() {
        hide();
    }

    void describe(std::ostream& os, bool quote_string) const override {
        if(quote_string) {
            if(_print) {
                os << '"';
                _print(os, _arg);
                os << '"';
            }
            else {
                os << "\"null\"";
            }
        }
        else {
            if(_print) {
                _print(os, _arg);
            }
            else {
                os << "null";
            }
        }
    }

private:
    void (*_print)(std::ostream&, void*);
    void* _arg;
};


} // end namespace var

#endif // VAR_PASSIVE_STATUS_H