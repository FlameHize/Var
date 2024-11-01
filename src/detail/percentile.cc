#include "src/detail/percentile.h"

namespace var {
namespace detail {

inline uint32_t ones32(uint32_t x) {
    /* 32-bit recursive reduction using SWAR...
     * but first step is mapping 2-bit values
     * into sum of 2 1-bit values in sneaky way
     */
    x -= ((x >> 1) & 0x55555555);
    x = (((x >> 2) & 0x33333333) + (x & 0x33333333));
    x = (((x >> 4) + x) & 0x0f0f0f0f);
    x += (x >> 8);
    x += (x >> 16);
    return (x & 0x0000003f);
}

inline uint32_t log2(uint32_t x) {
    int y = (x & (x - 1));
    y |= -y;
    y >>= 31;
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    return(ones32(x) - 1 - y);
}

inline size_t get_interval_index(int64_t &x) {
    if (x <= 2) {
        return 0;
    } else if (x > std::numeric_limits<uint32_t>::max()) {
        x = std::numeric_limits<uint32_t>::max();
        return 31;
    } else {
        return log2(x) - 1;
    }
}

// Special a new Op to handle the merge process
// from functon parameters(like int64_t latency) 
// to ElementTp(ThreadLocalPercentleSamples)
// and then to ResultTp(GlobalPercentileSamples).
class AddLatency {
public:
    AddLatency(int64_t latency) : _latency(latency) {}

    // global_value: wrapper for Combiner's ResultTp _global_result
    // local_value: Agent's ElementTp _value
    void operator()(GlobalValue<Percentile::combiner_type>& global_value,
                    ThreadLocalPercentileSamples& local_value) const {
        // Copy to latency since get_interval_index may change input.
        int64_t latency = _latency;
        const size_t index = get_interval_index(latency);
        PercentileInterval<ThreadLocalPercentileSamples::SAMPLE_SIZE>&
            interval = local_value.get_interval_at(index);
        if(interval.full()) {
            GlobalPercentileSamples* g = global_value.lock();
            g->get_interval_at(index).merge(interval);
            g->_num_added += interval.added_count();
            global_value.unlock();
            // Now allowed to change local_value.
            local_value._num_added -= interval.added_count();
            interval.clear();
        }
        interval.add64(latency);
        ++local_value._num_added;
    }

private:
    int64_t _latency;
};

Percentile::Percentile() : _combiner(nullptr), _sampler(nullptr) {
    _combiner = new combiner_type;
}

Percentile::~Percentile() {
    // Have to destroy sampler first to avoid the race between destruction and
    // sampler
    if (_sampler != nullptr) {
        _sampler->destroy();
        _sampler = nullptr;
    }
    delete _combiner;
}

Percentile::value_type Percentile::get_value() const {
    return _combiner->combine_agents();
}

Percentile& Percentile::operator<<(int64_t latency) {
    agent_type* agent = _combiner->get_or_create_tls_agent();
    if(VAR_UNLIKELY(!agent)) {
        LOG_ERROR << "Fail to create agent";
        return *this;
    }
    if (latency < 0) {
        // we don't check overflow(of uint32) in percentile because the
        // overflowed value which is included in last range does not affect
        // overall distribution of other values too much.
        if (!_debug_name.empty()) {
            LOG_WARN << "Input=" << latency << " to `" << _debug_name
                     << "' is negative, drop";
        } else {
            LOG_WARN << "Input=" << latency << " to Percentile("
                     << (void*)this << ") is negative, drop";
        }
        return *this;
    }
    agent->merge_global(AddLatency(latency));
    return *this;
}

Percentile::value_type Percentile::reset() {
    return _combiner->reset_all_agents();
}

} // end namespace detail
} // end namespace var