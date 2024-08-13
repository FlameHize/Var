#include "http_header.h"

using namespace var;
using namespace var::net;

HttpHeader::HttpHeader()
    : _status_code(HTTP_STATUS_OK)
    , _method(HTTP_METHOD_GET)
    , _version(1, 1) {
    // NOTE: don't forget to clear the field in Clear() as well.        
}

HttpHeader::~HttpHeader() {
}

void HttpHeader::Swap(HttpHeader& rhs) {
    _headers.swap(rhs._headers);
    _url.Swap(rhs._url);
    std::swap(_status_code, rhs._status_code);
    std::swap(_method, rhs._method);
    _content_type.swap(rhs._content_type);
    _unresolved_path.swap(rhs._unresolved_path);
    std::swap(_version, rhs._version);
}

void HttpHeader::Clear() {
    _headers.clear();
    _url.Clear();
    _status_code = HTTP_STATUS_OK;
    _method = HTTP_METHOD_GET;
    _content_type.clear();
    _unresolved_path.clear();
    _version = std::make_pair(1, 1);
}

void HttpHeader::AppendHeader(const std::string& key, const std::string& value) {
    std::string& slot = GetOrAddHeader(key);
    if(slot.empty()) {
        slot.assign(value.data(), value.size());
    }
    else {
        slot.reserve(slot.size() + value.size() + 1);
        slot.push_back(',');
        slot.append(value.data(), value.size());
    }
}

const char* HttpHeader::reason_phrase() const {
    return HttpReasonPhrase(_status_code);
}

const HttpHeader& DefaultHttpHeader() {
    static HttpHeader header;
    return header;
}