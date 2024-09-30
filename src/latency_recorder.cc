#include "src/latency_recorder.h"
#include <memory>

namespace var {
namespace detail {

const int32_t var_latency_p1 = 80;
const int32_t var_latency_p2 = 90;
const int32_t var_latency_p3 = 99;

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

template<int64_t numerator, int64_t denominator>
static int64_t get_percentile(void* arg) {
    return ((LatencyRecorder*)arg)->latency_percentile(
        (double)numerator / double(denominator));
}

// Used for gflags.
// static int64_t get_p1(void* arg) {
//     LatencyRecorder* lr = static_cast<LatencyRecorder*>(arg);
//     return lr->latency_percentile(var_latency_p1 / 100.0);
// }

// static int64_t get_p2(void* arg) {
//     LatencyRecorder* lr = static_cast<LatencyRecorder*>(arg);
//     return lr->latency_percentile(var_latency_p2 / 100.0);
// }

// static int64_t get_p3(void* arg) {
//     LatencyRecorder* lr = static_cast<LatencyRecorder*>(arg);
//     return lr->latency_percentile(var_latency_p3 / 100.0);
// }

static Vector<int64_t, 4> get_latencies(void* arg) {
    std::unique_ptr<CombinedPercentileSamples> cb(combine((PercentileWindow*)arg));
    // NOTE: We don't show 99.99% since it's often significantly larger than
    // other values and make other curves on the plotted graph small and
    // hard to read.
    Vector<int64_t, 4> result;
    result[0] = cb->get_number(var_latency_p1 / 100.0);
    result[1] = cb->get_number(var_latency_p2 / 100.0);
    result[2] = cb->get_number(var_latency_p3 / 100.0);
    result[3] = cb->get_number(0.999);
    return result;
}

LatencyRecorderBase::LatencyRecorderBase(time_t window_size)
    : _latency_window(&_latency, window_size)
    , _max_latency_window(&_max_latency, window_size)
    , _latency_percentile_window(&_latency_percentile, window_size)
    , _latency_cdf(&_latency_percentile_window) 
    , _latency_p1(get_percentile<var_latency_p1, 100>, this)
    , _latency_p2(get_percentile<var_latency_p2, 100>, this)
    , _latency_p3(get_percentile<var_latency_p3, 100>, this)
    , _latency_999(get_percentile<999,1000>, this)
    , _latency_9999(get_percentile<9999,10000>, this)
    , _latency_percentiles(get_latencies, &_latency_percentile_window)
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

    char namebuf[32];
    snprintf(namebuf, sizeof(namebuf), "latency_%d", (int)detail::var_latency_p1);
    if(_latency_p1.expose_as(prefix, namebuf, DISPLAY_ON_PLAIN_TEXT) != 0) {
        return -1;
    }
    snprintf(namebuf, sizeof(namebuf), "latency_%d", (int)detail::var_latency_p2);
    if(_latency_p2.expose_as(prefix, namebuf, DISPLAY_ON_PLAIN_TEXT) != 0) {
        return -1;
    }
    snprintf(namebuf, sizeof(namebuf), "latency_%u", (int)detail::var_latency_p3);
    if(_latency_p3.expose_as(prefix, namebuf, DISPLAY_ON_PLAIN_TEXT) != 0) {
        return -1;
    }
    if(_latency_999.expose_as(prefix, "latency_999", DISPLAY_ON_PLAIN_TEXT) != 0) {
        return -1;
    }
    if(_latency_9999.expose_as(prefix, "latency_9999", DISPLAY_ON_ALL) != 0) {
        return -1;
    }

    snprintf(namebuf, sizeof(namebuf), "%d%%,%d%%,%d%%,99.9%%",
             (int)detail::var_latency_p1, (int)detail::var_latency_p2,
             (int)detail::var_latency_p3);
    _latency_percentiles.set_vector_names(namebuf);
    if(_latency_percentiles.expose_as(prefix, "latency_percentiles", DISPLAY_ON_HTML) != 0) {
        return -1;
    }
    return 0;
}

void LatencyRecorder::hide() {
    _latency.hide();
    _latency_window.hide();
    _max_latency.hide();
    _max_latency_window.hide();
    _latency_percentile_window.hide();
    _latency_cdf.hide();
    _latency_p1.hide();
    _latency_p2.hide();
    _latency_p3.hide();
    _latency_999.hide();
    _latency_9999.hide();
}

int64_t LatencyRecorder::latency_percentile(double ratio) const {
    std::unique_ptr<detail::CombinedPercentileSamples> cb(
        combine((detail::PercentileWindow*)&_latency_percentile_window));
    return cb->get_number(ratio);
}

Vector<int64_t, 4> LatencyRecorder::latency_percentiles() const {
    ///@bug _latency_percentile has no data.
    var::detail::GlobalPercentileSamples g = _latency_percentile.get_value();
    uint32_t total = 0;
    for(size_t i = 0; i < var::detail::NUM_INTERVALS; ++i) {
        total += g.get_interval_at(i).added_count();
    }
    LOG_INFO << "added count: " << total;

    return detail::get_latencies(
        const_cast<detail::PercentileWindow*>(&_latency_percentile_window));
}

std::ostream& operator<<(std::ostream& os, const LatencyRecorder& latency_recorder) {
    return os << "{latency = " << latency_recorder.latency()
              << " max" << latency_recorder.window_size() 
              << " = " << latency_recorder.max_latency();
}

} // end namespace var