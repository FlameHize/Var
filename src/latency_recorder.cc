#include "src/latency_recorder.h"
#include <memory>

namespace var {
namespace detail {

typedef PercentileSamples<1022> CombinedPercentileSamples;

CDF::CDF(PercentileWindow* w) : _w(w) {}

CDF::~CDF() {
    hide();
}

void CDF::describe(std::ostream& os, bool quote_string) const {
    os << "\"click to view\"";
}

int CDF::describe_series(std::ostream& os) const {
    if(!_w) {
        return 1;
    }
    std::unique_ptr<CombinedPercentileSamples> cb(new CombinedPercentileSamples);
    std::vector<GlobalPercentileSamples> buckets;
    _w->get_samples(&buckets);
    cb->combine_of(buckets.begin(), buckets.end());

    std::pair<int, int> values[20];
    size_t n = 0;
    // 10%, 20%, 30%, ..., 90%
    for(int i = 1; i < 10; ++i) {
        values[n++] = std::make_pair(i * 10, cb->get_number(i * 0.1));
    }
    // 91%, 92%, 93%, ..., 99%
    for(int i = 91; i < 100; ++i) {
        values[n++] = std::make_pair(i, cb->get_number(i * 0.01));
    }
    // 99.9% 99.99%
    values[n++] = std::make_pair(100, cb->get_number(0.999));
    values[n++] = std::make_pair(101, cb->get_number(0.9999));

    os << "{\"label\":\"cdf\",\"data\":[";
    for(size_t i = 0; i < n; ++i) {
        if(i) {
            os << ',';
        }
        os << '[' << values[i].first << ',' << values[i].second << ']';
    }
    os << "]}";
    return 0;
}

// Caller is responible for deleting the return value.
static CombinedPercentileSamples* combine(PercentileWindow* w) {
    CombinedPercentileSamples* cb = new CombinedPercentileSamples;
    std::vector<GlobalPercentileSamples> buckets;
    w->get_samples(&buckets);
    cb->combine_of(buckets.begin(), buckets.end());
    return cb;
}

LatencyRecorderBase::LatencyRecorderBase(time_t window_size)
    : _latency_window(&_latency, window_size)
    , _max_latency_window(&_max_latency, window_size)
    , _latency_percentile_window(&_latency_percentile, window_size)
    , _latency_cdf(&_latency_percentile_window) 
    {}

} // end namespace detail

int LatencyRecorder::expose(const std::string& prefix, 
                           const std::string& name) {
    if(name.empty()) {
        LOG_ERROR << "Parameter[name] is empty";
        return -1;
    }
    // User may add "_latency" as the suffix, remove it.
    std::string suffix = name;
    size_t len = suffix.length();
    if(len >= 7) {
        size_t pos = len - 7;
        if(suffix.compare(pos, len, "latency") || 
           suffix.compare(pos, len, "Latency")) {
            suffix.erase(pos);
            if(suffix.empty()) {
                LOG_ERROR << "Invalid name = " << name;
                return -1; 
            }
        }
    }
    std::string tmp;
    if(!prefix.empty()) {
        tmp.reserve(prefix.size() + suffix.size() + 1);
        tmp.append(prefix.data(), prefix.size());
        tmp.push_back('_');
        tmp.append(suffix.data(), suffix.size());
    }

    // set debug names for printing helpful error log.
    _latency.set_debug_name(tmp);
    _latency_percentile.set_debug_name(tmp);

    if(_latency_window.expose_as(tmp, "latency") != 0) {
        return -1;
    }
    if(_max_latency_window.expose_as(tmp, "max_latency") != 0) {
        return -1;
    }
    if(_latency_cdf.expose_as(tmp, "latency_cdf", DISPLAY_ON_HTML) != 0) {
        return -1;
    }
}

void LatencyRecorder::hide() {
    _latency.hide();
    _latency_window.hide();
    _max_latency.hide();
    _max_latency_window.hide();
    _latency_percentile_window.hide();
    _latency_cdf.hide();
}


int64_t LatencyRecorder::latency_percentile(double ratio) const {
    std::unique_ptr<detail::CombinedPercentileSamples> cb(
        combine((detail::PercentileWindow*)&_latency_percentile_window));
    return cb->get_number(ratio);
}

std::ostream& operator<<(std::ostream& os, const LatencyRecorder& latency_recorder) {
    return os << "{latency = " << latency_recorder.latency()
              << " max" << latency_recorder.window_size() 
              << " = " << latency_recorder.max_latency();
}

} // end namespace var