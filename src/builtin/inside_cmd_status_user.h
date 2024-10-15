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

#include <string>
#include <vector>

namespace var {

struct KeyInfo {
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
};

struct ChipInfo {
public:
    std::string             label;
    int                     key_num;
    int                     field_byte;
    std::vector<KeyInfo>    _key_info_list;
};

// InsideCmdStatusUser("fbb", 8); // fbb是获取上级目录的名称得到的
class InsideCmdStatusUser {
public: 
    int parse(const std::string& path);

public:
    std::vector<ChipInfo> _chip_info_list;
};

} // end namespace var

#endif // VAR_BUILTIN_INSIDE_CMD_STATUS_USER_H