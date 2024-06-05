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
#include <iostream>

#include "net/http/http_parser.h"

class HttpParserTest : public testing::Test {
protected:
    void SetUp() {}
    void TearDown() {}
};

int OnMessageBegin(http_parser* parser)
{
    std::cout << "Start parsing message" << std::endl;
    return 0;
}

int OnUrl(http_parser* parser, const char* at, const size_t length)
{
    std::cout << "Get url: " << std::string(at,length) << std::endl;
    struct http_parser_url url;
    http_parser_url_init(&url);
    if(http_parser_parse_url(at,length,0,&url) == 0) {
        if(url.field_set & (1 << UF_PORT)) {
            std::cout << "  port: " << url.port << std::endl;
        }
        if(url.field_set & (1 << UF_HOST)) {
            char* resolved_host = (char*)malloc(url.field_data[UF_HOST].len + 1);
            strncpy(resolved_host, at + url.field_data[UF_HOST].off, url.field_data[UF_HOST].len);
            std::string host(resolved_host, url.field_data[UF_HOST].len);
            std::cout << " host: " << host << std::endl;
        }
        if(url.field_set & (1 << UF_PATH)) {
            char* resolved_path = (char*)malloc(url.field_data[UF_PATH].len + 1);
            strncpy(resolved_path, at + url.field_data[UF_HOST].off, url.field_data[UF_PATH].len);
            std::string path(at + url.field_data[UF_HOST].off, url.field_data[UF_PATH].len);
            std::cout << " path: " << path << std::endl;
        }
        if(url.field_set & (1 << UF_QUERY)) {
            std::string query(at + url.field_data[UF_QUERY].off, url.field_data[UF_QUERY].len);
            std::cout << " query: " << query << std::endl; 
        }
    }

    return 0;
}

int OnHeadersComplete(http_parser* parser)
{
    std::cout << "Header complete: " << std::endl;
    return 0; 
}

int OnMessageComplete(http_parser* parser)
{
    std::cout << "Message complete: " << std::endl;
    return 0;
}

int OnHeadersField(http_parser* parser, const char* at, const size_t length)
{
    std::cout << "Get header field: " << std::string(at,length) << std::endl;
    return 0;
}

int OnHeadersValue(http_parser* parser, const char* at, const size_t length)
{
    std::cout << "Get header value: " << std::string(at,length) << std::endl;
    return 0;
}

int OnBody(http_parser* parser, const char* at, const size_t length)
{
    std::cout << "Get body: " << std::string(at,length) << std::endl;
}

TEST_F(HttpParserTest, HttpExample)
{
    const char* http_request = 
        "GET /vars/bthread_count?series1=1&series2=2 HTTP/1.1\r\n"
        "Host: 192.168.74.160:8000,8001\r\n"
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36 Edg/125.0.0.0\r\n"
        "Accept: application/json, text/javascript, */*; q=0.01\r\n"
        "Accept-Encoding: gzip, deflate\r\n"
        "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6\r\n"
        "Referer: http://192.168.74.160:8000/vars\r\n"
        "X-Requested-With: XMLHttpRequest\r\n"
        "\r\n"
    ;

    // const char* http_request = 
    //     "GET /path/file.html?sdfsdf=sdfs HTTP/1.0\r\n"
    //     "From: someuser@jmarshall.com\r\n"
    //     "User-Agent: HTTPTool/1.0\r\n"
    //     "Content-Type: json\r\n"
    //     "Content-Length: 19\r\n"
    //     "Host: sdlfjslfd\r\n"
    //     "Accept: */*\r\n"
    //     "\r\n"
    //     "Message Body sdfsdf\r\n"
    // ;
    std::cout << "Wait resolved http request: " << http_request << std::endl;

    http_parser parser;
    http_parser_init(&parser,http_parser_type::HTTP_REQUEST);
    http_parser_settings settings;
    memset((char*)&settings,0,sizeof(settings));

    settings.on_message_begin = OnMessageBegin;
    settings.on_url = OnUrl;
    settings.on_headers_complete = OnHeadersComplete;
    settings.on_message_complete = OnMessageComplete;
    settings.on_header_field = OnHeadersField;
    settings.on_header_value = OnHeadersValue;
    settings.on_body = OnBody;

    std::cout << http_parser_execute(&parser,&settings,http_request,strlen(http_request)) << std::endl;
}