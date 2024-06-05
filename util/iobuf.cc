// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include "iobuf.h"
#include <errno.h>
#include <sys/uio.h>

namespace var {

const char IOBuf::kCRLF[] = "\r\n";

const size_t IOBuf::kCheapPrepend;
const size_t IOBuf::kInitialSize;

ssize_t IOBuf::readFd(int fd, int* savedErrno) {
    // // saved an ioctl()/FIONREAD call to tell how much to read
    // char extrabuf[65536];
    // struct iovec vec[2];
    // const size_t writable = writableBytes();
    // vec[0].iov_base = begin() + _writerIndex;
    // vec[0].iov_len = writable;
    // vec[1].iov_base = extrabuf;
    // vec[1].iov_len = sizeof extrabuf;
    // // when there is enough space in this buffer, don't read into extrabuf.
    // // when extrabuf is used, we read 128k-1 bytes at most.
    // const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    // const ssize_t n = sockets::readv(fd, vec, iovcnt);
    // if (n < 0) {
    //     *savedErrno = errno;
    // } else if (implicit_cast<size_t>(n) <= writable) {
    //     _writerIndex += n;
    // }
    // else {
    //     _writerIndex = _buffer.size();
    //     append(extrabuf, n - writable);
    // }
    // return n;
}

} // end namespace var