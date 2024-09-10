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

// Date: Mon Sep 09 09:58:33 CST 2024

#ifndef VAR_DETAIL_SAMPLER_H
#define VAR_DETAIL_SAMPLER_H

#include "src/util/bounded_queue.h"
#include "src/util/linked_list.h"
#include "src/util/type_traits.h"
#include "src/util/time.h"
#include "net/base/Logging.h"
#include <mutex>

namespace var {
namespace detail {

template<typename T>
struct Sample {
    T data;
    int64_t time_us;

    Sample() : data(), time_us(0) {}
    Sample(const T& data2, int64_t time2) : data(data2), time_us(time2) {}
};

// The base class for all samplers whose take_sample() all called periodically.
class Sampler : public LinkNode<Sampler> {
public:
    Sampler();

    // This function will be called every second(approximately) in a
    // dedicated thread if schedule() is called.
    virtual void take_sample() = 0;

    // Register this sampler globally so that take_sample() will be called
    // periodically.
    void schedule();

    // Call this function instead of delete to destroy the sampler. Deletion
    // of the sampler may be delayed for seconds.
    void destroy();

protected:
    // Prevent direct instantiation.
    // Ensure that all resource cleaning behaviors only occur in derived classes.
    virtual ~Sampler();

    bool _used;

    // Sync destroy() and take_sample().
    friend class SamplerCollector;
    std::mutex _mutex;
};

// Representing a non-existing operator so that we can test
// std::is_same<Op, VoidOp>::value to write code for different branches.
// The false branch should be removed by complier at complie-time.
struct VoidOp {
    template<typename T>
    T operator()(const T&, const T&) const {
        LOG_ERROR << "This function should never be called, abort";
        abort();
    }
};

// The sampler for reducer-alike variables.
// The R should have following methods:
// - T reset();
// - T get_value();
// - Op op();
// - InvOp inv_op();
template<typename R, typename T, typename Op, typename InvOp>
class ReducerSampler : public Sampler {
public:
    static const time_t MAX_SECONDS_LIMIT = 3600;
    explicit ReducerSampler(R* reducer) 
        : _reducer(reducer)
        , _window_size(1) {
        // Invoked take_sample() at begining so that the value of the first
        // second would not be ingored.
        take_sample();
    }
    ~ReducerSampler() {}

    void take_sample() override {
        // Make _queue ready.
        // If _window_size is lare than what _queue can hold, e.g a larger
        // Window<> is created after running of sampler, make _queue larger.
        if((size_t)_window_size + 1 > _queue.capacity()) {
            const size_t new_cap = 
                std::max(_queue.capacity() * 2, (size_t)_window_size + 1);
            const size_t memsize = sizeof(Sample<T>)* new_cap;
            void* mem = malloc(memsize);
            if(!mem) return;
            BoundedQueue<Sample<T>> new_queue(mem, memsize, OWNS_STORAGE);
            Sample<T> temp;
            while(_queue.pop(&temp)) {
                new_queue.push(temp);
            }
            new_queue.swap(_queue);
        }
        Sample<T> latest;
        if(std::is_same<InvOp, VoidOp>::value) {
            // take_sample()'s operator can't be inversed.
            // we reset the reducer and save the result as a sample.
            // Suming up samples gives the result within a window.
            // In this case, get_value() of _reducer gives wrong answer
            // and should not be called.
            // It will effcient reducer's agent data shared by others.
            latest.data = _reducer->reset();
        }
        else {
            // If reverse operation can be performed between the latest
            // and oldest samples, the operator can be inversed.
            // Save the result as a sample within a window gives result.
            // get_value() of _reducer can still be called.
            latest.data = _reducer->get_value();
        }
        latest.time_us = gettimeofday_us();
        _queue.elim_push(latest);
    }

    bool get_value(time_t window_size, Sample<T>* result) {
        if(window_size <= 0) {
            LOG_ERROR << "Invalid window size = " << window_size;
            return false;
        }
        std::lock_guard guard(_mutex);
        if(_queue.size() <= 1UL) {
            // we need more samples to get reasonable result.
            return false;
        }
        // latest window_size time(s) data changeing condition.
        // latset->oldset window_size time(s) reducer value data sequence.
        Sample<T>* oldset = _queue.bottom(window_size);
        if(!oldset) {
            oldset = _queue.top();
        }
        Sample<T>* latest = _queue.bottom();
        if(std::is_same<InvOp, VoidOp>::value) {
            // No inverse op. op all samples within the window.
            // var::Maxer<int> maxer; window_size = 3;
            // 1s maxer << 1;  
            // 2s maxer << 2;
            // 3s maxer << 3;
            // 4s maxer << 4;
            // When time clock is 3s, oldset Sample data is 1, latest Sample data is 3
            // result Sample data is 3.
            // When time clock is 4s, oldset Sample data is 2, latest Sample data is 4
            // result Sample data is 4.
            result->data = latest->data;
            for(int i = 1; true; ++i) {
                Sample<T>* e = _queue.bottom(i);
                if(e == oldset) {
                    break;
                }
                _reducer->op()(result->data, e->data);
            }
        }
        else {
            // Diff the latest and oldset sample within the window.
            // var::Adder<int> adder; window_size = 3;
            // 1s adder << 1;  
            // 2s adder << 2;
            // 3s adder << 3;
            // 4s adder << 4;
            // When time clock is 3s, oldset Sample data is 1, latest Sample data is 3
            // result Sample data is 2.
            // When time clock is 4s, oldset Sample data is 2, latest Sample data is 4
            // result Sample data is 2.
            result->data = latest->data;
            _reducer->inv_op()(result->data, oldset->data);
        }
        result->time_us = latest->time_us - oldset->time_us;
        return true;
    }

    // Change the time window which can only go larger.
    int set_window_size(time_t window_size) {
        if(window_size <= 0 || window_size >= MAX_SECONDS_LIMIT) {
            LOG_ERROR << "Invalid window size = " << window_size;
            return -1;
        }
        std::lock_guard guard(_mutex);
        if(window_size > _window_size) {
            _window_size = window_size;
        }
        return 0;
    }

    void get_samples(time_t window_size, std::vector<T>* samples) {
        if(window_size <= 0) {
            LOG_ERROR << "Invalid window size = " << window_size;
            return;
        }
        std::lock_guard guard(_mutex);
        if(_queue.size() <= 1) {
            // we need more samples to get reasonable result.
            return;
        }
        Sample<T>* oldest = _queue.bottom(window_size);
        if (NULL == oldest) {
            oldest = _queue.top();
        }
        // fix i = 1 to i = 0.
        for (int i = 0; true; ++i) {
            Sample<T>* e = _queue.bottom(i);
            if (e == oldest) {
                break;
            }
            samples->push_back(e->data);
        }
    }

private:
    R* _reducer;
    time_t _window_size;
    BoundedQueue<Sample<T>> _queue;
};


} // end namespace detail
} // end namespace var


#endif // VAR_DETAIL_SAMPLER_H