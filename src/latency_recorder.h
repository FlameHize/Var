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
// Record the situation of adding data percentile values every second.
typedef Window<Percentile, SERIES_IN_WINDOW> PercentileWindow;  


class CDF : public Variable {
public:
    explicit CDF(PercentileWindow* w);
    ~CDF();
    void describe(std::ostream& os, bool quote_string) const override;
    int describe_series(std::ostream& os) const override;
private:
    PercentileWindow* _w;
};

} // end namespace detail


} // end namespace var
#endif // VAR_LATENCY_RECORDER_H