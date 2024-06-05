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

#ifndef VAR_HTTP_URL_H
#define VAR_HTTP_URL_H

#include <string>
#include <ostream>
#include <unordered_map>

namespace var {
// The class for URI scheme : http://en.wikipedia.org/wiki/URI_scheme
//
//  foo://username:password@example.com:8042/over/there/index.dtb?type=animal&name=narwhal#nose
//  \_/   \_______________/ \_________/ \__/            \___/ \_/ \______________________/ \__/
//   |           |               |       |                |    |            |                |
//   |       userinfo           host    port              |    |          query          fragment
//   |    \________________________________/\_____________|____|/ \__/        \__/
// scheme                 |                          |    |    |    |          |
//                    authority                      |    |    |    |          |
//                                                 path   |    |    interpretable as keys
//                                                        |    |
//        \_______________________________________________|____|/       \____/     \_____/
//                             |                          |    |          |           |
//                     hierarchical part                  |    |    interpretable as values
//                                                        |    |
//                                   interpretable as filename |
//                                                             |
//                                                             |
//                                               interpretable as extension

struct URLStringHash : public std::hash<std::string> {
    std::size_t operator()(const std::string& s) const {
        std::size_t result = 0;
        for (std::string::const_iterator i = s.begin(); i != s.end(); ++i) {
            result = result * 101 + *i;
        }
        return result;        
    }
};

struct URLStringEqual : public std::equal_to<std::string> {  
    bool operator()(const std::string& lhs, const std::string& rhs) const {  
        return  lhs == rhs;
    }  
}; 

class URL {
public:
    typedef std::unordered_map<std::string, std::string, URLStringHash, URLStringEqual> QueryMap;
    typedef QueryMap::const_iterator QueryIterator;

    // You can copy a URL.
    URL();
    ~URL();

    // Exchange internal fields with another URL.
    void Swap(URL& rhs);

    // Reset internal fields as if they're just default-constructed.
    void Clear();

    // Resolved URL and set into corresponding fields.
    // heading and tailing spaces are allowed and skipped.
    // Returns 0 on success, -1 otherwise and status() is set.
    int ResolvedHttpURL(const char* url);
    int ResolvedHttpURL(const std::string& url) { 
        return ResolvedHttpURL(url.c_str()); 
    }

    // Set host/port with the input in form of "host:(optional)port".
    void ResolvedHttpHostAndPort(const std::string& host_and_optional_port);

    // Put path?query#fragment into 'path'
    void GenerateHttpPath(std::string* http_path) const;
    // Set path/query/fragment with the input in form of "path?query#fragment".
    void ResolvedHttpPath(const char* http_path);
    void ResolvedHttpPath(const std::string& http_path) {
        return ResolvedHttpPath(http_path.c_str());
    }

    // Syntactic sugar of SetHttpURL
    void operator=(const char* url) { ResolvedHttpURL(url); }
    void operator=(const std::string& url) { ResolvedHttpURL(url); };

    // Sub fields. Empty string if the field is not set.
    const std::string& scheme() const { return _scheme; }
    const std::string& host() const { return _host; }
    int port() const { return _port; }
    const std::string& path() const { return _path; }
    const std::string& user_info() const { return _user_info; }
    const std::string& fragment() const { return _fragment; }
    // NOTE: This method is not thread-safe because it may re-generate
    // the query-string if SetQuery()/RemoveQuery() were successfully called.
    const std::string& query() const;
    
    // Overwrite parts of the URL.
    // NOTE: The input must be guaranteed to be vaild.
    void set_scheme(const std::string& scheme) { _scheme = scheme; }
    void set_host(const std::string& host) { _host = host; }
    void set_path(const std::string& path) { _path = path; }
    void set_port(int port) { _port = port; }
    void set_user_info(const std::string& user_info) { _user_info = user_info; }
    void set_fragment(const std::string& fragment) { _fragment = fragment; }
    void set_query(const std::string& query) { _query = query; }

    // Get the value of a case-sensitive key. 
    // Returns pointer to the value, NULL when the key does not exist.
    const std::string* GetQuery(const std::string& key) const;

    // Add key/value pair. Override existing value.
    // change the modified flag to update the output query string.
    void SetQuery(const std::string& key, const std::string& value);

    // Remove value associated with 'key'.
    // Returns 1 on removed, 0 otherwise.
    size_t RemoveQuery(const std::string& key);

    // Get query iterators which are invailded after calling SetQuery
    // or SetHttpURL().
    QueryIterator QueryBegin() const { return get_query_map().begin(); }
    QueryIterator QueryEnd() const { return get_query_map().end(); }

    // #queries
    size_t QueryCount() const { return get_query_map().size(); }

    // Print this URL to the ostream.
    // PrintWithoutHost only prints components includeing and after path.
    void PrintWithoutHost(std::ostream& os) const;
    void Print(std::ostream& os) const;
    
private:
    void InitializeQueryMap() const;

    QueryMap& get_query_map() const {
        if(!_initialized_query_map) {
            InitializeQueryMap();
        }
        return _query_map;
    }

    // Iterate _query_map and append all queries to 'query'.
    void AppendQueryString(std::string* query, bool append_question_mask) const;

    // Used to check whether the query has been changed.
    mutable bool _query_was_modified;
    mutable bool _initialized_query_map;
    int _port;
    std::string _scheme;
    std::string _host;
    std::string _path;
    std::string _user_info;
    std::string _fragment;
    mutable std::string _query;
    mutable QueryMap _query_map;
};

// Parse scheme/host/port from 'url' if the corrresponding parameter is not NULL.
// Returns 0 on success, -1 otherwise.
int ParseURL(const char* url, std::string* scheme, std::string* host, int* port);

inline const std::string* URL::GetQuery(const std::string& key) const {
    QueryIterator it = get_query_map().find(key);
    if(it == QueryEnd()) {
        return nullptr;
    }
    return &it->second;
}

inline void URL::SetQuery(const std::string& key, const std::string& value) {
    get_query_map()[key] = value;
    _query_was_modified = true;
}

inline size_t URL::RemoveQuery(const std::string& key) {
    if(get_query_map().erase(key)) {
        _query_was_modified = true;
        return 1;
    }
    return 0;
}

inline const std::string& URL::query() const {
    if(_initialized_query_map && _query_was_modified) {
        _query_was_modified = false;
        _query.clear();
        AppendQueryString(&_query,false);
    }
    return _query;
}

inline std::ostream& operator<<(std::ostream& os, const URL& url) {
    url.Print(os);
    return os;
}

// This function can append key and value to *query_string
// in consideration of all possible format of *query_string
// for example:
// "" -> "key=value"
// "key1=value1" -> "key1=value1&key=value"
// "/some/path?" -> "/some/path?key=value"
// "/some/path?key1=value1" -> "/some/path?key1=value1&key=value"
void AppendQuery(std::string* query_string, 
                const std::string& key, 
                const std::string& value);

}; // end namespace var

#endif