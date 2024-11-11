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

// Date Thur Sep 26 13:55:26 CST 2024.

#ifndef VAR_DETAIL_PERCENTILE_H
#define VAR_DETAIL_PERCENTILE_H

#include "metric/common.h"
#include "metric/reducer.h"
#include "metric/window.h"
#include "metric/detail/combiner.h"
#include "metric/detail/sampler.h"
#include "metric/util/fast_rand.h"
#include <string.h>                     // memset memcmp
#include <stdint.h>                     // uint32_t
#include <limits>                       // std::numeric_limits
#include <ostream>                      // std::ostream
#include <algorithm>                    // std::sort
#include <math.h>                       // ceil

namespace var {
namespace detail {

// Round of expectation of a rational number |a/b| to a natural number.
inline unsigned long round_of_expection(unsigned long a, unsigned long b) {
    if(VAR_UNLIKELY(b == 0)) {
        return 0;
    }
    return a / b + (fast_rand_less_than(b) < a % b); 
}

// Storing latencies inside a interval.
template<size_t SAMPLE_SIZE>
class PercentileInterval {
public:
    PercentileInterval()
        : _sorted(false)
        , _num_added(0)
        , _num_samples(0) {
    }

    // Get index-th sample in ascending order.
    uint32_t get_sample_at(size_t index) {
        const size_t saved_num = _num_samples;
        if(index >= saved_num) {
            if(saved_num == 0) {
                return 0;
            }
            index = saved_num - 1;
        }
        if(!_sorted) {
            std::sort(_samples, _samples + saved_num);
            _sorted = true;
        }
        return _samples[index];
    }

    // Add an unsigned 32-bit latency(what percentile actually accepts) to a
    // non-full interval, which is invoked by Percentile::operator<< to add a
    // sample into the ThreadLocalPercentileSamples.
    // Return true if the input was stored.
    bool add32(uint32_t x) {
        if(VAR_UNLIKELY(_num_samples >= SAMPLE_SIZE)) {
            LOG_ERROR << "This interval was full";
            return false;
        }
        ++_num_added;
        _samples[_num_samples++] = x;
        return true;
    }

    // Add a signed latency.
    bool add64(int64_t x) {
        if(x >= 0) {
            return add32((uint32_t)x);
        }
        return false;
    }

    // Add samples of another interval(TLS to Global). This function tries to
    // make each sample in merged _samples has (approximately) equal probability
    // to remain.
    // This method is invoked when merging ThreadLocalPercentileSamples in to
    // GlobalPercentileSamples.
    template<size_t size2>
    void merge(const PercentileInterval<size2>& rhs) {
        if(rhs._num_added == 0) {
            return;
        }
        assert(rhs._num_samples == rhs._num_added);
        // _num_added + rhs._num_added <= SAMPLE_SIZE  which indicates that
        // no sample has been dropped.
        // _num_added + rhs._num_added > SAMPLE_SIZE, so we should keep
        // SAMPLE_SIZE * (_num_added / (_num_added + rhs._num_added)) from |this|,
        // SAMPLE_SIZE * (rhs._num_added / (_num_added + rhs._num_added)) from |rhs|.
        // So we could keep the probability of each samples in |this|
        // (_num_samples / _num_added) equal to the probability of each samples in |rhs|
        // (rhs._num_samples / rhs._num_added).
        // Ex. SAMPLE_SIZE = 100
        // |this| added_count() is 100, sample_count() is 100.
        // |rhs| added_count() is 50, sample_count() is 50.
        // |this| should remain SAMPLE_SIZE * (100 / (100 + 50)) = 67.
        // |rhs| should remain SAMPLE_SIZE * (50 / (100 + 50)) = 33.
        if(_num_added + rhs._num_added <= SAMPLE_SIZE) {
            // No samples should be dropped.
            memcpy(_samples + _num_samples, rhs._samples, 
                    sizeof(_samples[0]) * rhs._num_samples);
            _num_samples += rhs._num_samples;
        } 
        else {
            size_t num_remain = round_of_expection(
                SAMPLE_SIZE * _num_added, _num_added + rhs._num_added);
            assert(num_remain <= _num_samples);
            // Randomly drop samples of this. 
            // Use [num_remain, _num_samples) value to random replace
            // [0, num_remain) value.
            for(size_t i = _num_samples; i > num_remain; --i) {
                _samples[fast_rand_less_than(i)] = _samples[i - 1];
            }
            // Have to copy data from rhs to shuffle since it's const.
            const size_t num_remain_from_rhs = SAMPLE_SIZE - num_remain;
            assert(num_remain_from_rhs <= rhs._num_samples);
            uint32_t tmp[rhs._num_samples];
            memcpy(tmp, rhs._samples, sizeof(uint32_t) * rhs._num_samples);
            for(size_t i = 0; i < num_remain_from_rhs; ++i) {
                const int index = fast_rand_less_than(rhs._num_samples - i);
                _samples[num_remain++] = tmp[index];
                // Prevent same.
                tmp[index] = tmp[rhs._num_samples - i - 1];
            }
            _num_samples = num_remain;
            assert(_num_samples == SAMPLE_SIZE);
        }
        _num_added += rhs._num_added;
    }

    // Randomly pick n samples from mutable_rhs to |this|.
    template<size_t size2>
    void merge_with_expectation(PercentileInterval<size2>& mutable_rhs, size_t n) {
        assert(n <= mutable_rhs._num_samples);
        _num_added += mutable_rhs._num_added;
        if(_num_samples + n <= SAMPLE_SIZE && n == mutable_rhs._num_samples) {
            memcpy(_samples + _num_samples, mutable_rhs._samples,
                        sizeof(_samples[0]) * n);
            _num_samples += n;
            return;
        }
        for(size_t i = 0; i < n; ++i) {
            size_t index = fast_rand_less_than(mutable_rhs._num_samples);
            if(_num_samples < SAMPLE_SIZE) {
                _samples[_num_samples++] = mutable_rhs._samples[index];
            }
            else {
                _samples[fast_rand_less_than(_num_samples)] = mutable_rhs._samples[index];
            }
            std::swap(mutable_rhs._samples[index],
                    mutable_rhs._samples[mutable_rhs._num_samples - i - 1]);
        }
    }


    // Remove all samples inside, reset the state.
    void clear() {
        _sorted = false;
        _num_added = 0;
        _num_samples = 0;
    }

    // True if no more room for new samples.
    bool full() const { return _num_samples == SAMPLE_SIZE; }

    // True if there's no samples.
    bool empty() const { return !_num_samples; }

    // samples ever added by calling add*().
    uint32_t added_count() const { return _num_added; }

    // samples stored.
    uint32_t sample_count() const { return _num_samples; }

    // For debuggin.
    void describe(std::ostream& os) const {
        os << "(num added = " << added_count() << ")[";
        for(size_t i = 0; i < _num_samples; ++i) {
            os << ' ' << _samples[i];
        }
        os << " ]";
    }

    // True if two PercentileInterval are exactly same, namely same # of added and
    // same samples, mainly for debuggin.
    bool operator==(const PercentileInterval& rhs) const {
        return (_num_added == rhs._num_added &&
                _num_samples == rhs._num_samples &&
                memcmp(_samples, rhs._samples,  _num_samples * sizeof(uint32_t)) == 0);
    }

private:
    template<size_t size2>
    friend class PercentileInterval;

    bool _sorted;
    // Total samples count ever added in interval.
    uint32_t _num_added;
    // Now memory store samples count.
    uint16_t _num_samples;
    uint32_t _samples[SAMPLE_SIZE];
};


static const size_t NUM_INTERVALS = 32;

class AddLatency;

// Group of PercentileIntervals.
template<size_t SAMPLE_SIZE_IN>
class PercentileSamples {
public:
    friend class AddLatency;
    static const size_t SAMPLE_SIZE = SAMPLE_SIZE_IN;

    PercentileSamples() {
        memset(this, 0, sizeof(*this));
    }

    ~PercentileSamples() {
        for(size_t i = 0; i < NUM_INTERVALS; ++i) {
            if(_intervals[i]) {
                delete _intervals[i];
                _intervals[i] = nullptr;
            }
        }
    }

    // Copy-construct from another PercentileSamples.
    PercentileSamples(const PercentileSamples& rhs) {
        _num_added = rhs._num_added;
        for (size_t i = 0; i < NUM_INTERVALS; ++i) {
            if (rhs._intervals[i] && !rhs._intervals[i]->empty()) {
                _intervals[i] = new PercentileInterval<SAMPLE_SIZE>(*rhs._intervals[i]);
            } else {
                _intervals[i] = nullptr;
            }
        }
    }

    // Assign from another PercentileSamples.
    // Notice that we keep empty _intervals to avoid future allocations.
    void operator=(const PercentileSamples& rhs) {
        _num_added = rhs._num_added;
        for (size_t i = 0; i < NUM_INTERVALS; ++i) {
            if (rhs._intervals[i] && !rhs._intervals[i]->empty()) {
                get_interval_at(i) = *rhs._intervals[i];
            } else if (_intervals[i]) {
                _intervals[i]->clear();
            }
        }
    }

    // Get the `ratio'-ile value. E.g. 0.99 means 99%-ile value.
    // Since we store samples in different intervals internally. We first
    // address the interval by multiplying ratio with _num_added, then
    // find the sample inside the interval.
    uint32_t get_number(double ratio) {
        size_t n = (size_t)ceil(ratio * _num_added);
        if(n > _num_added) {
            n = _num_added;
        }
        else if(n == 0) {
            return 0;
        }
        for(size_t i = 0; i < NUM_INTERVALS; ++i) {
            if(!_intervals[i]) {
                continue;
            }
            PercentileInterval<SAMPLE_SIZE>& invl = *_intervals[i];
            if(n <= invl.added_count()) {
                size_t sample_n = n * invl.sample_count() / invl.added_count();
                size_t sample_index = (sample_n ? sample_n - 1 : 0);
                return invl.get_sample_at(sample_index);
            }
            n -= invl.added_count();
        }
        // Can't reach here.
        return std::numeric_limits<uint32_t>::max();
    }

    // Add samples in another PercentileSamples.
    template <size_t size2>
    void merge(const PercentileSamples<size2> &rhs) {
        _num_added += rhs._num_added;
        for (size_t i = 0; i < NUM_INTERVALS; ++i) {
            if (rhs._intervals[i] && !rhs._intervals[i]->empty()) {
                get_interval_at(i).merge(*rhs._intervals[i]);
            }
        }
    }

    // Combine multiple into a single PercentileSamples.
    template<typename Iterator>
    void combine_of(const Iterator& begin, const Iterator& end) {
        if (_num_added) {
            // Very unlikely
            for (size_t i = 0; i < NUM_INTERVALS; ++i) {
                if (_intervals[i]) {
                    _intervals[i]->clear();
                }
            }
            _num_added = 0;
        }

        for (Iterator iter = begin; iter != end; ++iter) {
            _num_added += iter->_num_added;
        }

        // Calculate probabilities for each interval.
        for(size_t i = 0; i < NUM_INTERVALS; ++i) {
            size_t total = 0;
            size_t total_samples = 0;
            for(Iterator iter = begin; iter != end; ++iter) {
                if(iter->_intervals[i]) {
                    total += iter->_intervals[i]->added_count();
                    total_samples += iter->_intervals[i]->sample_count();
                }
            }
            if(total == 0) {
                // Empty interval
                continue;
            }

            // Consider that sub interval took |a| samples out of |b| totally,
            // each sample won the probability of |a/b| according to the
            // algorithm of add32(), so each subintervals expected number 
            // of samples is |SAMPLE_SIZE * (b / total)|.
            for(Iterator iter = begin; iter != end; ++iter) {
                if(!iter->_intervals[i] || iter->_intervals[i]->empty()) {
                    continue;
                }
                typename add_reference<decltype(*(iter->_intervals[i]))>::type
                        invl = *(iter->_intervals[i]);
                if(total <= SAMPLE_SIZE) {
                    get_interval_at(i).merge_with_expectation(invl, invl.sample_count());
                    continue;
                }
                // Each
                const size_t b = invl.added_count();
                const size_t remain = std::min(
                    round_of_expection(SAMPLE_SIZE * b, total), 
                    (size_t)invl.sample_count());
                get_interval_at(i).merge_with_expectation(invl, remain);
            }
        }
    }

    // For debuggin.
    void describe(std::ostream &os) const {
        os << this << "{num_added = " << _num_added << "}\r\n";
        for (size_t i = 0; i < NUM_INTERVALS; ++i) {
            if (_intervals[i] && !_intervals[i]->empty()) {
                os << " interval[" << i << "]=";
                _intervals[i]->describe(os);
                os << "\r\n";
            }
        }
    }

    // True if intervals of two PercentileSamples are exactly same.
    bool operator==(const PercentileSamples& rhs) const {
        for (size_t i = 0; i < NUM_INTERVALS; ++i) {
            if (_intervals != rhs._intervals[i]) {
                return false;
            }
        }
        return true;
    }

    // Get/Create interval on-demand.
    PercentileInterval<SAMPLE_SIZE>& get_interval_at(size_t index) {
        if(!_intervals[index]) {
            _intervals[index] = new PercentileInterval<SAMPLE_SIZE>;
        }
        return *_intervals[index];
    }

private:
    template<size_t size2>
    friend class PercentileSamples;

    // Sum of _num_added of all intervals. we update this value after any
    // changes to intervals inside to make it O(1)-time accessible.
    size_t _num_added;
    PercentileInterval<SAMPLE_SIZE>* _intervals[NUM_INTERVALS];
};

template<size_t size> 
const size_t PercentileSamples<size>::SAMPLE_SIZE;

template<size_t size>
std::ostream& operator<<(std::ostream& os, const PercentileInterval<size>& p) {
    p.describe(os);
    return os;
}

template<size_t size>
std::ostream& operator<<(std::ostream& os, const PercentileSamples<size>& p) {
    p.describe(os);
    return os;
}

typedef PercentileSamples<254> GlobalPercentileSamples;
typedef PercentileSamples<30> ThreadLocalPercentileSamples;

// A specialized reducer for finding the precentile of latencies
// NOTE: DON'T use it directly, use LatencyRecorder instead.
class Percentile : public noncopyable {
public:
    struct AddPercentileSamples {
        // Used for Window GlobalSamples merge, NOT call in operator()<<.
        template<size_t size1, size_t size2>
        void operator()(PercentileSamples<size1>& b1, 
                        const PercentileSamples<size2>& b2) const {
            b1.merge(b2);
        }
    };

    typedef GlobalPercentileSamples                 value_type;     // window.h inside need.
    typedef AgentCombiner<GlobalPercentileSamples,
                          ThreadLocalPercentileSamples,
                          AddPercentileSamples>     combiner_type;
    typedef ReducerSampler<Percentile,
                           GlobalPercentileSamples,
                           AddPercentileSamples,
                           VoidOp>                  sampler_type;
    typedef combiner_type::Agent                    agent_type;

    Percentile();
    ~Percentile();

    // The sampler for windows over percentile.
    sampler_type* get_sampler() {
        if(!_sampler) {
            _sampler = new sampler_type(this);
            _sampler->schedule();
        }
        return _sampler;
    }

    value_type get_value() const;

    // The Reducer::Element::modify() func was passing parameters
    // directly to the ResultTp. Here, we need to merge global from
    // passing parameter to TLS and then to Global.
    Percentile& operator<<(int64_t latency);

    value_type reset();

    AddPercentileSamples op() const { 
        return AddPercentileSamples();
    }

    VoidOp inv_op() const { 
        return VoidOp();
    }

    bool valid() const {
        return _combiner != nullptr && _combiner->vaild();
    }

    // This name is useful for warning negative latencies in operator<<.
    void set_debug_name(const std::string& name) {
        _debug_name = name;
    }

private:
    combiner_type*  _combiner;
    sampler_type*   _sampler;
    std::string     _debug_name;
};

} // end namespace detail
} // end namespace var

#endif // VAR_DETAIL_PERCENTILE_H