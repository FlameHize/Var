#include "http_message.h"

namespace var {

// Implement callbacks for http parser
int HttpMessage::on_message_begin(http_parser* parser) {
    HttpMessage* http_message = (HttpMessage*)parser->data;
    http_message->_stage = HTTP_ON_MESSAGE_BEGIN;
    return 0;
}

// For request
int HttpMessage::on_url(http_parser* parser,
                        const char* at, const size_t length) {
    HttpMessage* http_message = (HttpMessage*)parser->data;
    http_message->_stage = HTTP_ON_URL;
    http_message->_url.append(at, length);
    return 0;
}

// For response
int HttpMessage::on_status(http_parser* parser,
                           const char* at, const size_t length) {
    HttpMessage* http_message = (HttpMessage*)parser->data;
    http_message->_stage = HTTP_ON_STATUS;
    return 0;
}

// http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
// Multiple message-header fields with the same field-name MAY be present in a
// message if and only if the entire field-value for that header field is
// defined as a comma-separated list [i.e., #(values)]. It MUST be possible to
// combine the multiple header fields into one "field-name: field-value" pair,
// without changing the semantics of the message, by appending each subsequent
// field-value to the first, each separated by a comma. The order in which
// header fields with the same field-name are received is therefore significant
// to the interpretation of the combined field value, and thus a proxy MUST NOT
// change the order of these field values when a message is forwarded. 
int HttpMessage::on_header_field(http_parser* parser,
                                 const char* at, const size_t length) {
    HttpMessage* http_message = (HttpMessage*)parser->data;
    if(http_message->_stage != HTTP_ON_HEADER_FIELD) {
        http_message->_stage = HTTP_ON_HEADER_FIELD;
        http_message->_cur_header.clear();
    }
    http_message->_cur_header.append(at, length);
    return 0;
}

// Merge values from multiple identical fields and separate them with commas
int HttpMessage::on_header_value(http_parser* parser,
                                 const char* at, const size_t length) {
    HttpMessage* http_message = (HttpMessage*)parser->data;
    bool first_entry = false;
    if(http_message->_stage != HTTP_ON_HEADER_VALUE) {
        http_message->_stage = HTTP_ON_HEADER_VALUE;
        first_entry = true;
        if(http_message->_cur_header.empty()) {
            return -1;
        }
        http_message->_cur_value = 
            &http_message->header().GetOrAddHeader(http_message->_cur_header);
        if(http_message->_cur_value && !http_message->_cur_value->empty()) {
            http_message->_cur_value->push_back(',');
        }
    }
    if(http_message->_cur_value) {
        http_message->_cur_value->append(at, length);
    }
}

int HttpMessage::on_headers_complete(http_parser* parser) {
    HttpMessage* http_message = (HttpMessage*)parser->data;
    http_message->_stage = HTTP_ON_HEADERS_COMPLETE;
    // Resolved and set content-type.
    const std::string* content_type = http_message->header().GetHeader("content-type");
    if(content_type) {
        http_message->header().set_content_type(*content_type);
        http_message->header().RemoveHeader("content-type");
    }
    if(parser->http_major > 1) {
        parser->http_major = 1;
    }
    http_message->header().set_version(parser->http_major, parser->http_minor);
    // Only for response
    // http_parser may set status_code to 0 when the field is not needed,
    // e.g. in a request. In principle status_code is undefined in a request,
    // but to be consistent and not surprise users, we set it to OK as well.
    http_message->header().set_status_code(
        !parser->status_code ? HTTP_STATUS_OK : parser->status_code);
    
    // Only for request
    // method is 0(which is DELETE) for response as well. Since users are
    // unlikely to check method of a response, we don't do anything.
    http_message->header().set_method(static_cast<HttpMethod>(parser->method));
    // Resolved the http url here.
    if(parser->type == HTTP_REQUEST &&
            http_message->header().url().ResolvedHttpURL(http_message->_url) != 0) {
        return -1;
    }
    // rfc2616-sec5.2
    // 1. If Request-URI is an absoluteURI, the host is part of the Request-URI.
    // Any Host header field value in the request MUST be ignored.
    // 2. If the Request-URI is not an absoluteURI, and the request includes a
    // Host header field, the host is determined by the Host header field value.
    // 3. If the host as determined by rule 1 or 2 is not a valid host on the
    // server, the responce MUST be a 400 error messsage.
    HttpUrl& url = http_message->header().url();
    if(url.host().empty()) {
        const std::string* host_header = http_message->header().GetHeader("host");
        if(host_header) {
            url.ResolvedHttpHostAndPort(*host_header);
        }
    }
    return 0;
}

int HttpMessage::on_body(http_parser* parser,
                         const char* at, const size_t length) {
    HttpMessage* http_message = (HttpMessage*)parser->data;
    if(http_message->_stage != HTTP_ON_BODY) {
        http_message->_stage = HTTP_ON_BODY;
    }
    ///@todo
    // _body.append(at, length);
    return 0;                   
}

int HttpMessage::on_message_complete(http_parser* parser) {
    HttpMessage* http_message = (HttpMessage*)parser->data;
    if(http_message->_stage != HTTP_ON_MESSAGE_COMPLETE) {
        http_message->_stage = HTTP_ON_MESSAGE_COMPLETE;
    }
    return 0;
}

const http_parser_settings g_parser_settings = {
    &HttpMessage::on_message_begin,
    &HttpMessage::on_url,
    &HttpMessage::on_status,
    &HttpMessage::on_header_field,
    &HttpMessage::on_header_value,
    &HttpMessage::on_headers_complete,
    &HttpMessage::on_body,
    &HttpMessage::on_message_complete
};

HttpMessage::HttpMessage() 
    : _stage(HTTP_ON_MESSAGE_BEGIN)
    , _parsed_length(0)
    , _cur_value(NULL) {
    http_parser_init(&_parser, HTTP_BOTH);
    _parser.data = this;
}

HttpMessage::~HttpMessage() {
}

ssize_t HttpMessage::ParseFromBytes(const char* data, const size_t length) {
    if(Completed()) {
        if(length == 0) {
            return 0;
        }
        return -1;
    }
    const ssize_t nprocessed = 
        http_parser_execute(&_parser, &g_parser_settings, data, length);
    if(_parser.http_errno != 0) {
        return -1;
    }
    _parsed_length += length;
    return nprocessed;
}

static void DescribeHttpParserFlags(std::ostream& os, unsigned int flags) {
    if (flags & F_CHUNKED) {
        os << "F_CHUNKED|";
    }
    if (flags & F_CONNECTION_KEEP_ALIVE) {
        os << "F_CONNECTION_KEEP_ALIVE|";
    }
    if (flags & F_CONNECTION_CLOSE) {
        os << "F_CONNECTION_CLOSE|";
    }
    if (flags & F_TRAILING) {
        os << "F_TRAILING|";
    }
    if (flags & F_UPGRADE) {
        os << "F_UPGRADE|";
    }
    if (flags & F_SKIPBODY) {
        os << "F_SKIPBODY|";
    }
}

std::ostream& operator<<(std::ostream& os, const http_parser& parser) {
    os << "{type=" << http_parser_type_name((http_parser_type)parser.type)
       << " flags=`";
    DescribeHttpParserFlags(os, parser.flags);
    os << "' state=" << http_parser_state_name(parser.state)
       << " header_state=" << http_parser_header_state_name(
           parser.header_state)
       << " http_errno=`" << http_errno_description(
           (http_errno)parser.http_errno)
       << "' index=" << parser.index
       << " nread=" << parser.nread
       << " content_length=" << parser.content_length
       << " http_major=" << parser.http_major
       << " http_minor=" << parser.http_minor;
    if (parser.type == HTTP_RESPONSE || parser.type == HTTP_BOTH) {
        os << " status_code=" << parser.status_code;
    }
    if (parser.type == HTTP_REQUEST || parser.type == HTTP_BOTH) {
        os << " method=" << HttpMethod2Str((HttpMethod)parser.method);
    }
    os << " data=" << parser.data
       << '}';
    return os;
}

} // end namespace var