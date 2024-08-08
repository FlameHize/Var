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

#ifndef VAR_HTTP_MESSAGE_H
#define VAR_HTTP_MESSAGE_H

#include "util/iobuf.h"
#include "http_header.h"
#include "http_parser.h"

namespace var {

enum HttpParserStage {
    HTTP_ON_MESSAGE_BEGIN,
    HTTP_ON_URL,
    HTTP_ON_STATUS,
    HTTP_ON_HEADER_FIELD,
    HTTP_ON_HEADER_VALUE,
    HTTP_ON_HEADERS_COMPLETE,
    HTTP_ON_BODY,
    HTTP_ON_MESSAGE_COMPLETE
};

class HttpMessage {
public:
    HttpMessage();
    ~HttpMessage();

    // Parse from bytes, length=0 is treated as EOF.
    // Returns bytes parsed, -1 on failure.
    ssize_t ParseFromBytes(const char* data, const size_t length);

    const IOBuf &body() const { return _body; }
    IOBuf &body() { return _body; }

    bool Completed() const { return _stage == HTTP_ON_MESSAGE_COMPLETE; }
    HttpParserStage stage() const { return _stage; }

    HttpHeader& header() { return _header; }
    const HttpHeader& header() const { return _header; }
    size_t parsed_length() const { return _parsed_length; }

    // http parser callback functions.
    static int on_message_begin(http_parser*);
    static int on_url(http_parser*, const char*, const size_t);
    static int on_status(http_parser*, const char*, const size_t);
    static int on_header_field(http_parser*, const char*, const size_t);
    static int on_header_value(http_parser*, const char*, const size_t);
    static int on_headers_complete(http_parser*);
    static int on_body(http_parser*, const char*, const size_t);
    static int on_message_complete(http_parser*);

    const http_parser& parser() const { return _parser; }

protected:
    int OnBody(const char* data, size_t size);

private:
    HttpParserStage _stage;
    HttpHeader _header;
    std::string _url;
    IOBuf _body;
    struct http_parser _parser;
    size_t _parsed_length;
    std::string _cur_header;
    std::string *_cur_value;
};

std::ostream& operator<<(std::ostream& os, const http_parser& parser);

} // end namespace var

#endif