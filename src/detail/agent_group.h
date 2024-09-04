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

#ifndef VAR_DETAIL_AGENT_GROUP_H
#define VAR_DETAIL_AGENT_GROUP_H

#include "src/common.h"
#include "net/base/Logging.h"
#include "net/base/Mutex.h"
#include <pthread.h>
#include <deque>
#include <vector>

namespace var {
namespace detail {

typedef int AgentId;

// General NOTES:
// * Don't use bound-checking vector::at.
// * static functions in template class are not guaranteed to be inlined,
//   add inline keyword explicitly.
// * Put fast path in "if" branch, which is more cpu-wise.
// * don't use __builtin_expect excessively because CPU may predict the branch
//   better than you. Only hint branches that are definitely unusual.

template<typename Agent>
class AgentGroup {
public:
    // Purpose to get template param 'Agent' type used for ::.
    typedef Agent agent_type;

    // TODO: We should remove the template parameter and unify AgentGroup
    // of all bvar with a same one, to reuse the memory between different
    // type of bvar. The unified AgentGroup allocates small structs in-place
    // and large structs on heap, thus keeping batch efficiencies on small
    // structs and improving memory usage on large structs.
    const static size_t CACHE_LINE_SIZE = 64;
    const static size_t RAW_BLOCK_SIZE = 4096;
    const static size_t ELEMENTS_PER_BLOCK = 
        (RAW_BLOCK_SIZE  + sizeof(Agent) - 1) / sizeof(Agent);

    // The most generic method to allocate agents is to call ctor when
    // agent is needed, however we construct all agents when initializing
    // ThreadBlock, which has side effects:
    //  * calling ctor ELEMENTS_PER_BLOCK times is slower.
    //  * calling ctor of non-pod types may be unpredictably slow.
    //  * non-pod types may allocate space inside ctor excessively.
    //  * may return non-null for unexist id.
    //  * lifetime of agent is more complex. User has to reset the agent before
    //    destroying id otherwise when the agent is (implicitly) reused by
    //    another one who gets the reused id, things are screwed.
    // TODO(chenzhangyi01): To fix these problems, a method is to keep a bitmap
    // along with ThreadBlock* in _s_tls_blocks, each bit in the bitmap
    // represents that the agent is constructed or not. Drawback of this method
    // is that the bitmap may take 32bytes (for 256 agents, which is common) so
    // that addressing on _s_tls_blocks may be slower if identifiers distribute
    // sparsely. Another method is to put the bitmap in ThreadBlock. But this
    // makes alignment of ThreadBlock harder and to address the agent we have
    // to touch an additional cacheline: the bitmap. Whereas in the first
    // method, bitmap and ThreadBlock* are in one cacheline.
    struct VAR_CACHELINE_ALIGNMENT ThreadBlock {
        inline Agent* at(size_t offset) { return _agents + offset; }
    private:
        Agent _agents[ELEMENTS_PER_BLOCK];
    };

    inline ~AgentGroup() {
        _destroy_tls_blocks();
    }

    inline static AgentId create_new_agent() {
        MutexLockGuard lock(_s_mutex);
        AgentId agent_id = 0;
        if(!_get_free_ids().empty()) {
            agent_id = _get_free_ids().back();
            _get_free_ids().pop_back();
        }
        else {
            agent_id = _s_agent_kinds++;
        }
        return agent_id;
    }

    inline static int destroy_agent(AgentId id) {
        MutexLockGuard lock(_s_mutex);
        if(id < 0 || id >= _s_agent_kinds) {
            errno = EINVAL;
            return -1;
        }
        _get_free_ids().push_back(id);
        return 0;
    }

    // NOTE: May return non-null for unexist id, see notes on ThreadBlock
    // We need this function to be as fast as possible.
    inline static Agent* get_tls_agent(AgentId id) {
        if(VAR_LIKELY(id >= 0)) {
            if(_s_tls_blocks) {
                const size_t block_id = (size_t)id / ELEMENTS_PER_BLOCK;
                if(block_id < _s_tls_blocks->size()) {
                    ThreadBlock* const tb = (*_s_tls_blocks)[block_id];
                    if(tb) {
                        return tb->at(id - block_id * ELEMENTS_PER_BLOCK);
                    }
                }
            }
        }
        return nullptr;
    }

    // NOTE: May return non-null for unexist id, see notes on ThreadBlock.
    inline static Agent* get_or_create_tls_agent(AgentId id) {
        if(VAR_UNLIKELY(id < 0)) {
            LOG_ERROR << "Invaild id = " << id;
            return nullptr;
        }
        if(!_s_tls_blocks) {
            _s_tls_blocks = new (std::nothrow) std::vector<ThreadBlock*>();
            if(VAR_UNLIKELY(!_s_tls_blocks)) {
                LOG_ERROR << "Fail to create tls blocks ";
                return nullptr;
            }
        }
        const size_t block_id = (size_t)id / ELEMENTS_PER_BLOCK;
        if(block_id >= _s_tls_blocks->size()) {
            _s_tls_blocks->resize(std::max(block_id + 1, 32ul));
        }
        ThreadBlock* tb = (*_s_tls_blocks)[block_id];
        if(!tb) {
            ThreadBlock* new_block = new (std::nothrow) ThreadBlock;
            if(VAR_UNLIKELY(!new_block)) {
                return nullptr;
            }
            tb = new_block;
            (*_s_tls_blocks)[block_id] = new_block;
        }
        return tb->at(id - block_id * ELEMENTS_PER_BLOCK);
    }

private:
    static void _destroy_tls_blocks() {
        if(!_s_tls_blocks) {
            return;
        }
        for(size_t i = 0; i < _s_tls_blocks->size(); ++i) {
            delete (*_s_tls_blocks)[i];
        }
        delete _s_tls_blocks;
        _s_tls_blocks = nullptr;
    }
    
    inline static std::deque<AgentId>& _get_free_ids() {
        if(VAR_UNLIKELY(!_s_free_ids)) {
             _s_free_ids = new (std::nothrow) std::deque<AgentId>();
        }
        return *_s_free_ids;
    }

    static MutexLock                            _s_mutex;
    static AgentId                              _s_agent_kinds;
    static std::deque<AgentId>*                 _s_free_ids;
    static __thread std::vector<ThreadBlock*>*  _s_tls_blocks;
};

template<typename Agent>
MutexLock AgentGroup<Agent>::_s_mutex = MutexLock();

template<typename Agent>
AgentId AgentGroup<Agent>::_s_agent_kinds = 0;

template<typename Agent>
std::deque<AgentId>* AgentGroup<Agent>::_s_free_ids = nullptr;

template<typename Agent>
__thread std::vector<typename AgentGroup<Agent>::ThreadBlock*>*
AgentGroup<Agent>::_s_tls_blocks = nullptr;

} // end namespace detail
} // end namespace var

#endif // VAR_DETAIL_AGENT_GROUP_H