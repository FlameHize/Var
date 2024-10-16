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
#include <ostream>
#include "src/builtin/tabbed.h"

namespace var {

struct KeyInfo {
public:
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

struct ChipInfo : public Tabbed {
public:
    void describe(const char* data, size_t len,
                  std::ostream& os, bool use_html);

    void GetTabInfo(TabInfoList*) const override;

public:
    std::string             label;
    int                     key_num;
    int                     field_byte;
    std::vector<KeyInfo>    key_info_list;

    size_t                  index;
};

// InsideCmdStatusUser("fbb", 8); // fbb是获取上级目录的名称得到的
// 8 -> (12) 8 9 10 11 12-> 各Variable解析地址
class InsideCmdStatusUser {
public: 
    int parse(const std::string& path, size_t chip_group_index);

    // 在输出时，依据key_info的各种属性，进行文字输出
    void describe(const char* data, size_t len,
                  std::ostream& os, bool use_html);

private:
    std::vector<ChipInfo> _chip_info_list;
    size_t                _chip_group_index;
    TabInfoList           _tab_info_list;
};

} // end namespace var

#endif // VAR_BUILTIN_INSIDE_CMD_STATUS_USER_H