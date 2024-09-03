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

#ifndef VAR_COMMON_H
#define VAR_COMMON_H

#include "net/base/StringSplitter.h"

#include <vector>
#include <set>
#include <streambuf>
#include <cstring>
#include <mutex>

// Compiler detection.
#if defined(__GNUC__)
#define COMPILER_GCC 1
#elif defined(_MSC_VER)
#define COMPILER_MSVC 1
#else
#error Please add support for your compiler in butil/build_config.h
#endif

// Mark a branch likely or unlikely to be true.
// We can't remove the BAIDU_ prefix because the name is likely to conflict,
// namely kylin already has the macro.
#if defined(COMPILER_GCC)
#  if defined(__cplusplus)
#    define VAR_LIKELY(expr) (__builtin_expect((bool)(expr), true))
#    define VAR_UNLIKELY(expr) (__builtin_expect((bool)(expr), false))
#  else
#    define VAR_LIKELY(expr) (__builtin_expect(!!(expr), 1))
#    define VAR_UNLIKELY(expr) (__builtin_expect(!!(expr), 0))
#  endif
#else
#  define VAR_LIKELY(expr) (expr)
#  define VAR_UNLIKELY(expr) (expr)
#endif

namespace var {

// [ Ease getting first/last character of std::string before C++11 ]
// return the first/last character, UNDEFINED when the string is empty.
inline char front_char(const std::string& s) { return s[0]; }
inline char back_char(const std::string& s) { return s[s.size() - 1]; }

// Underlying buffer to store logs. Comparing to using std::ostringstream
// directly, this utility exposes more low-level methods so that we avoid
// creation of std::string which allocates memory internally.
class CharArrayStreamBuf : public std::streambuf {
public:
    explicit CharArrayStreamBuf() : _data(NULL), _size(0) {}
    ~CharArrayStreamBuf() {
        free(_data);
    }

    int overflow(int ch) override {
        if (ch == std::streambuf::traits_type::eof()) {
            return ch;
        }
        size_t new_size = std::max(_size * 3 / 2, (size_t)64);
        char* new_data = (char*)malloc(new_size);
        if (VAR_UNLIKELY(new_data == NULL)) {
            setp(NULL, NULL);
            return std::streambuf::traits_type::eof();
        }
        memcpy(new_data, _data, _size);
        free(_data);
        _data = new_data;
        const size_t old_size = _size;
        _size = new_size;
        setp(_data, _data + new_size);
        pbump(old_size);
        // if size == 1, this function will call overflow again.
        return sputc(ch);
    }

    int sync() override {
        // data are already there.
        return 0;
    }

    void reset() {
        setp(_data, _data + _size);
    }
    
    std::string data() {
        return std::string(pbase(), pptr() - pbase());
    }

private:
    char* _data;
    size_t _size;
};

// Written by Jack Handy
// <A href="mailto:jakkhandy@hotmail.com">jakkhandy@hotmail.com</A>
inline bool wildcmp(const char* wild, const char* str, char question_mark) {
    const char* cp = NULL;
    const char* mp = NULL;

    while (*str && *wild != '*') {
        if (*wild != *str && *wild != question_mark) {
            return false;
        }
        ++wild;
        ++str;
    }

    while (*str) {
        if (*wild == '*') {
            if (!*++wild) {
                return true;
            }
            mp = wild;
            cp = str+1;
        } else if (*wild == *str || *wild == question_mark) {
            ++wild;
            ++str;
        } else {
            wild = mp;
            str = cp++;
        }
    }

    while (*wild == '*') {
        ++wild;
    }
    return !*wild;
}

class WildcardMatcher {
public:
    WildcardMatcher(const std::string& wildcards,
                    char question_mark,
                    bool on_both_empty)
        : _question_mark(question_mark)
        , _on_both_empty(on_both_empty) {
        if (wildcards.empty()) {
            return;
        }
        std::string name;
        const char wc_pattern[3] = { '*', question_mark, '\0' };
        for (StringMultiSplitter sp(wildcards.c_str(), ",;"); sp != NULL; ++sp) {
            name.assign(sp.field(), sp.length());
            if (name.find_first_of(wc_pattern) != std::string::npos) {
                if (_wcs.empty()) {
                    _wcs.reserve(8);
                }
                _wcs.push_back(name);
            } else {
                _exact.insert(name);
            }
        }
    }
    
    bool match(const std::string& name) const {
        if (!_exact.empty()) {
            if (_exact.find(name) != _exact.end()) {
                return true;
            }
        } else if (_wcs.empty()) {
            return _on_both_empty;
        }
        for (size_t i = 0; i < _wcs.size(); ++i) {
            if (wildcmp(_wcs[i].c_str(), name.c_str(), _question_mark)) {
                return true;
            }
        }
        return false;
    }

    const std::vector<std::string>& wildcards() const { return _wcs; }
    const std::set<std::string>& exact_names() const { return _exact; }

private:
    char _question_mark;
    bool _on_both_empty;
    std::vector<std::string> _wcs;
    std::set<std::string> _exact;
};

} // end namespace var

#endif // VAR_COMMON_H