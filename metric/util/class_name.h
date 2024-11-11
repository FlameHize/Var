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

// Date: Mon Sep 09 14:49:21 CST 2024.

#ifndef VAR_UTIL_CLASS_NAME_H
#define VAR_UTIL_CLASS_NAME_H

#include <cxxabi.h>
#include <stdlib.h>
#include <typeinfo>
#include <string>

namespace var {

// Try to convert mangled |name| to human-readable name.
// Returns:
//   |name|    -  Fail to demangle |name|
//   otherwise -  demangled name
inline std::string demangle(const char* name) {
    // mangled_name
    //   A NULL-terminated character string containing the name to
    //   be demangled.
    // output_buffer:
    //   A region of memory, allocated with malloc, of *length bytes,
    //   into which the demangled name is stored. If output_buffer is
    //   not long enough, it is expanded using realloc. output_buffer
    //   may instead be NULL; in that case, the demangled name is placed
    //   in a region of memory allocated with malloc.
    // length
    //   If length is non-NULL, the length of the buffer containing the
    //   demangled name is placed in *length.
    // status
    //   *status is set to one of the following values:
    //    0: The demangling operation succeeded.
    //   -1: A memory allocation failure occurred.
    //   -2: mangled_name is not a valid name under the C++ ABI
    //       mangling rules.
    //   -3: One of the arguments is invalid.
    int status = 0;
    char* buf = abi::__cxa_demangle(name, NULL, NULL, &status);
    if (status == 0 && buf) {
        std::string s(buf);
        free(buf);
        return s;
    }
    return std::string(name);
}

namespace {
template<typename T> 
struct ClassNameHelper { 
    static std::string name; 
};
template<typename T> 
std::string ClassNameHelper<T>::name = demangle(typeid(T).name());
}

// Get name of class |T|, in std::string.
template<typename T>
const std::string& class_name_str() {
    return ClassNameHelper<T>::name;
}

// Get name of class |T|, in const char*.
// Address of returned name never changes.
template<typename T> const char* class_name() {
    return class_name_str<T>().c_str();
}

// Get typename of |obj|, in std::string.
template<typename T> std::string class_name_str(T const& obj) {
    return demangle(typeid(obj).name());
}

} // end namespace var

#endif // VAR_UTIL_CLASS_NAME_H