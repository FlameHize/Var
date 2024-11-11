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

// Date Oct Tue 08 14:32:11 CST 2024.

#ifndef VAR_UTIL_FILE_READER_LINUX_H
#define VAR_UTIL_FILE_READER_LINUX_H

#include "metric/util/move.h"
#include <stdio.h>

namespace var {

//// Automatically closes |FILE*|s.
class FileReaderLinux {
    MOVE_ONLY_TYPE_FOR_CPP_03(FileReaderLinux, RValue);
public:
    FileReaderLinux() : _fp(NULL) {}

    // Open file at |path| with |mode|.
    // If fopen failed, operator FILE* returns NULL and errno is set.
    FileReaderLinux(const char *path, const char *mode) {
        _fp = fopen(path, mode);
    }

    // |fp| must be the return value of fopen as we use fclose in the
    // destructor, otherwise the behavior is undefined
    explicit FileReaderLinux(FILE *fp) :_fp(fp) {}

    FileReaderLinux(RValue rvalue) {
        _fp = rvalue.object->_fp;
        rvalue.object->_fp = NULL;
    }

    ~FileReaderLinux() {
        if (_fp != NULL) {
            fclose(_fp);
            _fp = NULL;
        }
    }

    // Close current opened file and open another file at |path| with |mode|
    void reset(const char* path, const char* mode) {
        reset(fopen(path, mode));
    }

    void reset() { reset(NULL); }

    void reset(FILE *fp) {
        if (_fp != NULL) {
            fclose(_fp);
            _fp = NULL;
        }
        _fp = fp;
    }

    // Set internal FILE* to NULL and return previous value.
    FILE* release() {
        FILE* const prev_fp = _fp;
        _fp = NULL;
        return prev_fp;
    }
    
    operator FILE*() const { return _fp; }

    FILE* get() { return _fp; }
    
private:
    FILE* _fp;
};

} // end namespace var

#endif // VAR_UTIL_FILE_READER_LINUX_H