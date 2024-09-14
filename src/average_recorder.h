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

// Date Thur Sep 12 18:59:20 CST 2024.

#ifndef VAR_AVERAGE_RECORDER_H
#define VAR_AVERAGE_RECORDER_H

#include "src/variable.h"
#include "src/detail/combiner.h"
#include "src/detail/sampler.h"
#include "src/reducer.h"

namespace var {

struct Stat {
    Stat() : sum(0), num(0) {}
    Stat(int64_t sum2, int64_t num2) : sum(sum2), num(num2) {}
    int64_t sum;
    int64_t num;

    int64_t get_average_int() const {
        // num can be changed by sampling thread, use tmp_num.
        int64_t tmp_num = num;
        if(tmp_num == 0) {
            return 0;
        }
        return sum / tmp_num;
    }

    double get_average_double() const {
        int64_t tmp_num = num;
        if(tmp_num == 0) {
            return 0.0;
        }
        return (double)sum / (double)tmp_num;
    }

    Stat operator-(const Stat& rhs) const {
        return Stat(sum - rhs.sum, num - rhs.num);
    }
    void operator-=(const Stat& rhs) {
        sum -= rhs.sum;
        num -= rhs.num;
    }
    Stat operator+(const Stat& rhs) const {
        return Stat(sum + rhs.sum, num + rhs.num);
    }
    void operator+=(const Stat& rhs) {
        sum += rhs.sum;
        num += rhs.num;
    }
};

inline std::ostream& operator<<(std::ostream& os, const Stat& s) {
    const int64_t v = s.get_average_int();
    if(v != 0) {
        return os << v;
    }
    else {
        return os << s.get_average_double();
    }
}

// 区分std::is_integtal / std::is_floating_point or others.
// For calculating average of numbers.
// Example:
//   IntRecorder latency;
//   latency << 1 << 3 << 5;
//   CHECK_EQ(3, latency.average());
class AverageRecorder : public Variable {
public:
    // Specialize for Stat operator int64_t of combiner.
    struct AddStat {
        void operator()(Stat& s1, const Stat& s2) const { s1 += s2; }
    };
    struct MinusStat {
        void operator()(Stat& s1, const Stat& s2) const { s1 -= s2; }
    };
    typedef Stat value_type;    
    typedef detail::ReducerSampler<AverageRecorder, Stat,
                                   AddStat, MinusStat> sampler_type;

    struct AddToStat {
        void operator()(Stat& lhs, uint64_t rhs) const {
            lhs.sum += rhs;
            lhs.num += 1;
        }
    };

    typedef detail::AgentCombiner<Stat, uint64_t, AddToStat> combiner_type;
    typedef combiner_type::Agent agent_type;

public:
    AverageRecorder() : _sampler(nullptr) {}
    explicit AverageRecorder(const std::string& name) : _sampler(nullptr) {
        expose(name);
    }
    AverageRecorder(const std::string& prefix, const std::string& name) 
        : _sampler(nullptr) {
        expose_as(prefix, name);
    }
    ~AverageRecorder() {
        hide();
        if(!_sampler) {
            _sampler->destroy();
            _sampler = nullptr;
        }
    }

    // Note: The input type is acutally int. Use int64_t to check overflow.
    inline AverageRecorder& operator<<(uint64_t value) {
        agent_type* agent = _combiner.get_or_create_tls_agent();
        if(VAR_UNLIKELY(!agent)) {
            LOG_ERROR << "Fail to create agent";
            return *this;
        }
        // Does not correct with template
        
    }
    
    int64_t average() const {
        return _combiner.combine_agents().get_average_int();
    }

    double average(double) const {
        return _combiner.combine_agents().get_average_double();
    }

    Stat get_value() const {
        return _combiner.combine_agents();
    }

    Stat reset() {
        return _combiner.reset_all_agents();
    }

    AddStat op() const { return AddStat(); }
    MinusStat inv_op() const { return MinusStat(); }

    bool valid() const { return _combiner.vaild(); }

    void describe(std::ostream& os, bool quote_string) const override {
        os << get_value();
    }

    sampler_type* get_sampler() {
        if (NULL == _sampler) {
            _sampler = new sampler_type(this);
            _sampler->schedule();
        }
        return _sampler;
    }

    // This name is useful for printing overflow log in operator<< since
    // IntRecorder is often used as the source of data and not exposed.
    void set_debug_name(const std::string& name) {
        _debug_name.assign(name.data(), name.size());
    }

private:
    combiner_type   _combiner;
    sampler_type*   _sampler;
    std::string     _debug_name;
};


} // end namespace var

#endif // VAR_AVERAGE_RECORDER_H