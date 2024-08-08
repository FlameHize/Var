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

///@todo need to check higher and lower
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
    var::HttpMessage http_message;
    ASSERT_EQ((ssize_t)strlen(http_request), 
              http_message.ParseFromBytes(http_request, strlen(http_request)));
    ASSERT_TRUE(http_message.Completed());
    ASSERT_EQ(http_message.stage(), var::HTTP_ON_MESSAGE_COMPLETE);
    const var::HttpHeader& header = http_message.header();
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
    ASSERT_EQ(var::HTTP_METHOD_POST, header.method());
    ASSERT_EQ(var::HTTP_STATUS_OK, header.status_code());
    ASSERT_STREQ("OK", header.reason_phrase());

    ASSERT_TRUE(header.GetHeader("log-id"));
    ASSERT_EQ("456", *header.GetHeader("log-id"));
    ASSERT_TRUE(NULL != header.GetHeader("Authorization"));
    ASSERT_EQ("test", *header.GetHeader("Authorization"));
}

TEST(HttpMessageTest, incompleted_request_line)
{
    const char* http_request = 
        "POST /vars/bthread_count?series HTTP/12.34\r\n"
        "From: someuser@jmarshall.com\r\n"
        "User-Agent: HTTPTool/1.0  \r\n"
        "Content-Ty"
    ;
    var::HttpMessage http_message;
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
    var::HttpMessage http_message;
    size_t parsed_len = http_message.ParseFromBytes(http_request, strlen(http_request));
    ASSERT_EQ(parsed_len, strlen(http_request));
    var::HttpParserStage parsed_stage = http_message.stage();
    ASSERT_EQ(parsed_stage, var::HTTP_ON_URL);

    // 流式处理第二部分
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
    var::HttpParserStage parsed_stage_2 = http_message.stage();
    ASSERT_EQ(parsed_stage_2, var::HTTP_ON_MESSAGE_COMPLETE);
    ASSERT_TRUE(http_message.Completed());
}

TEST(HttpMessageTest, parse_from_iobuf) {
    const size_t content_length = 8192;
    char header[1024];
    snprintf(header, sizeof(header),
            "GET /service/method?key1=value1&key2=value2&key3=value3 HTTP/1.1\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %lu\r\n"
            "\r\n",
            content_length);
    std::string content;
    for (size_t i = 0; i < content_length; ++i) content.push_back('2');
    var::IOBuf request;
    request.append(header);
    request.append(content);

    var::HttpMessage http_message;
    ASSERT_TRUE(http_message.ParseFromBytes(request.peek(), request.readableBytes()) >= 0);
    ASSERT_TRUE(http_message.Completed());
    ASSERT_EQ(content, http_message.body().retrieveAllAsString());
    ASSERT_EQ("text/plain", http_message.header().content_type());
}