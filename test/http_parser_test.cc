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
        "GET /path/file.html?sdfsdf=sdfs HTTP/1.0\r\n"
        "From: someuser@jmarshall.com\r\n"
        "User-Agent: HTTPTool/1.0\r\n"
        "Content-Type: json\r\n"
        "Content-Length: 19\r\n"
        "Host: sdlfjslfd\r\n"
        "Accept: */*\r\n"
        "\r\n"
        "Message Body sdfsdf\r\n"
    ;
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