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

#include "metric/detail/agent_group.h"
#include "metric/detail/call_op_returing_void.h"
#include "metric/util/linked_list.h"
#include "metric/util/type_traits.h"
#include "net/base/Logging.h"
#include <atomic>
#include <mutex>

namespace var {
namespace detail {

// Parameter to merge global, used for ResultTp is not same as ElementTp.
// GlobalValue is wrapper of TLS->Global processing.
template<typename Combiner>
class GlobalValue {
public:
    typedef typename Combiner::result_type result_type;
    typedef typename Combiner::Agent       agent_type;

    GlobalValue(agent_type* agent, Combiner* combiner)
        : _agent(agent), _combiner(combiner) {}
    ~GlobalValue() {}

    // Call this method to unlock tls element and lock the combiner.
    // Unlocking tls element avoids potential deadlock with
    // AgentCombiner::reset(), which also means that tls element may be
    // changed during calling of this method. BE AWARE OF THIS!
    // You are not allowed to change the tls value after call this func
    // before unlock().
    result_type* lock() {
        _agent->element._mutex.unlock();
        _combiner->_mutex.lock();
        return &_combiner->_global_result;
    }

    // Call this method to unlock the combiner and lock tls element again.
    void unlock() {
        _combiner->_mutex.unlock();
        _agent->element._mutex.lock();
    }

private:
    agent_type* _agent;
    Combiner*   _combiner;
};

template<typename T, typename Enabler = void>
class ElementContainer {
template<typename> friend class GlobalValue;
public:
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
    // Special a new Op to handle the merge process
    // from functon parameters(like int64_t latency) 
    // to ElementTp(ThreadLocalPercentleSamples)
    // and then to ResultTp(GlobalPercentileSamples).
    template<typename Op, typename GlobalValue>
    void merge_global(const Op& op, GlobalValue& global_value) {
        _mutex.lock();
        // TLS: _value
        // Global: *global_value.lock()
        // In this op, process func parameters to ElementTp
        // and then to ResultTp.
        op(global_value, _value);
        _mutex.unlock();
    }

private:
    T _value;
    std::mutex _mutex;
};

template<typename T>
class ElementContainer<T, typename std::enable_if<
    std::is_integral<T>::value || std::is_floating_point<T>::value>::type> {
public:
    // Do not need any memory fencing here, every op is relaxed.
    inline void load(T* out) {
        *out = _value.load(std::memory_order_relaxed);
    }

    inline void store(T new_value) {
        _value.store(new_value, std::memory_order_relaxed);
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

template<typename ResultTp, typename ElementTp, typename BinaryOp>
class AgentCombiner {
public:
    typedef ResultTp result_type;
    typedef ElementTp element_type;
    typedef AgentCombiner<ResultTp, ElementTp, BinaryOp> self_type;
    friend class GlobalValue<self_type>;

    struct Agent : public LinkNode<Agent> {
        Agent() : combiner(nullptr) {}
        ~Agent() {
            if(combiner) {
                combiner->commit_and_erase(this);
                combiner = nullptr;
            }
        }

        void reset(const ElementTp& val, self_type* c) {
            combiner = c;
            element.store(val);
        }

        // Call op(GlobalValue<Combiner> &, ElementTp &) to merge tls element
        // into global_result. The common impl. is:
        //   struct XXX {
        //       void operator()(GlobalValue<Combiner> & global_value,
        //                       ElementTp & local_value) const {
        //           if (test_for_merging(local_value)) {
        // 
        //               // Unlock tls element and lock combiner. Obviously
        //               // tls element can be changed during lock().
        //               ResultTp* g = global_value.lock();
        // 
        //               // *g and local_value are not changed provided
        //               // merge_global is called from the thread owning
        //               // the agent.
        //               merge(*g, local_value);
        //
        //               // unlock combiner and lock tls element again.
        //               global_value.unlock();
        //           }
        //
        //           // safe to modify local_value because it's already locked
        //           // or locked again after merging.
        //           ...
        //       }
        //   };
        // 
        // NOTE: Only available to non-atomic types.
        template<typename Op>
        void merge_global(const Op& op) {
            GlobalValue<self_type> g(this, combiner);
            element.merge_global(op, g);
        }

        self_type* combiner;
        ElementContainer<ElementTp> element;
    };

    typedef detail::AgentGroup<Agent> AgentGroup;

    explicit AgentCombiner(const ResultTp result_identity = ResultTp(),
                           const ElementTp element_idetity = ElementTp(),
                           const BinaryOp& op = BinaryOp())
        : _id(AgentGroup::create_new_agent())
        , _op(op)
        , _global_result(result_identity)
        , _result_identity(result_identity)
        , _element_identity(element_idetity) {
    }

    ~AgentCombiner() {
        if(_id >= 0) {
            clear_all_agents();
            AgentGroup::destroy_agent(_id);
            _id = -1;
        }
    }


    // [ThreadSafe] May be called from anywhere.
    ResultTp combine_agents() const {
        ElementTp tls_value;
        std::lock_guard guard(_mutex);
        ResultTp ret = _global_result;
        for(LinkNode<Agent>* node = _agents.head(); node != _agents.end(); node = node->next()) {
            node->value()->element.load(&tls_value);
            call_or_returning_void(_op, ret, tls_value);
        }
        return ret;
    }

    // [ThreadSafe] May be called from anywhere.
    ResultTp reset_all_agents() {
        ElementTp prev;
        std::lock_guard guard(_mutex);
        ResultTp tmp = _global_result;
        _global_result = _result_identity;
        for(LinkNode<Agent>* node = _agents.head(); node != _agents.end(); node = node->next()) {
            node->value()->element.exchange(&prev, _element_identity);
            call_or_returning_void(_op, tmp, prev);
        }
        return tmp;
    }

    // Always called from the thread owning the agent.
    void commit_and_erase(Agent* agent) {
        if(!agent) return;
        ElementTp local;
        std::lock_guard guard(_mutex);
        agent->element.load(&local);
        call_or_returning_void(_op, _global_result, local);
        agent->RemoveFromList();
    }

    // Always called from the thread owning the agent.
    void commit_and_clear(Agent* agent) {
        if(!agent) return;
        ElementTp prev;
        std::lock_guard guard(_mutex);
        agent->element.exchange(&prev, _element_identity);
        call_or_returning_void(_op, _global_result, prev);
    }

    // MUST be as fast as possible. 
    inline Agent* get_or_create_tls_agent() {
        Agent* agent = AgentGroup::get_tls_agent(_id);
        if(!agent) {
            // Create new agent.
            agent = AgentGroup::get_or_create_tls_agent(_id);
            if(!agent) {
                LOG_ERROR << "Failed to create agent";
                return nullptr;
            }
        }
        if(agent->combiner) {
            return agent;
        }
        agent->reset(_element_identity, this);
        {
            std::lock_guard guard(_mutex);
            _agents.Append(agent);
        }
        return agent;
    }

    // Reseting agents is MUST beacuse the agent object may be reused.
    // Set element to be default-constructed so that if its' non-pod,
    // internal allocations should be released.
    void clear_all_agents() {
        std::lock_guard guard(_mutex);
        for(LinkNode<Agent>* node = _agents.head(); node != _agents.end(); ) {
            node->value()->reset(ElementTp(), nullptr);
            LinkNode<Agent>* const saved_next = node->next();
            node->RemoveFromList();
            node = saved_next;
        }
    }

    typename var::add_cr_non_integral<ElementTp>::type element_identity() const {
        return _element_identity;
    }
    typename var::add_cr_non_integral<ResultTp>::type result_identity() const {
        return _result_identity;
    }

    const BinaryOp& op() const { return _op; }

    const AgentId& id() const { return _id; }

    bool vaild() const { return _id >= 0; }

    // Overload BinaryOp to Parameter Op.

private:
    AgentId                 _id;
    BinaryOp                _op;
    ResultTp                _global_result;
    ResultTp                _result_identity;
    ElementTp               _element_identity;
    mutable std::mutex      _mutex;
    LinkedList<Agent>       _agents;
};

} // end namespace detail
} // end namespace var


#endif