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

#include <gtest/gtest.h>
#include "net/http/http_message.h"

using namespace var;
using namespace var::net;

TEST(HttpMessageTest, request_sanity) {
    const char* http_request = 
        "POST /path/file.html?sdfsdf=sdfs&sldf1=sdf HTTP/12.34\r\n"
        "From: someuser@jmarshall.com\r\n"
        "User-Agent: HTTPTool/1.0  \r\n"  // intended ending spaces
        "Content-Type: json\r\n"
        "Content-Length: 20\r\n"
        "Log-ID: 456\r\n"
        "Host: myhost\r\n"
        "Correlation-ID: 123\r\n"
        "Authorization: test\r\n"
        "Accept: */*\r\n"
        "\r\n"
        "Message Body sdfsdfa\r\n"
    ;
    HttpMessage http_message;
    ASSERT_EQ((ssize_t)strlen(http_request), 
              http_message.ParseFromBytes(http_request, strlen(http_request)));
    ASSERT_TRUE(http_message.Completed());
    ASSERT_EQ(http_message.stage(), HTTP_ON_MESSAGE_COMPLETE);
    const HttpHeader& header = http_message.header();
    // Check all keys
    ASSERT_EQ("json", header.content_type());
    ASSERT_TRUE(header.GetHeader("HOST"));
    ASSERT_EQ("myhost", *header.GetHeader("host"));
    ASSERT_TRUE(header.GetHeader("CORRELATION-ID"));
    ASSERT_EQ("123", *header.GetHeader("CORRELATION-ID"));
    ASSERT_TRUE(header.GetHeader("User-Agent"));
    ASSERT_EQ("HTTPTool/1.0  ", *header.GetHeader("User-Agent"));
    ASSERT_TRUE(header.GetHeader("Host"));
    ASSERT_EQ("myhost", *header.GetHeader("Host"));
    ASSERT_TRUE(header.GetHeader("Accept"));
    ASSERT_EQ("*/*", *header.GetHeader("Accept"));
    
    ASSERT_EQ(1, header.major_version());
    ASSERT_EQ(34, header.minor_version());
    ASSERT_EQ(HTTP_METHOD_POST, header.method());
    ASSERT_EQ(HTTP_STATUS_OK, header.status_code());
    ASSERT_STREQ("OK", header.reason_phrase());

    ASSERT_TRUE(header.GetHeader("log-id"));
    ASSERT_EQ("456", *header.GetHeader("log-id"));
    ASSERT_TRUE(NULL != header.GetHeader("Authorization"));
    ASSERT_EQ("test", *header.GetHeader("Authorization"));
}

TEST(HttpMessageTest, response_sanity) {
    const char *http_response = 
        "HTTP/12.34 410 GoneBlah\r\n"
        "From: someuser@jmarshall.com\r\n"
        "User-Agent: HTTPTool/1.0  \r\n"  // intended ending spaces
        "Content-Type: json2\r\n"
        "Content-Length: 19\r\n"
        "Log-ID: 456\r\n"
        "Host: myhost\r\n"
        "Correlation-ID: 123\r\n"
        "Authorization: test\r\n"
        "Accept: */*\r\n"
        "\r\n"
        "Message Body sdfsdf\r\n"
    ;
    HttpMessage http_message;
    ASSERT_EQ((ssize_t)strlen(http_response), 
              http_message.ParseFromBytes(http_response, strlen(http_response)));
    // Check all keys
    const HttpHeader& header = http_message.header();
    ASSERT_EQ("json2", header.content_type());
    ASSERT_TRUE(header.GetHeader("HOST"));
    ASSERT_EQ("myhost", *header.GetHeader("host"));
    ASSERT_TRUE(header.GetHeader("CORRELATION-ID"));
    ASSERT_EQ("123", *header.GetHeader("CORRELATION-ID"));
    ASSERT_TRUE(header.GetHeader("User-Agent"));
    ASSERT_EQ("HTTPTool/1.0  ", *header.GetHeader("User-Agent"));
    ASSERT_TRUE(header.GetHeader("Host"));
    ASSERT_EQ("myhost", *header.GetHeader("Host"));
    ASSERT_TRUE(header.GetHeader("Accept"));
    ASSERT_EQ("*/*", *header.GetHeader("Accept"));
    
    ASSERT_EQ(1, header.major_version());
    ASSERT_EQ(34, header.minor_version());
    // method is undefined for response, in our case, it's set to 0.
    ASSERT_EQ(HTTP_METHOD_DELETE, header.method());
    ASSERT_EQ(HTTP_STATUS_GONE, header.status_code());
    ASSERT_STREQ(HttpReasonPhrase(header.status_code()), /*not GoneBlah*/
                 header.reason_phrase());
    
    ASSERT_TRUE(header.GetHeader("log-id"));
    ASSERT_EQ("456", *header.GetHeader("log-id"));
    ASSERT_TRUE(header.GetHeader("Authorization"));
    ASSERT_EQ("test", *header.GetHeader("Authorization"));
}

TEST(HttpMessageTest, eof) {
    const char* http_request = 
        "GET /CloudApiControl/HttpServer/telematics/v3/weather?location=%E6%B5%B7%E5%8D%97%E7%9C%81%E7%9B%B4%E8%BE%96%E5%8E%BF%E7%BA%A7%E8%A1%8C%E6%94%BF%E5%8D%95%E4%BD%8D&output=json&ak=0l3FSP6qA0WbOzGRaafbmczS HTTP/1.1\r\n"
        "X-Host: api.map.baidu.com\r\n"
        "X-Forwarded-Proto: http\r\n"
        "Host: api.map.baidu.com\r\n"
        "User-Agent: IME/Android/4.4.2/N80.QHD.LT.X10.V3/N80.QHD.LT.X10.V3.20150812.031915\r\n"
        "Accept: application/json\r\n"
        "Accept-Charset: UTF-8,*;q=0.5\r\n"
        "Accept-Encoding: deflate,sdch\r\n"
        "Accept-Language: zh-CN,en-US;q=0.8,zh;q=0.6\r\n"
        "Bfe-Atk: NORMAL_BROWSER\r\n"
        "Bfe_logid: 8767802212038413243\r\n"
        "Bfeip: 10.26.124.40\r\n"
        "CLIENTIP: 119.29.102.26\r\n"
        "CLIENTPORT: 59863\r\n"
        "Cache-Control: max-age=0\r\n"
        "Content-Type: application/json;charset=utf8\r\n"
        "X-Forwarded-For: 119.29.102.26\r\n"
        "X-Forwarded-Port: 59863\r\n"
        "X-Ime-Imei: 35629601890905\r\n"
        "X_BD_LOGID: 3959476981\r\n"
        "X_BD_LOGID64: 16815814797661447369\r\n"
        "X_BD_PRODUCT: map\r\n"
        "X_BD_SUBSYS: apimap\r\n";
    var::net::Buffer buf;
    buf.append(http_request);
    HttpMessage http_message;
    ASSERT_EQ((ssize_t)buf.readableBytes(), http_message.ParseFromBytes(buf.peek(), buf.readableBytes()));
    ASSERT_FALSE(http_message.Completed());
    ASSERT_EQ(2, http_message.ParseFromBytes("\r\n", 2));
    ASSERT_TRUE(http_message.Completed());
}

TEST(HttpMessageTest, bad_format) {
    const char *http_request =
        "slkdjflksdf skldjf\r\n";
    HttpMessage http_message;
    ASSERT_EQ(-1, http_message.ParseFromBytes(http_request, strlen(http_request)));
}

TEST(HttpMessageTest, incompleted_request_line)
{
    const char* http_request = 
        "POST /vars/bthread_count?series HTTP/12.34\r\n"
        "From: someuser@jmarshall.com\r\n"
        "User-Agent: HTTPTool/1.0  \r\n"
        "Content-Ty"
    ;
    HttpMessage http_message;
    ASSERT_TRUE(http_message.ParseFromBytes(http_request, strlen(http_request)) >= 0);
    ASSERT_FALSE(http_message.Completed());
}

TEST(HttpMessageTest, one_more_request_line)
{
    const char* http_request = 
        "POST /path/file.html?sdfsdf=sdfs&sldf1=sdf HTTP/12.34\r\n"
        "From: someuser@jmarshall.com\r\n"
        "User-Agent: HTTPTool/1.0  \r\n"  // intended ending spaces
        "Content-Type: json\r\n"
        "Content-Length: 20\r\n"
        "Log-ID: 456\r\n"
        "Host: myhost\r\n"
        "Correlation-ID: 123\r\n"
        "Authorization: test\r\n"
        "Accept: */*\r\n"
        "\r\n"
        "Message Body sdfsdfa\r\n"
        "GET /vars/bthread_count?series HTTP/1.1\r\n"
    ;
    HttpMessage http_message;
    size_t parsed_len = http_message.ParseFromBytes(http_request, strlen(http_request));
    ASSERT_EQ(parsed_len, strlen(http_request));
    HttpParserStage parsed_stage = http_message.stage();
    ASSERT_EQ(parsed_stage, HTTP_ON_URL);

    const char* http_request_more = 
        "User-Agent: HTTPTool/1.0  \r\n"  // intended ending spaces
        "Content-Type: json\r\n"
        "Content-Length: 20\r\n"
        "Log-ID: 456\r\n"
        "Host: myhost\r\n"
        "Correlation-ID: 123\r\n"
        "Authorization: test\r\n"
        "Accept: */*\r\n"
        "\r\n"
        "Message Body sdfsdfa\r\n"
    ;
    size_t parsed_more_len = http_message.ParseFromBytes(http_request_more, strlen(http_request_more));
    ASSERT_EQ(parsed_more_len, strlen(http_request_more));
    HttpParserStage parsed_stage_2 = http_message.stage();
    ASSERT_EQ(parsed_stage_2, HTTP_ON_MESSAGE_COMPLETE);
    ASSERT_TRUE(http_message.Completed());
}

TEST(HttpMessageTest, parse_from_iobuf) 
{
    const size_t content_length = 8192;
    char header[1024];
    snprintf(header, sizeof(header),
            "GET /service/method?key1=value1&key2=value2&key3=value3 HTTP/1.1\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %lu\r\n"
            "\r\n",
            content_length);
    std::string content;
    for (size_t i = 0; i < content_length; ++i) {
        content.push_back('2');
    }
    var::net::Buffer request;
    request.append(header);
    request.append(content);

    HttpMessage http_message;
    ASSERT_TRUE(http_message.ParseFromBytes(request.peek(), request.readableBytes()) >= 0);
    ASSERT_TRUE(http_message.Completed());
    ASSERT_EQ(content, http_message.body().retrieveAllAsString());
    ASSERT_EQ("text/plain", http_message.header().content_type());
}

TEST(HttpMessageTest, parse_http_header) 
{
    const char* http_request = 
        "POST foobar://user:passwd@www.baidu.com:80/var/bthread_count?wd1=url1&wd2=url2#frag HTTP/1.1\r\n"
        "From: someuser@jmarshall.com\r\n"
        "User-Agent: HTTPTool/1.0  \r\n"  
        "Content-Type: json\r\n"
        "Content-Length: 20\r\n"
        "Log-ID: 456\r\n"
        "Host: myhost\r\n"
        "Correlation-ID: 123\r\n"
        "Authorization: test\r\n"
        "Accept: */*\r\n"
        "\r\n"
        "Message Body sdfsdfa\r\n"
    ;
    HttpMessage http_message;
    ASSERT_EQ((ssize_t)strlen(http_request), 
              http_message.ParseFromBytes(http_request, strlen(http_request)));
    ASSERT_TRUE(http_message.Completed());
    const HttpHeader& header = http_message.header();
    const HttpUrl& url = header.url();
    ASSERT_EQ("foobar", url.scheme());
    ASSERT_EQ("user:passwd", url.user_info());
    ASSERT_EQ(80, url.port());
    ASSERT_EQ("www.baidu.com", url.host());
    ASSERT_EQ("/var/bthread_count", url.path());
    ASSERT_EQ("frag", url.fragment());
    ASSERT_TRUE(url.GetQuery("wd1"));
    ASSERT_EQ(*url.GetQuery("wd1"), "url1");
    ASSERT_TRUE(url.GetQuery("wd2"));
    ASSERT_EQ(*url.GetQuery("wd2"), "url2");
    ASSERT_FALSE(url.GetQuery("nonkey"));
}

TEST(HttpMessageTest, parse_brpc_request) 
{
    const char* http_request = 
        "GET /vars/bthread_count?series HTTP/1.1\r\n"
        "Host: 0.0.0.0:8000\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux aarch64; rv:78.0) Gecko/20100101 Firefox/78.0\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"
        "Accept-Language: zh-CN,zh;q=0.8,zh-TW;q=0.7,zh-HK;q=0.5,en-US;q=0.3,en;q=0.2\r\n"
        "Accept-Encoding: gzip, deflate\r\n"
        "X-Requested-With: XMLHttpRequest\r\n"
        "Connection: keep-alive\r\n"
        "Referer: http://0.0.0.0:8000/vars/\r\n"
        "Cookie: key=%24argon2i%24v%3D19%24m%3D4096%2Ct%3D3%2Cp%3D1%24UFk%2BiH2ZXvkDSdoqlycaVg%24%2FgdItxoX8GduV0PhrM28cALrDUjqHkGH488OQZ8KvZY\r\n"
        "Upgrade-Insecure-Requests: 1\r\n"
        "\r\n"
    ;
    HttpMessage http_message;
    ASSERT_EQ((ssize_t)strlen(http_request), 
              http_message.ParseFromBytes(http_request, strlen(http_request)));
    ASSERT_TRUE(http_message.Completed());
    ASSERT_EQ(http_message.stage(), HTTP_ON_MESSAGE_COMPLETE);
    const HttpHeader& header = http_message.header();
    ASSERT_EQ(HTTP_METHOD_GET, header.method());
    ASSERT_EQ(1, header.major_version());
    ASSERT_EQ(1, header.minor_version());
    ASSERT_EQ(HTTP_STATUS_OK, header.status_code());
    ASSERT_EQ("0.0.0.0:8000", *header.GetHeader("Host"));
    ASSERT_EQ("gzip, deflate", *header.GetHeader("Accept-Encoding"));
    ASSERT_EQ("XMLHttpRequest", *header.GetHeader("X-Requested-With"));
    ASSERT_EQ("keep-alive", *header.GetHeader("Connection"));
    ASSERT_EQ("http://0.0.0.0:8000/vars/", *header.GetHeader("Referer"));
    ASSERT_EQ("1", *header.GetHeader("Upgrader-Insecure-Requests"));
    const HttpUrl& url = header.url();
    ASSERT_EQ("/var/bthread_count", url.path());

}

TEST(HttpMessageTest, parse_series_http_message)
{
    HttpMessage* http_message = nullptr;
    // used for mock ref. 这里是为了不带包内容 提前解析包头
    bool ref = false;
    // Model.
    auto ParseHttpMessage = [&](var::net::Buffer& buf, bool read_eof) -> HttpMessage {
        if(http_message == nullptr) {
            // 1. read_eof：完整的HTTP消息后读取EOF，这是一个常见情况。
            // 请注意，除了not_enough_data之外，无法返回错误，否则socket将被设置为失败，并且仅在processHtttpxxx()中的消息就可以删除。
            // 2. Source->empty()：也很常见，InputMessage尝试解析处理程序，直到满足错误。如果消耗消息，来源可能是空的。
            if(read_eof || buf.readableBytes() == 0) {
                // Need more data.
                HttpMessage result;
                return result;
            }
            // Reset.
            http_message = new HttpMessage;
            ref = false;
        }
        ssize_t rc = 0;
        if(read_eof) {
            // Received eof, check comments in http_meesga.h
            rc = http_message->ParseFromBytes(NULL, 0);
        }
        else {
            // Resolved all buf's data.
            rc = http_message->ParseFromBytes(buf.peek(), buf.readableBytes());
        }

        // Resolved once data success.
        if(ref) {
            // Header部分已经被解析为processHTTPXXX的完整HTTP消息。在这里解析Body部分
            if(rc >= 0) {
                buf.retrieve(rc);
                if(http_message->Completed()) {
                    // Already returned the message before, don't return again.
                    ref = false;
                    delete http_message;
                    http_message = nullptr;
                    HttpMessage result;
                    return result;
                }
            }
            else {
                // Fail to parse the body, Since header were parsed successfully,
                // the message is assumed to be HTTP, stop trying other protocols.
                HttpMessage result;
                return result;
            }
        }
        else if(rc >= 0) {
            // http协议解析中即使source不包含一个完整的http消息，它也会被http parser消费掉，以避免下一次重复解析
            buf.retrieve(rc);
            if(http_message->Completed()) {
                HttpMessage result = *http_message;
                delete http_message;
                http_message = nullptr;
                return result;
            }
            else if(http_message->stage() >= HTTP_ON_HEADERS_COMPLETE) {
                // 返回的内容包含完整包头 但不包含内容 可以提前解析
                HttpMessage result = *http_message;
                ref = true;
                return result;
            }
            else {
                // 返回parsed_not_enough_data.
                HttpMessage result;
                return result;
            }
        }
    };

    size_t count = 10;
    const char* http_request = 
    "POST /path/file.html?sdfsdf=sdfs&sldf1=sdf HTTP/12.34\r\n"
    "From: someuser@jmarshall.com\r\n"
    "User-Agent: HTTPTool/1.0  \r\n"  // intended ending spaces
    "Content-Type: json\r\n"
    "Content-Length: 20\r\n"
    "Log-ID: 456\r\n"
    "Host: myhost\r\n"
    "Correlation-ID: 123\r\n"
    "Authorization: test\r\n"
    "Accept: */*\r\n"
    "\r\n"
    "Message Body sdfsdfa\r\n"
    ;
    var::net::Buffer buf;
    buf.append(http_request, strlen(http_request));
    while(buf.readableBytes() >= 0) {
        bool read_eof = buf.readableBytes() == 0 ? true : false;
        HttpMessage http_message = ParseHttpMessage(buf, read_eof);
        if(read_eof) {
            break;
        }
        if(http_message.Completed()) {
            ASSERT_EQ(0, buf.readableBytes());
            const HttpHeader& header = http_message.header();
            ASSERT_EQ("json", header.content_type());
            ASSERT_TRUE(header.GetHeader("HOST"));
            ASSERT_EQ("myhost", *header.GetHeader("host"));
        }
        buf.append(http_request, strlen(http_request));
        if(!count--) {
            break;
        }
    }
}