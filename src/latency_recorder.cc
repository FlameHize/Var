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

} // end namespace detail
} // end namespace var