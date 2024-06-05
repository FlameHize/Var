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

#ifndef VAR_HTTP_HEADER_H
#define VAR_HTTP_HEADER_H

#include "http_url.h"
#include "http_status_code.h"
#include "http_method.h"

namespace var {

class HttpHeader {
public:
    typedef std::unordered_map<std::string, std::string> HeaderMap;
    typedef HeaderMap::const_iterator HeaderIterator;

    HttpHeader();
    ~HttpHeader();

    // Exchange internal fields with another HttpHeader.
    void Swap(HttpHeader& rhs);

    // Reset internal fields if they're just default-constructed.
    void Clear();

    // Get http version, 1.1 by default.
    int major_version() const { return _version.first; }
    int minor_version() const { return _version.second; }
    // Change version.
    void set_version(int http_major, int http_minor) {
        _version = std::make_pair(http_major, http_minor);
    }

    // True if the message from HTTP2.
    bool is_http2() const { return major_version() == 2; }

    // Get/set "Content-Type". Notice that you can't get "Content-Type"
    // via GetHeader().
    // possible values: "text/plain", "application/json" ...
    const std::string& content_type() const { return _content_type; }
    std::string& mutable_content_type() { return _content_type; }
    void set_content_type(const std::string& type) { _content_type = type; }
    void set_content_type(const char* type) { _content_type = type; }

    // Get value of a header which is case-insensitive according to:
    // https://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
    // Namely, GetHeader("log-id"), GetHeader("Log-Id"), GetHeader("LOG-ID")
    // point to the same value.
    // Return pointer to the value, NULL on not found.
    // NOTE: Not work for "Content-Type", call content_type() instead.
    const std::string* GetHeader(const char* key) const;
    const std::string* GetHeader(const std::string& key) const { return GetHeader(key.c_str()); }

    // Set value of a header.
    // NOTE: Not work for "Content-Type", call set_content_type() instead.
    void SetHeader(const std::string& key, const std::string& value) {
        GetOrAddHeader(key) = value;
    }

    // Remove a header.
    void RemoveHeader(const char* key) { _headers.erase(key); }
    void RemoveHeader(const std::string& key) { _headers.erase(key); }

    // Append value to a header. If the header already exists, separate
    // old value and new value with comma(,) according to:
    // https://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
    void AppendHeader(const std::string& key, const std::string& value);

    // Get header iterators which are invaildated after calling AppendHeader().
    HeaderIterator HeaderBegin() const { return _headers.begin(); }
    HeaderIterator HeaderEnd() const { return _headers.end(); }

    // Get header field count.
    size_t HeaderCount() const { return _headers.size(); }

    // Get the URL object.
    const HttpUrl& url() const { return _url; }
    HttpUrl& url() { return _url; }

    // Get/set http method.
    HttpMethod method() const { return _method; }
    void set_method(const HttpMethod& method) { _method = method; }

    // Get/set status-code and reason-phrase. Notice that the const char*
    // returned by reason_phrase() will be invalidated after next call to
    // set_status_code().
    int status_code() const { return _status_code; }
    void set_status_code(int status_code) { _status_code = status_code; };
    const char* reason_phrase() const;

    // Used to find service.
    // The URL path removed with matched prefix.
    // NOTE: always normalized and NOT started with /.
    //
    // Accessing HttpService.Echo
    //   [URL]                               [unresolved_path]
    //   "/HttpService/Echo"                 ""
    //   "/HttpService/Echo/Foo"             "Foo"
    //   "/HttpService/Echo/Foo/Bar"         "Foo/Bar"
    //   "/HttpService//Echo///Foo//"        "Foo"
    //
    // Accessing FileService.default_method:
    //   [URL]                               [unresolved_path]
    //   "/FileService"                      ""
    //   "/FileService/123.txt"              "123.txt"
    //   "/FileService/mydir/123.txt"        "mydir/123.txt"
    //   "/FileService//mydir///123.txt//"   "mydir/123.txt"
    const std::string& unresolved_path() const { return _unresolved_path; }
    void set_unresolved_path(const std::string& path) { _unresolved_path = path; }

private:
friend class HttpMessage;

    std::string& GetOrAddHeader(const std::string& key) {
        return _headers[key];
    }

    HeaderMap _headers;
    HttpUrl _url;
    int _status_code;
    HttpMethod _method;
    std::string _content_type;
    std::string _unresolved_path;
    std::pair<int, int> _version;
};

const HttpHeader& DefaultHttpHeader();

inline const std::string* HttpHeader::GetHeader(const char* key) const {
    HeaderIterator it = _headers.find(key);
    if(it == HeaderEnd()) {
        return NULL;
    }
    return &it->second;
}

} // end namespace var

#endif