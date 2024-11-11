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

// Date: Mon Sep 09 15:06:40 CST 2024

#include "metric/detail/sampler.h"
#include "metric/util/singleton.h"
#include "metric/reducer.h"
#include "net/base/Logging.h"
#include <unistd.h>
#include <sys/prctl.h>

namespace var {
namespace detail {

const int WARN_NOSLEEP_THRESHOLD = 2;

// Combine two circular linked list into one.
struct CombineSampler {
    void operator()(Sampler*& s1, Sampler* s2) const {
        if(!s2) return;
        if(!s1) {
            s1 = s2;
            return;
        }
        s1->InsertBeforeAsList(s2);
    }
};

// True if pthread_atfork was called. The callback to atfork works for
// child of child as well, no need to register in the child again.
static bool registered_atfork = false;

// Call take_sample() of all scheduled samplers.
// This can be done with regular time thread, but it's way too slow(global
// contention + log(N) heap manipulations). We need it to be super fast
// so that creation overhead of Window<> is negliable.
// The trick is to use Reducer<Sampler*, CombineSampler>. Each Sampler
// is double linked, thus we can reduce multiple Samplers into one circularly
// double linked list, and multiple lists into large lists. We create a 
// dedicated thread to periodically get_value() which is just the combined
// list of Samplers. Waking through the list and call take_sample().
// If a Sampler needs to be deleted, we just mark it as unused and the
// deletion is taken place in the thread as well.
class SamplerCollector : public Reducer<Sampler*, CombineSampler> {
public:
    SamplerCollector()
        : _created(false)
        , _stop(false)
        , _cumulated_time_us(0) {
        create_sampling_thread();
    }
    ~SamplerCollector() {
        if(_created) {
            _stop = true;
            pthread_join(_tid, NULL);
            _created = false;
        }
    }

private:
    void create_sampling_thread() {
        const int rc = pthread_create(&_tid, NULL, sampling_thread, this);
        if(rc != 0) {
            LOG_ERROR << "Fail to create sampling_thread, error: " << rc;
        }
        else {
            _created = true;
            if(!registered_atfork) {
                registered_atfork = true;
                pthread_atfork(NULL, NULL, child_callback_atfork);
            }
        }
    }

    static void* sampling_thread(void* arg) {
        static_cast<SamplerCollector*>(arg)->run();
        return nullptr;
    }

    static double get_cumulated_time(void* arg) {
        return static_cast<SamplerCollector*>(arg)->_cumulated_time_us / 1000.0 / 1000.0;
    }

    // Support for fork:
    // * The singleton can be null before forking, the child callback will not
    //   be registered.
    // * If the singleton is not null before forking, the child callback will
    //   be registered and the sampling thread will be re-created.
    // * A forked program can be forked again.
    static void child_callback_atfork() {
        var::get_singleton<SamplerCollector>()->after_forked_as_child();
    }

    void after_forked_as_child() {
        _created = false;
        create_sampling_thread();
    }

    void run() {
        ::prctl(PR_SET_NAME, "sampler thread");
        ::usleep(10000);
        LinkNode<Sampler> root;
        int consecutice_nosleep = 0;
        while(!_stop) {
            int64_t abstime = gettimeofday_us();
            // Insert new adding Samplers double linked list.
            // Samplers which already exists has reset to be NULL within
            // previous process. With CombineSampler operator, s2(NULL)
            // will not be added into linked list in this Sampler s.
            Sampler* s = this->reset();
            if(s) {
                s->InsertBeforeAsList(&root);
            }

            for(LinkNode<Sampler>* p = root.next(); p != &root; ) {
                // We may remove p from the list, save next first.
                LinkNode<Sampler>* saved_next = p->next();
                Sampler* s = p->value();
                s->_mutex.lock();
                if(s->_used) {
                    s->take_sample();
                    s->_mutex.unlock();
                }
                else {
                    // Sampler's is stop, no race-condition.
                    s->_mutex.unlock();
                    p->RemoveFromList();
                    delete s;
                }
                p = saved_next;
            }
            bool slept = false;
            int64_t now = gettimeofday_us();
            _cumulated_time_us += now - abstime;
            abstime += 1000000L;
            while(abstime > now) {
                ::usleep(abstime - now);
                slept = true;
                now = gettimeofday_us();
            }
            if(slept) {
                consecutice_nosleep = 0;
            }
            else {
                if(++consecutice_nosleep >= WARN_NOSLEEP_THRESHOLD) {
                    consecutice_nosleep = 0;
                    LOG_WARN << "var is busy ar sampling for "
                             << WARN_NOSLEEP_THRESHOLD << " seconds!";
                }
            }
        }
    }

private:
    bool _created;
    bool _stop;
    int64_t _cumulated_time_us;
    pthread_t _tid;
};

Sampler::Sampler() : _used(true) {}

Sampler::~Sampler() {}

void Sampler::schedule() {
    *var::get_singleton<SamplerCollector>() << this;
}

void Sampler::destroy() {
    _mutex.lock();
    _used = false;
    _mutex.unlock();
}

} // end namespace detail
} // end namespace var