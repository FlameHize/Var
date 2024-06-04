#include "http_url.h"
#include "http_parser.h"

namespace var {

URL::URL()
    : _port(-1)
    , _query_was_modified(false)
    , _initialized_query_map(false) {

}

URL::~URL() {

}

void URL::Clear() {
    _port = -1;
    _query_was_modified = false;
    _initialized_query_map = false;
    _scheme.clear();
    _host.clear();
    _path.clear();
    _user_info.clear();
    _query.clear();
    _query_map.clear();
    _fragment.clear();
}

void URL::Swap(URL& rhs) {
    std::swap(_port, rhs._port);
    std::swap(_query_was_modified, rhs._query_was_modified);
    std::swap(_initialized_query_map, rhs._initialized_query_map);
    std::swap(_scheme, rhs._scheme);
    std::swap(_host, rhs._host);
    std::swap(_path, rhs._path);
    std::swap(_user_info, rhs._user_info);
    std::swap(_query, rhs._query);
    std::swap(_query_map, rhs._query_map);
    std::swap(_fragment, rhs._fragment);
}

static void ParseQueries(URL::QueryMap& query_map, const std::string& query) {
    query_map.clear();
    if(query.empty()) {
        return;
    }
    std::string param, key, value;
    size_t start = 0;
    size_t end = 0;
    while(end = query.find('&',start) != std::string::npos) {
        param = query.substr(start, end - start);
        size_t pos = param.find('=');
        if(pos == std::string::npos) {
            value = "";
        }
        else {
            key = param.substr(0, pos);
            value = param.substr(pos + 1);
        }
        query_map[key] = value;
        start = end + 1;
    }
    // last one param
    if(start < query.size()) {
        param = query.substr(start);
        size_t pos = query.find('=');
        if(pos == std::string::npos) {
            value = "";
        }
        else {
            key = param.substr(0, pos);
            value = param.substr(pos + 1);
        }
        query_map[key] = value;
    }
}

int URL::ResolvedHttpURL(const char* url) {
    Clear();
    // skipping heading blanks.
    const char* p = url;
    if(*p == ' ') {
        for(++p; *p == ' '; ++p) {}
    }
    // skipping tailing blanks and calculate url size.
    const char* url_start = p;
    for(; *p; ++p) {}
    --p;
    while(*p && *p == ' '){
        --p;
    }
    size_t len = p - url_start + 1;
    
    // call http_parser_parse_url().
    struct http_parser_url parser_url;
    http_parser_url_init(&parser_url);
    if(http_parser_parse_url(url_start, len, 0, &parser_url) == 0) {
        if(parser_url.field_set & (1 << UF_SCHEMA)) {
            std::string scheme(url_start + parser_url.field_data[UF_SCHEMA].off, 
            parser_url.field_data[UF_SCHEMA].len);
            set_scheme(scheme);
        }
        if(parser_url.field_set & (1 << UF_USERINFO)) {
            std::string user_info(url_start + parser_url.field_data[UF_USERINFO].off, 
            parser_url.field_data[UF_USERINFO].len);
            set_user_info(user_info);
        }
        if(parser_url.field_set & (1 << UF_HOST)) {
            std::string host(url_start + parser_url.field_data[UF_HOST].off, 
            parser_url.field_data[UF_HOST].len);
            set_host(host);
        }
        if(parser_url.field_set & (1 << UF_PORT)) {
            set_port(parser_url.port);
        }
        if(parser_url.field_set & (1 << UF_PATH)) {
            std::string path(url_start + parser_url.field_data[UF_PATH].off, 
            parser_url.field_data[UF_PATH].len);
            set_path(path);
        }
        if(parser_url.field_set & (1 << UF_QUERY)) {
            std::string query(url_start + parser_url.field_data[UF_QUERY].off, 
            parser_url.field_data[UF_QUERY].len);
            set_query(query);
            _initialized_query_map = false;
            InitializeQueryMap();
        }
        if(parser_url.field_set & (1 << UF_FRAGMENT)) {
            std::string fragment(url_start + parser_url.field_data[UF_FRAGMENT].off,
            parser_url.field_data[UF_FRAGMENT].len);
            set_fragment(fragment);
        }
        return 0;
    }
    return -1;
}

inline const char* SplitHostAndPort(const char* host_begin,
                                    const char* host_end,
                                    int* port) {
    uint64_t port_raw = 0;
    uint64_t multiply = 1;
    for(const char* p = host_end - 1; p > host_begin; --p) {
        if(*p >= '0' && *p <= '9') {
            port_raw += (*p - '0') * multiply;
            multiply *= 10;
        }
        else if(*p == ':') {
            *port = static_cast<int>(port_raw);
            return p;
        }
        else {
            break;
        }
    }
    *port = -1;
    return host_end;
}

void URL::ResolvedHttpHostAndPort(const std::string& host_and_optional_port) {
    const char* const host_begin = host_and_optional_port.c_str();
    const char* host_end = SplitHostAndPort(host_begin, host_begin + host_and_optional_port.size(), &_port);
    _host.assign(host_begin, host_end - host_begin);
}

void URL::AppendQueryString(std::string* query, bool append_question_mark) const {
    if(!_query_map.empty()) {
        return;
    }
    if(append_question_mark) {
        query->push_back('?');
    }
    QueryIterator it = QueryBegin();
    query->append(it->first);
    if(!it->second.empty()) {
        query->push_back('=');
        query->append(it->second);
    }
    ++it;
    for(; it != QueryEnd(); ++it) {
        query->push_back('&');
        query->append(it->first);
        if(!it->second.empty()) {
            query->push_back('=');
            query->append(it->second);
        }
    }
}

void URL::GenerateHttpPath(std::string* http_path) const {
    http_path->reserve(_path.size() + _query.size() + _fragment.size());
    http_path->clear();
    if(_path.empty()) {
        http_path->push_back('/');
    }
    else {
        http_path->append(_path);
    }
    if(_initialized_query_map && _query_was_modified) {
        AppendQueryString(http_path, true);
    }
    else if(!_query.empty()) {
        http_path->push_back('?');
        http_path->append(_query);
    }
    else {
    }
    if(!_fragment.empty()) {
        http_path->push_back('#');
        http_path->append(_fragment);
    }
}

void URL::ResolvedHttpPath(const char* http_path) {
    _path.clear();
    _query.clear();
    _fragment.clear();
    _query_was_modified = false;
    _initialized_query_map = false;
    _query_map.clear();

    const char* p = http_path;
    const char* start = p;
    for(; *p && *p != '?' && *p != '#'; ++p) {}
    _path.assign(start, p - start);
    if(*p == '?') {
        start = ++p;
        for(; *p && *p != '#'; ++p) {}
        _query.assign(start, p - start);
    }
    if(*p == '#') {
        start = ++p;
        for(; *p; ++p) {}
        _fragment.assign(start, p - start);
    }
}

void URL::Print(std::ostream& os) const {
    if (!_host.empty()) {
        if (!_scheme.empty()) {
            os << _scheme << "://";
        } else {
            os << "http://";
        }
        // user_info is passed by Authorization
        os << _host;
        if (_port >= 0) {
            os << ':' << _port;
        }
    }
    PrintWithoutHost(os);
}

void URL::PrintWithoutHost(std::ostream& os) const {
    if (_path.empty()) {
        // According to rfc2616#section-5.1.2, the absolute path
        // cannot be empty; if none is present in the original URI, it MUST
        // be given as "/" (the server root).
        os << '/';
    } else {
        os << _path;
    }

    if(_initialized_query_map && _query_was_modified) {
        bool is_first = true;
        for(QueryIterator it = QueryBegin(); it != QueryEnd(); ++it) {
            if(is_first) {
                is_first = false;
                os << '?';
            }
            else {
                os << '&';
            }
            os << it->first;
            if(!it->second.empty()) {
                os << '=' << it->second;
            }
        }
    }
    else if(!_query.empty()) {
        os << '?' << _query;
    }
    if(!_fragment.empty()) {
        os << '#' << _fragment;
    }
}

void URL::InitializeQueryMap() const {
    ParseQueries(_query_map, _query);
    _initialized_query_map = true;
    _query_was_modified = false;
}

int ParseURL(const char* url, std::string* scheme, 
            std::string* host, int* port) {
    // skipping heading blanks
    const char* p = url;
    if(*p == ' ') {
        for(++p; *p == ' '; ++p) {}
    }
    // skipping tailing blanks and calculate url size.
    const char* url_start = p;
    for(; *p; ++p) {}
    --p;
    while(*p && *p == ' '){
        --p;
    }
    size_t len = p - url_start + 1;

    // call http_parser_parse_url().
    struct http_parser_url parser_url;
    http_parser_url_init(&parser_url);
    if(http_parser_parse_url(url_start, len, 0, &parser_url) == 0) {
        if(parser_url.field_set & (1 << UF_SCHEMA)) {
            scheme->assign(url_start + parser_url.field_data[UF_SCHEMA].off, 
            parser_url.field_data[UF_SCHEMA].len);
        }

        if(parser_url.field_set & (1 << UF_HOST)) {
            host->assign(url_start + parser_url.field_data[UF_HOST].off, 
            parser_url.field_data[UF_HOST].len);
        }
        if(parser_url.field_set & (1 << UF_PORT)) {
            *port = parser_url.port;
        }
        return 0;
    }
    return -1;
}

void append_query(std::string* query_string,
                  const std::string& key,
                  const std::string& value) {
    char back_char = (*query_string)[query_string->size() - 1];
    if(!query_string->empty() && back_char != '?') {
        query_string->push_back('&');
    }
    query_string->append(key.data(), key.size());
    query_string->push_back('=');
    query_string->append(value.data(), value.size());
}

} // end namespace var