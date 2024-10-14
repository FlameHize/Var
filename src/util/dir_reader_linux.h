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

// Date Oct Tue 08 14:03:00 CST 2024.

#ifndef VAR_UTIL_DIR_READER_LINUX_H
#define VAR_UTIL_DIR_READER_LINUX_H

#include "net/base/Logging.h"
#include "net/base/StringSplitter.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

namespace var {

struct linux_dirent {
  uint64_t        d_ino;
  int64_t         d_off;
  unsigned short  d_reclen;
  unsigned char   d_type;
  char            d_name[0];
};

class DirReaderLinux {
 public:
  explicit DirReaderLinux(const char* directory_path)
      : fd_(open(directory_path, O_RDONLY | O_DIRECTORY)),
        offset_(0),
        size_(0) {
    memset(buf_, 0, sizeof(buf_));
  }

  ~DirReaderLinux() {
    if (fd_ >= 0) {
      if (close(fd_))
        LOG_ERROR << "Failed to close directory handle";
    }
  }

  bool IsValid() const {
    return fd_ >= 0;
  }

  static bool DirectoryExists(const char* directory_path) {
    struct stat info;
    if(stat(directory_path, &info) != 0) {
      return false;
    }
    if(info.st_mode & S_IFDIR) {
      return true;
    }
    return false;
  }

  static bool CreateDirectoryIfNotExists(const char* directory_path) {
    if(!directory_path) {
      LOG_ERROR << "Parameter[directory_path] is NULL";
      return false;
    }
    std::vector<std::string> paths;
    StringSplitter sp(directory_path, '/');
    for(; sp != nullptr; sp++) {
      std::string tmp(sp.field(), sp.length());
      if(tmp != "/") {
        paths.push_back(std::string(sp.field(), sp.length()));
      }
    }
    if(paths.size() == 1) {
      std::string& path = paths.front();
      if(!DirectoryExists(path.c_str())) {
        if(mkdir(path.c_str(), 0755) == 0) {
          return true;
        }
        return false;
      }
      return true;
    }
    else {
      std::string path;
      for(size_t i = 0; i < paths.size(); ++i) {
        path += paths.at(i);
        if(!DirectoryExists(path.c_str())) {
          if(mkdir(path.c_str(), 0755) != 0) {
            LOG_ERROR << "Failed to create " << path << " dir";
            return false;
          }
        }
        path += "/";
      }
      return true;
    }
  }

  // Move to the next entry returning false if the iteration is complete.
  bool Next() {
    if (size_) {
      linux_dirent* dirent = reinterpret_cast<linux_dirent*>(buf_ + offset_);
      offset_ += dirent->d_reclen;
    }

    if (offset_ != size_)
      return true;

    const int r = syscall(__NR_getdents64, fd_, buf_, sizeof(buf_));
    if (r == 0)
      return false;
    if (r == -1) {
      LOG_ERROR << "getdents64 returned an error: " << errno;
      return false;
    }
    size_ = r;
    offset_ = 0;
    return true;
  }

  const char* name() const {
    if (!size_)
      return NULL;

    const linux_dirent* dirent =
        reinterpret_cast<const linux_dirent*>(&buf_[offset_]);
    return dirent->d_name;
  }

  int fd() const {
    return fd_;
  }

  static bool IsFallback() {
    return false;
  }

 private:
  const int fd_;
  unsigned char buf_[512];
  size_t offset_, size_;
  std::string _path;

  DirReaderLinux(const DirReaderLinux&) = delete;
  void operator=(const DirReaderLinux&) = delete;
};

} // end namespace var


#endif // VAR_UTIL_DIR_READER_LINUX_H