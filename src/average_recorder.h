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

// 区分std::is_integtal / std::is_floating_point.
// For calculating average of numbers.
// Example:
//   IntRecorder latency;
//   latency << 1 << 3 << 5;
//   CHECK_EQ(3, latency.average());
class AverageRecorder : public Variable {
public:
    // Compressing format:
    // | 20 bits (unsigned) | sign bit | 43 bits |
    //       num                   sum
    // Specialize for Stat operator int64_t of combiner.
    const static size_t SUM_BIT_WIDTH = 44;
    const static uint64_t MAX_SUM_PER_THREAD = (1ul << SUM_BIT_WIDTH) - 1;
    const static uint64_t MAX_NUM_PER_THREAD = (1ul << (64ul - SUM_BIT_WIDTH)) - 1;

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
            lhs.sum += _extend_sign_bit(_get_sum(rhs));
            lhs.num += _get_num(rhs);
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
        if(_sampler) {
            _sampler->destroy();
            _sampler = nullptr;
        }
    }

    // Note: The input type is acutally int. Use int64_t to check overflow.
    inline AverageRecorder& operator<<(int64_t value) {
        // Truncate to be max or min of int. We using SUM_BIT_WIDTH bits to
        // store the sum thus following aggregations are not likely to 
        // be overflow/underflow.
        if(VAR_UNLIKELY((int64_t)(int)value != value)) {
            const char* reason = nullptr;
            if(value > std::numeric_limits<int>::max()) {
                reason = "overflows";
                value = std::numeric_limits<int>::max();
            }
            else {
                reason = "underflows";
                value = std::numeric_limits<int>::min();
            }
            if(!name().empty()) {
                // Already exposed.
                LOG_WARN << "Input = " << value << " to "
                         << name() << "\' " << reason;
            }
            else if(!_debug_name.empty()) {
                LOG_WARN << "Input = " << value << " to "
                         << _debug_name << "\' " << reason;
            }
            else {
                LOG_WARN << "Input = " << value << " to AverageRecorder("
                         << (void*)this << ") " << reason; 
            }
        }

        agent_type* agent = _combiner.get_or_create_tls_agent();
        if(VAR_UNLIKELY(!agent)) {
            LOG_ERROR << "Fail to create agent";
            return *this;
        }
        uint64_t n;
        agent->element.load(&n);
        const uint64_t complement = _get_sum(value);
        uint64_t num;
        uint64_t sum;
        do {
            num = _get_num(n);
            sum = _get_sum(n);
            if(VAR_UNLIKELY((num + 1 > MAX_NUM_PER_THREAD) ||
                _will_overflow(_extend_sign_bit(sum), value))) {
                agent->combiner->commit_and_clear(agent);
                sum = 0;
                num = 0;
                n = 0;
            }
        } while(!agent->element.compare_exchange_weak(
                    n, _compress(num + 1, sum + complement)));
        return *this;
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
    static uint64_t _get_sum(const uint64_t n) {
        return (n & MAX_SUM_PER_THREAD);
    }

    static uint64_t _get_num(const uint64_t n) {
        return (n >> SUM_BIT_WIDTH);
    }

    static uint64_t _compress(const uint64_t num, const uint64_t sum) {
        // There is a redundant '1' in the front of sum which was combined
        // with two negative number, so truncation has to be done here.
        return (num << SUM_BIT_WIDTH) | (sum & MAX_SUM_PER_THREAD);
    }

    // Expanding a signed number to a larger data type requires performing
    // symbol extension, with the reule being to extend the symbol bits
    // to the required number of bits.
    // Fill all the first (64 - SUM_BIT_WITH + 1) bits with 1 if the sign 
    // bit to represent a complete 64-bit negatived number;
    // 1ul << (SUM_BIT_WIDTH - 1) & sum: get signed value 1 or 0.
    // (1ul << (64ul - SUB_BIT_WIDTH + 1)) - 1 expant to fill symbol ahead of sign to sign.
    // | (int64_t)sum fill the sum value to last (SUM_BIT_WIDTH -1) digits.
    // E.X set int64_t to int8_t SUM_BIT_WIDTH = 4 sum's signed value = 1
    // 1ul << (SUM_BIT_WIDTH - 1) & sum =        0b00001000
    // (1ul << (64ul - SUB_BIT_WIDTH + 1)) - 1 = 0b00011111
    // multiple result = 0b11111 =               0b11111000
    // sum'a actually max value is last 3 digits max value = 0b111 = 7.
    static int64_t _extend_sign_bit(const uint64_t sum) {
        return ((1ul << (SUM_BIT_WIDTH - 1) & sum)
            * ((1ul << (64ul - SUM_BIT_WIDTH + 1)) - 1))
            | (int64_t)sum;
    }

    // Check whether the sum of twon integer overflows the range of signed
    // integer with the width of SUM_BIT_WIDTH. which is 
    // [-2^(SUM_BIT_WIDTH-1), 2^(SUM_BIT_WIDTH-1)-1].
    // eg. [-12, 127] for signed 8-bit integer.
    static bool _will_overflow(const int64_t lhs, const int rhs) {
        return 
            // Both integers are positive and the sum is larger than the 
            // largest number
            ((lhs > 0) && (rhs > 0)
                && (lhs + rhs > ((int64_t)MAX_SUM_PER_THREAD >> 1)))
            // Or both integers are negative and the sum is less than the 
            // lowest number
            || ((lhs < 0) && (rhs < 0)
                && (lhs + rhs < (-((int64_t)MAX_SUM_PER_THREAD >> 1)) - 1));
            // otherwise the sum cannot overflow if lhs does not overflow
            // because |sum| < |lhs|.
    } 

private:
    combiner_type   _combiner;
    sampler_type*   _sampler;
    std::string     _debug_name;
};


} // end namespace var

#endif // VAR_AVERAGE_RECORDER_H