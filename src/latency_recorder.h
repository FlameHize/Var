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

// Date Sun Sep 29 11:05:00 CST 2024.

#ifndef VAR_LATENCY_RECORDER_H
#define VAR_LATENCY_RECORDER_H

#include "src/reducer.h"
#include "src/average_recorder.h"
#include "src/passive_status.h"
#include "src/detail/percentile.h"

namespace var {
namespace detail {

class Percentile;
// SERIES_IN_SECOND: Reflact the changes of input data per second.
// Record the average latency per second.
typedef Window<AverageRecorder, SERIES_IN_SECOND> AverageWindow;
// Record the max latency per second.
typedef Window<Maxer<int64_t>, SERIES_IN_SECOND> MaxWindow;
// Record the situation of adding data percentile values per second.
typedef Window<Percentile, SERIES_IN_SECOND> PercentileWindow;  


class CDF : public Variable {
public:
    explicit CDF(PercentileWindow* w);
    ~CDF();
    void describe(std::ostream& os, bool quote_string) const override;
    int describe_series(std::ostream& os) const override;
private:
    PercentileWindow* _w;
};

class LatencyRecorderBase {
public:
    explicit LatencyRecorderBase(time_t window_size);
    time_t window_size() const {
        return _latency_window.window_size();
    }

protected:
    AverageRecorder                     _latency;
    AverageWindow                       _latency_window;

    Maxer<int64_t>                      _max_latency;
    MaxWindow                           _max_latency_window;

    Percentile                          _latency_percentile;
    PercentileWindow                    _latency_percentile_window;
    CDF                                 _latency_cdf;

    PassiveStatus<int64_t>              _latency_p1;
    PassiveStatus<int64_t>              _latency_p2;
    PassiveStatus<int64_t>              _latency_p3;
    PassiveStatus<int64_t>              _latency_999;
    PassiveStatus<int64_t>              _latency_9999;

    PassiveStatus<Vector<int64_t, 4>>   _latency_percentiles;
};
} // end namespace detail

// Specialized structure to record latency.
// It's not a Variable, but it contains multiple var inside.
class LatencyRecorder : public detail::LatencyRecorderBase {
    typedef detail::LatencyRecorderBase Base;
public:
    LatencyRecorder() : Base(-1) {}
    explicit LatencyRecorder(time_t window_size) : Base(window_size) {}
    explicit LatencyRecorder(const std::string& prefix) : Base(-1) {
        expose(prefix);
    }
    LatencyRecorder(const std::string& prefix,
                    time_t window_size) : Base(window_size) {
        expose(prefix);
    }
    LatencyRecorder(const std::string& prefix,
                    const std::string& name) : Base(-1) {
        expose(prefix, name);
    }
    LatencyRecorder(const std::string& prefix,
                    const std::string& name,
                    time_t window_size) : Base(window_size) {
        expose(prefix, name);
    }
    ~LatencyRecorder() { hide(); }

    // Returns 0 on success, -1 otherwise.
    // Example:
    //   LatencyRecorder rec;
    //   rec.expose("foo_bar_write");     // foo_bar_write_latency
    //                                    // foo_bar_write_max_latency
    //                                    // foo_bar_write_count
    //                                    // foo_bar_write_qps
    //   rec.expose("foo_bar", "read");   // foo_bar_read_latency
    //                                    // foo_bar_read_max_latency
    //                                    // foo_bar_read_count
    //                                    // foo_bar_read_qps
    int expose(const std::string& name) {
        return expose(std::string(), name);
    }
    int expose(const std::string& prefix, const std::string& name);

    // Hide all internal variables, called in dtor as well.
    void hide();

    // Record the latency
    inline LatencyRecorder& operator<<(int64_t latency) {
        // _latency << latency;
        // _max_latency << latency;
        _latency_percentile << latency;
        return *this;
    }

    // Get the average latency in recent |window_size| seconds.
    int64_t latency(time_t window_size) const {
        return _latency_window.get_value(window_size).get_average_int();
    }
    int64_t latency() const {
        return _latency_window.get_value().get_average_int();
    }

    // Get the max latency in recent window_size seconds.
    int64_t max_latency() const {
        return _max_latency_window.get_value();
    }

    // Get |ratio|-ile latency in recent |window_size| seconds.
    int64_t latency_percentile(double ratio) const;

    // Get p1/p2/p3/99.9 -ile latencies in recent |window_size| seconds.
    Vector<int64_t, 4> latency_percentiles() const;

    // Get name of a sub-var.
    const std::string& latency_name() const { 
        return _latency_window.name(); 
    }
    const std::string& max_latency_name() const {
        return _max_latency_window.name();
    }
    const std::string& latency_cdf_name() const {
        return _latency_cdf.name();
    }
    const std::string& latency_percentiles_name() const {
        return _latency_percentiles.name();
    }
};

std::ostream& operator<<(std::ostream& os, const LatencyRecorder& latency_recorder);

} // end namespace var
#endif // VAR_LATENCY_RECORDER_H