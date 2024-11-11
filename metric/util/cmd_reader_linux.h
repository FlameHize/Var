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

// Date Oct Tue 08 14:44:23 CST 2024.

#ifndef VAR_UTIL_CMD_READER_LINUX_H
#define VAR_UTIL_CMD_READER_LINUX_H

#include "metric/util/fd_guard.h"
#include "net/base/Logging.h"

#include <fcntl.h>                      // open
#include <stdio.h>                      // snprintf
#include <sys/types.h>  
#include <sys/uio.h>
#include <unistd.h>                     // read, gitpid
#include <sstream>                      // std::ostringstream

namespace var {

ssize_t ReadCommandLine(char* buf, size_t len, bool with_args) {
    fd_guard fd(open("/proc/self/cmdline", O_RDONLY));
    if(fd < 0) {
        LOG_ERROR << "Fail to open /proc/self/cmdline";
        return -1;
    }
    ssize_t nr = read(fd, buf, len);
    if(nr <= 0) {
        LOG_ERROR << "Fail to read /proc/self/cmdline";
        return -1;
    }
    if(with_args) {
        if((size_t)nr == len) {
            return len;
        }
        for(ssize_t i = 0; i < nr; ++i) {
            if(buf[i] == '\0') {
                buf[i] = '\n';
            }
        }
        return nr;
    } 
    else {
        for(ssize_t i = 0; i < nr; ++i) {
            // The command in macos is separated with space and ended with '\n'
            if(buf[i] == '\0' || buf[i] == '\n' || buf[i] == ' ') {
                return i;
            }
        }
        if((size_t)nr == len) {
            LOG_ERROR << "buf is not big enough";
            return -1;
        }
        return nr;
    }
}

ssize_t ReadCommandOutput(std::ostream& os, const char* cmd) {
    FILE* pipe = popen(cmd, "r");
    if (pipe == NULL) {
        return -1;
    }
    char buffer[1024];
    for(;;) {
        size_t nr = fread(buffer, 1, sizeof(buffer), pipe);
        if(nr != 0) {
            os.write(buffer, nr);
        }
        if(nr != sizeof(buffer)) {
            if(feof(pipe)) {
                break;
            } 
            else if(ferror(pipe)) {
                LOG_ERROR << "Encountered error while reading for the pipe";
                break;
            }
            // retry;
        }
    }

    const int wstatus = pclose(pipe);

    if(wstatus < 0) {
        return wstatus;
    }
    if(WIFEXITED(wstatus)) {
        return WEXITSTATUS(wstatus);
    }
    if(WIFSIGNALED(wstatus)) {
        os << "Child process was killed by signal "
           << WTERMSIG(wstatus);
    }
    errno = ECHILD;
    return -1;
}

} // end namespace var

#endif // VAR_UTIL_CMD_READER_LINUX_H