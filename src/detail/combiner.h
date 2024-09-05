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

#ifndef VAR_DETAIL_COMBINER_H
#define VAR_DETAIL_COMBINER_H

#include "call_op_returing_void.h"
#include "is_atomical.h"
#include <type_traits>
#include <mutex>

namespace var {
namespace detail {

template<typename T, typename Enabler = void>
class ElementContainer {
public:
    // Used for debug.
    ElementContainer() { 
        std::cout << "POD" << std::endl; 
    }
    void load(T* out) {
        std::lock_guard<std::mutex> guard(_mutex);
        *out = _value;
    }

    void store(const T& new_value) {
        std::lock_guard<std::mutex> guard(_mutex);
        _value = new_value;
    }

    void exchange(T* prev, const T& new_value) {
        std::lock_guard<std::mutex> guard(_mutex);
        *prev = _value;
        _value = new_value;
    }

    template<typename Op, typename T1>
    void modify(const Op& op, const T1& value2) {
        std::lock_guard<std::mutex> guard(_mutex);
        call_or_returning_void(op, _value, value2);
    }

    // [Unique]
    template<typename Op, typename GlobalValue>
    void merge_global(const Op& op, GlobalValue& global_value) {
        std::lock_guard<std::mutex> guard(_mutex);
        call_or_returning_void(op, global_value, _value);
        //op(global_value, _value);
    }

private:
    T _value;
    std::mutex _mutex;
};

template<typename T>
class ElementContainer<T, typename std::enable_if<std::is_atomic<T>::value>::type> {
public:
    // Used for debug.
    ElementContainer() { 
        std::cout << "ATOMIC" << std::endl; 
    }

    // Do not need any memory fencing here, every op is relaxed.
    inline void load(T* out) {
        *out = _value.load(std::memory_order_relaxed);
    }

    inline void store(const T& new_value) {
        _value.store(new_value.load(), std::memory_order_relaxed);
    }

    inline void exchange(T* prev, T new_value) {
        *prev = _value.exchange(new_value, std::memory_order_relaxed);
    }

    template<typename Op, typename T1>
    void modify(const Op& op, const T1& value2) {
        T old_value = _value.load(std::memory_order_relaxed);
        T new_value = old_value;
        call_or_returning_void(op, new_value, value2);
        // There's a contention with the reset operation of combiner,
        // if the tls value has been modified during _op, the compare
        // exchange weak operation will fail and recalculation is 
        // to be processed according to the new version of value.
        // std::atomic::compare_exchange_weak(T& expected, T desired, memory_order)
        // Func will check if cur atomic value is equal to expected.
        // if equal, func will try to change cur atomic value to |desired| value.
        // Return true if success, otherwise return false. 
        while(!_value.compare_exchange_weak(old_value, new_value, std::memory_order_relaxed)) {
            new_value = old_value;
            call_or_returning_void(op, new_value, value2);
        }
    }

    // [Unique]
    inline bool compare_exchange_weak(T& expected, T new_value) {
        return _value.compare_exchange_weak(expected, new_value, std::memory_order_relaxed);
    }

private:
    std::atomic<T> _value;
};


} // end namespace detail
} // end namespace var


#endif