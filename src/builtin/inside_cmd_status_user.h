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

// Date Oct Tues 15 10:20:56 CST 2024.

#ifndef VAR_BUILTIN_INSIDE_CMD_STATUS_USER_H
#define VAR_BUILTIN_INSIDE_CMD_STATUS_USER_H

#include "src/util/tinyxml2.h"
#include "net/base/Logging.h"
#include <iomanip>
#include <string>
#include <vector>
#include <ostream>

namespace var {

struct KeyInfo {
public:
    void describe(const char* data, size_t len,
                  std::ostream& os, bool use_html);
                  
    std::string name;
    int         type;
    int         byte;
    bool        sign;
    int         default_value;
    int         enable;
    double      unit;
    double      offset;
    int         precesion;
    std::string dimension;

    size_t      resolved_addr;
};

struct ChipInfo {
public:
    void describe(const char* data, size_t len,
                  std::ostream& os, bool use_html);

public:
    std::string             label;
    int                     key_num;
    int                     field_byte;
    std::vector<KeyInfo>    key_info_list;

    size_t                  index;
};

class InsideCmdStatusUser {
public: 
    InsideCmdStatusUser(const std::string& user_name, size_t user_id);

    int parse(const std::string& path);
    int parse(const char* data, size_t len);

    void describe(const char* data, size_t len,
                  std::ostream& os, bool use_html);

    std::string name() const {
        return _user_name;
    }
    size_t id() const {
        return _user_id;
    }

    const std::vector<ChipInfo>& chip_group() const {
        return _chip_info_list;
    }

private:
    int parse_internal(tinyxml2::XMLDocument& doc);

private:
    std::vector<ChipInfo> _chip_info_list;

    std::string           _user_name;
    size_t                _user_id;
};

} // end namespace var

#endif // VAR_BUILTIN_INSIDE_CMD_STATUS_USER_H