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

#ifndef VAR_UTIL_IOBUF_H
#define VAR_UTIL_IOBUF_H

#include "macros.h"
#include "endian.h"
#include "string_piece.h"

#include <algorithm>
#include <vector>
#include <assert.h>
#include <string.h>

namespace var {

/// A IOBuf class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode
class IOBuf {
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit IOBuf(size_t initialSize = kInitialSize)
        : _buffer(kCheapPrepend + initialSize)
        , _readerIndex(kCheapPrepend)
        , _writerIndex(kCheapPrepend) {
        assert(readableBytes() == 0);
        assert(writableBytes() == initialSize);
        assert(prependableBytes() == kCheapPrepend);
    }

    // implicit copy-ctor, move-ctor, dtor and assignment are fine
    // NOTE: implicit move-ctor is added in g++ 4.6

    void swap(IOBuf& rhs) {
        _buffer.swap(rhs._buffer);
        std::swap(_readerIndex, rhs._readerIndex);
        std::swap(_writerIndex, rhs._writerIndex);
    }

    size_t readableBytes() const { return _writerIndex - _readerIndex; }

    size_t writableBytes() const { return _buffer.size() - _writerIndex; }

    size_t prependableBytes() const { return _readerIndex; }

    const char* peek() const { return begin() + _readerIndex; }

    const char* findCRLF() const {
        const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF+2);
        return crlf == beginWrite() ? NULL : crlf;
    }

    const char* findCRLF(const char* start) const {
        assert(peek() <= start);
        assert(start <= beginWrite());
        // FIXME: replace with memmem()?
        const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF+2);
        return crlf == beginWrite() ? NULL : crlf;
    }

    const char* findEOL() const {
        const void* eol = memchr(peek(), '\n', readableBytes());
        return static_cast<const char*>(eol);
    }

    const char* findEOL(const char* start) const {
        assert(peek() <= start);
        assert(start <= beginWrite());
        const void* eol = memchr(start, '\n', beginWrite() - start);
        return static_cast<const char*>(eol);
    }

    // retrieve returns void, to prevent
    // string str(retrieve(readableBytes()), readableBytes());
    // the evaluation of two functions are unspecified
    void retrieve(size_t len) {
        assert(len <= readableBytes());
        if (len < readableBytes()) {
            _readerIndex += len;
        }
        else {
            retrieveAll();
        }
    }

    void retrieveUntil(const char* end) {
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(end - peek());
    }

    void retrieveInt64(){
        retrieve(sizeof(int64_t));
    }

    void retrieveInt32() {
        retrieve(sizeof(int32_t));
    }

    void retrieveInt16() {
        retrieve(sizeof(int16_t));
    }

    void retrieveInt8() {
        retrieve(sizeof(int8_t));
    }

    void retrieveAll() {
        _readerIndex = kCheapPrepend;
        _writerIndex = kCheapPrepend;
    }

    std::string retrieveAllAsString(){
        return retrieveAsString(readableBytes());
    }

    std::string retrieveAsString(size_t len) {
        assert(len <= readableBytes());
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }

    StringPiece toStringPiece() const {
        return StringPiece(peek(), static_cast<int>(readableBytes()));
    }

    void append(const StringPiece& str) {
        append(str.data(), str.size());
    }

    void append(const char* /*restrict*/ data, size_t len) {
        ensureWritableBytes(len);
        std::copy(data, data+len, beginWrite());
        hasWritten(len);
    }

    void append(const void* /*restrict*/ data, size_t len) {
        append(static_cast<const char*>(data), len);
    }

    void ensureWritableBytes(size_t len) {
        if(writableBytes() < len) {
            makeSpace(len);
        }
        assert(writableBytes() >= len);
    }

    char* beginWrite() { return begin() + _writerIndex; }

    const char* beginWrite() const { return begin() + _writerIndex; }

    void hasWritten(size_t len) {
        assert(len <= writableBytes());
        _writerIndex += len;
    }

    void unwrite(size_t len) {
        assert(len <= readableBytes());
        _writerIndex -= len;
    }

    // Append int64_t using network endian
    void appendInt64(int64_t x){
        int64_t be64 = sockets::hostToNetwork64(x);
        append(&be64, sizeof be64);
    }

    // Append int32_t using network endian
    void appendInt32(int32_t x){
        int32_t be32 = sockets::hostToNetwork32(x);
        append(&be32, sizeof be32);
    }

    void appendInt16(int16_t x) {
        int16_t be16 = sockets::hostToNetwork16(x);
        append(&be16, sizeof be16);
    }

    void appendInt8(int8_t x){
        append(&x, sizeof x);
    }

    // Read int64_t from network endian
    // Require: buf->readableBytes() >= sizeof(int32_t)
    int64_t readInt64() {
        int64_t result = peekInt64();
        retrieveInt64();
        return result;
    }

    // ead int32_t from network endian
    // Require: buf->readableBytes() >= sizeof(int32_t)
    int32_t readInt32() {
        int32_t result = peekInt32();
        retrieveInt32();
        return result;
    }

    int16_t readInt16() {
        int16_t result = peekInt16();
        retrieveInt16();
        return result;
    }

    int8_t readInt8() {
        int8_t result = peekInt8();
        retrieveInt8();
        return result;
    }

    // Peek int64_t from network endian
    // Require: buf->readableBytes() >= sizeof(int64_t)
    int64_t peekInt64() const {
        assert(readableBytes() >= sizeof(int64_t));
        int64_t be64 = 0;
        ::memcpy(&be64, peek(), sizeof be64);
        return sockets::networkToHost64(be64);
    }

    // Peek int32_t from network endian
    // Require: buf->readableBytes() >= sizeof(int32_t)
    int32_t peekInt32() const{
        assert(readableBytes() >= sizeof(int32_t));
        int32_t be32 = 0;
        ::memcpy(&be32, peek(), sizeof be32);
        return sockets::networkToHost32(be32);
    }

    int16_t peekInt16() const {
        assert(readableBytes() >= sizeof(int16_t));
        int16_t be16 = 0;
        ::memcpy(&be16, peek(), sizeof be16);
        return sockets::networkToHost16(be16);
    }

    int8_t peekInt8() const {
        assert(readableBytes() >= sizeof(int8_t));
        int8_t x = *peek();
        return x;
    }

    // Prepend int64_t using network endian
    void prependInt64(int64_t x) {
        int64_t be64 = sockets::hostToNetwork64(x);
        prepend(&be64, sizeof be64);
    }

    // Prepend int32_t using network endian
    void prependInt32(int32_t x) {
        int32_t be32 = sockets::hostToNetwork32(x);
        prepend(&be32, sizeof be32);
    }

    void prependInt16(int16_t x) {
        int16_t be16 = sockets::hostToNetwork16(x);
        prepend(&be16, sizeof be16);
    }

    void prependInt8(int8_t x) {
        prepend(&x, sizeof x);
    }

    void prepend(const void* /*restrict*/ data, size_t len) {
        assert(len <= prependableBytes());
        _readerIndex -= len;
        const char* d = static_cast<const char*>(data);
        std::copy(d, d+len, begin()+_readerIndex);
    }

    void shrink(size_t reserve) {
        // FIXME: use vector::shrink_to_fit() in C++ 11 if possible.
        IOBuf other;
        other.ensureWritableBytes(readableBytes()+reserve);
        other.append(toStringPiece());
        swap(other);
    }

    size_t internalCapacity() const {
        return _buffer.capacity();
    }

    // Read data directly into buffer.
    // It may implement with readv(2)
    // Return result of read(2), @c errno is saved
    ssize_t readFd(int fd, int* savedErrno);

private:
    char* begin() { return &*_buffer.begin(); }

    const char* begin() const { return &*_buffer.begin(); }

    void makeSpace(size_t len)
    {
        if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
            // FIXME: move readable data
            _buffer.resize(_writerIndex+len);
        }
        else {
            // move readable data to the front, make space inside iobuf
            assert(kCheapPrepend < _readerIndex);
            size_t readable = readableBytes();
            std::copy(begin()+_readerIndex,
                    begin()+_writerIndex,
                    begin()+kCheapPrepend);
            _readerIndex = kCheapPrepend;
            _writerIndex = _readerIndex + readable;
            assert(readable == readableBytes());
        }
    }

private:
    std::vector<char> _buffer;
    size_t _readerIndex;
    size_t _writerIndex;

    static const char kCRLF[];
};

}  // namespace var

#endif  // VAR_UTIL_IOBUF_H