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

// Date Nov Thus 26 09:04:55 CST 2024.

#ifndef VAR_BUILTIN_METHOD_STATUS_H
#define VAR_BUILTIN_METHOD_STATUS_H

#include "metric/var.h"
#include "metric/util/class_name.h"

namespace var {

// May be no need.
struct DescribeOptions {
    DescribeOptions() : verbose(true), use_html(false) {}
    bool verbose;
    bool use_html;
};

class Describable {
public:
    virtual ~Describable() {}
    virtual void Describe(std::ostream& os, const DescribeOptions&) const {
        os << class_name_str(*this);
    }
};

inline std::ostream& operator<<(std::ostream& os, const Describable& obj) {
    DescribeOptions options;
    options.verbose = false;
    obj.Describe(os, options);
    return os;
}

// Record accessing stats of a method.
class MethodStatus : public Describable {

};

} // end namespace var

#endif // VAR_BUILTIN_METHOD_STATUS_H