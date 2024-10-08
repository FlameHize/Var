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

// Date Thur Sep 12 09:31:20 CST 2024.

#ifndef VAR_STATUS_H
#define VAR_STATUS_H

#include "src/variable.h"
#include "src/reducer.h"
#include "src/util/type_traits.h"
#include "net/base/StringPrintf.h"
#include <string>
#include <atomic>
#include <mutex>

namespace var {

// Display a rarely or periodically updated value.
// Usage:
//   bvar::Status<int> foo_count1(17);
//   foo_count1.expose("my_value");
//
//   bvar::Status<int> foo_count2;
//   foo_count2.set_value(17);
//   
//   bvar::Status<int> foo_count3("my_value", 17);

template<typename T, typename Enabler = void>
class Status : public Variable {
public:
    struct PlaceHolderOp {
        void operator()(T&, const T&) const {}
    };
    class SeriesSampler : public detail::Sampler {
    public:
        typedef typename var::conditional<
        !std::is_pointer<T>::value, detail::AddTo<T>, PlaceHolderOp>::type Op;
        explicit SeriesSampler(Status* owner)
            : _owner(owner), _series(Op()) {}
        void take_sample() override {
            _series.append(_owner->get_value());
        }
        void describe(std::ostream& os) {
            _series.describe(os, nullptr);
        }
    private:
        Status* _owner;
        detail::Series<T, Op> _series;
    };
public:
    Status() : _series_sampler(nullptr) {}
    Status(const T& value) : _value(value), _series_sampler(nullptr) {}
    Status(const std::string& name, const T& value) 
        : _value(value), _series_sampler(nullptr) {
        this->expose(name);
    }
    Status(const std::string& prefix, const std::string& name,
           const T& value) : _value(value), _series_sampler(nullptr) {
        this->expose_as(prefix, name);
    }
    ~Status() { 
        hide();
        if(_series_sampler) {
            _series_sampler->destroy();
            _series_sampler = nullptr;
        } 
    }

    T get_value() const {
        std::lock_guard guard(_lock);
        const T res = _value;
        return res;
    }

    void set_value(const T& value) {
        std::lock_guard guard(_lock);
        _value = value;
    }

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

protected:
    int expose_impl(const std::string& prefix, const std::string& name,
                    DisplayFilter display_filter) override {
        const int rc = Variable::expose_impl(prefix, name, display_filter);
        if(rc == 0 && _series_sampler == nullptr) {
            _series_sampler = new SeriesSampler(this);
            _series_sampler->schedule();
        }
        return rc;
    }

private:
    T _value;
    SeriesSampler* _series_sampler;
    // We use lock rather than std::atomic for generic values because
    // std::atomic requires the type to be memcpy-able (POD basically)
    mutable std::mutex _lock;
};

template<typename T>
class Status<T, typename std::enable_if<
    std::is_integral<T>::value || std::is_floating_point<T>::value>::type> 
    : public Variable {
public:
    struct PlaceHolderOp {
        void operator()(T&, const T&) const {}
    };
    class SeriesSampler : public detail::Sampler {
    public:
        typedef typename var::conditional<
        true, detail::AddTo<T>, PlaceHolderOp>::type Op;
        explicit SeriesSampler(Status* owner)
            : _owner(owner), _series(Op()) {}
        void take_sample() override {
            _series.append(_owner->get_value());
        }
        void describe(std::ostream& os) {
            _series.describe(os, nullptr);
        }
    private:
        Status* _owner;
        detail::Series<T, Op> _series;
    };

public:
    Status() : _series_sampler(nullptr) {}
    Status(const T& value) : _value(value), _series_sampler(nullptr) {}
    Status(const std::string& name, const T& value)
        : _value(value), _series_sampler(nullptr) {
        this->expose(name);
    }
    Status(const std::string& prefix, const std::string& name,
           const T& value) : _value(value), _series_sampler(nullptr) {
        this->expose_as(prefix, name);
    }
    ~Status() {
        hide();
        if(_series_sampler) {
            _series_sampler->destroy();
            _series_sampler = nullptr;
        }
    }

    T get_value() const {
        return _value.load(std::memory_order_relaxed);
    }

    void set_value(const T& value) {
        _value.store(value, std::memory_order_relaxed);
    }

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

protected:
    int expose_impl(const std::string& prefix, const std::string& name,
                    DisplayFilter display_filter) override {
        const int rc = Variable::expose_impl(prefix, name, display_filter);
        if(rc == 0 && _series_sampler == nullptr) {
            _series_sampler = new SeriesSampler(this);
            _series_sampler->schedule();
        }
        return rc;
    }

private:
    std::atomic<T>  _value;
    SeriesSampler*  _series_sampler;
};

// Specialize for std::string, adding a printf-stype set_value().
template<>
class Status<std::string, void> : public Variable {
public:
    Status() {}
    Status(const std::string& name, const char* fmt, ...) {
        if(fmt) {
            va_list ap;
            va_start(ap, fmt);
            string_vprintf(&_value, fmt, ap);
            va_end(ap);
        }
        expose(name);
    } 
    Status(const std::string& prefix,
           const std::string& name, const char* fmt, ...) {
        if (fmt) {
            va_list ap;
            va_start(ap, fmt);
            string_vprintf(&_value, fmt, ap);
            va_end(ap);
        }
        expose_as(prefix, name);
    }

    ~Status() { hide(); }

    void describe(std::ostream& os, bool quote_string) const override {
        if (quote_string) {
            os << '"' << get_value() << '"';
        } else {
            os << get_value();
        }
    }

    std::string get_value() const {
        std::lock_guard guard(_lock);
        return _value;
    }

    void set_value(const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        {
            std::lock_guard guard(_lock);
            string_vprintf(&_value, fmt, ap);
        }
        va_end(ap);
    }

    void set_value(const std::string& s) {
        std::lock_guard guard(_lock);
        _value = s;
    }

private:
    std::string _value;
    mutable std::mutex _lock;
};

} // end namespace var


#endif // VAR_STATUS_H