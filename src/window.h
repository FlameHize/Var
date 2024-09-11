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

// Date Wed Sep 11 10:52:08 CST 2024.

#ifndef VAR_WINDOW_H
#define VAR_WINDOW_H

#include "src/variable.h"
#include "src/detail/sampler.h"
#include "src/detail/series.h"
#include "net/base/Logging.h"
#include <math.h>
#include <limits>

namespace var {

enum SeriesFrequency {
    SERIES_IN_WINDOW = 0,
    SERIES_IN_SECOND = 1
};

namespace detail {
// Just for constructor reusing of Window<>.
template<typename R, SeriesFrequency series_freq>
class WindowBase : public Variable {
public:
    typedef typename R::value_type value_type;
    typedef typename R::sampler_type sampler_type;

    class SeriesSampler : public Sampler {
    public:
        struct Op {
            explicit Op(R* reducer) : _reducer(reducer) {}
            void operator()(value_type& v1, const value_type& v2) const {
                _reducer->op()(v1, v2);
            }

        private:
            R* _reducer;
        };

        SeriesSampler(WindowBase* owner, R* reducer)
            : _owner(owner)
            , _series(Op(reducer)) {
        }
        
        ~SeriesSampler() {}
        
        void take_sample() override {
            if(series_freq == SERIES_IN_WINDOW) {
                // Get the value inside the full window. 
                // "get_value(1)" is incorrect when users intend to see 
                // aggregated values of the full window in the plot.
                _series.append(_owner->get_value());
            }
            else {
                // Special with a Sampler window_size is 1.
                // Used for PerSecond<> ,otherwise the "smoother" plot may hide peaks.
                _series.append(_owner->get_value(1));
            }
        }

        void describe(std::ostream& os) {
            _series.describe(os, nullptr);
        }

    private:
        WindowBase* _owner;
        Series<value_type, Op> _series;
    };

    WindowBase(R* reducer, time_t window_size)
        : _reducer(reducer)
        , _window_size(window_size > 0 ? window_size : 10)
        , _sampler(reducer->get_sampler())
        , _series_sampler(nullptr) {
        _sampler->set_window_size(_window_size);
    }

    ~WindowBase() {
        hide();
        if(_series_sampler) {
            _series_sampler->destroy();
            _series_sampler = nullptr;
        }
    }

    bool get_span(time_t window_size, Sample<value_type>* result) const {
        return _sampler->get_value(window_size, result);
    }

    bool get_span(Sample<value_type>* result) const {
        return get_span(_window_size, result);
    }

    virtual value_type get_value(time_t window_size) const {
        Sample<value_type> tmp;
        if(get_span(window_size, &tmp)) {
            return tmp.data;
        }
        return value_type();
    }

    value_type get_value() const { return get_value(_window_size); }

    void describe(std::ostream& os, bool quote_string) const override {
        if(std::is_same<value_type, std::string>::value && quote_string) {
            os << '"' << get_value() << '"';
        }
        else {
            os << get_value();
        }
    }

    int describe_series(std::ostream& os) const override {
        if(!_series_sampler) {
            return 1;
        }
        _series_sampler->describe(os);
        return 0;
    }

    time_t window_size() const {
        return _window_size;
    }

    void get_samples(std::vector<value_type>* samples) const {
        samples->clear();
        samples->reserve(_window_size);
        return _sampler->get_samples(_window_size, samples);
    }

protected:
    int expose_impl(const std::string& prefix,
                    const std::string& name,
                    DisplayFilter display_filter) override {
        const int rc = Variable::expose_impl(prefix, name, display_filter);
        if(rc == 0 && !_series_sampler) {
            _series_sampler = new SeriesSampler(this, _reducer);
            _series_sampler->schedule();
        }
        return rc;
    }

private:
    R*              _reducer;
    time_t          _window_size;
    sampler_type*   _sampler;
    SeriesSampler*  _series_sampler;
};
} // end namespace detail

// Get data within a time window.
// The time unit is 1 second fixed.
// Window relies on other var which should be constructed before this window
// and destructs after this window.

// R must:
// - have get_sampler() (not require thread-safe).
// - define value_type and sampler type.
template<typename R, SeriesFrequency series_freq = SERIES_IN_WINDOW>
class Window : public detail::WindowBase<R, series_freq> {
    typedef detail::WindowBase<R, series_freq> Base;
    typedef typename R::value_type value_type;
    typedef typename R::sampler_type sampler_type;
public:
    // Different from PerSecond, we require window size here because get_value()
    // of window is largely affected by window size while PerSecond is not.
    Window(R* reducer, time_t window_size) : Base(reducer, window_size) {}
    Window(const std::string& name, R* reducer, time_t window_size) 
        : Base(reducer, window_size) {
        this->expose(name);
    }
    Window(const std::string& prefix, const std::string& name,
           R* reducer, time_t window_size)
        : Base(reducer, window_size) {
        this->expose_as(prefix, name);
    }
};

// Obtain the average change value persecond of a var.
// The only difference between PerScond and Window is that PerSecond
// return value is divided by time window_size.
template<typename R, SeriesFrequency series_freq = SERIES_IN_SECOND>
class PerSecond : public detail::WindowBase<R, series_freq> {
    typedef detail::WindowBase<R, series_freq> Base;
    typedef typename R::value_type value_type;
    typedef typename R::sampler_type sampler_type;
public:
    // If window_size is non-positive or absent, use default value.
    PerSecond(R* reducer) : Base(reducer, -1) {}
    PerSecond(R* reducer, time_t window_size) : Base(reducer, window_size) {}
    PerSecond(const std::string& name, R* reducer) : Base(reducer, -1) {
        this->expose(name);
    }
    PerSecond(const std::string& prefix, const std::string& name, R* reducer)
        : Base(reducer, -1) {
        this->expose_as(prefix, name);
    }
    PerSecond(const std::string& prefix, const std::string& name, 
              R* reducer, time_t window_size)
        : Base(reducer, window_size) {
        this->expose_as(prefix, name);
    }

    value_type get_value(time_t window_size) const override {
        detail::Sample<value_type> s;
        this->get_span(window_size, &s);
        // We may test if the mulitiplcation overflows and use intergral ops
        // if possible. However signed/unsigned 32-bit/64-bit make the solution
        // complex. Since this function is not called often, we use floating
        // point for simplicity.
        if(s.time_us <= 0) {
            return static_cast<value_type>(0);
        }
        if(std::is_floating_point<value_type>::value) {
            return static_cast<value_type>(s.data * 1000000.0 / s.time_us);
        }
        else {
            return static_cast<value_type>(round(s.data * 1000000.0 / s.time_us));
        }
    }

    value_type get_value() const {
        return Base::get_value();
    }
};


} // end namespace var


#endif // VAR_WINDOW_H