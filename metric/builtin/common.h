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

// Date Oct Tue 08 18:48:00 CST 2024.

#ifndef VAR_BUILTIN_COMMON_H
#define VAR_BUILTIN_COMMON_H

#include "net/http/http_header.h"

namespace var {

// These static strings are referenced more than once in var.
// Don't turn them to std::strings whose constructing sequences are undefined.
const char* const UNKNOWN_METHOD_STR = "unknown_method";
const char* const CONSOLE_STR = "console";
const char* const USER_AGENT_STR = "user-agent";

bool UseHTML(const net::HttpHeader& header);

std::string UrlEncode(const std::string& value);
std::string UrlDecode(const std::string& value);

std::string double_to_string(double value, int decimal);
std::string decimal_to_hex(int decimal);
std::string decimal_to_binary(int decimal);

std::string format_byte_size(size_t value);

std::string program_work_dir(const std::string& filter_word);

// Put inside <head></head> of html to work with Tabbed.
const char* TabsHead();

// The logo ascii art.
const char* logo();

} // end namespace var

#endif // VAR_BUILTIN_COMMON_H