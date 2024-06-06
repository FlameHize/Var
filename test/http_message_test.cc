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
const char *http_request = 
        "POST /path/file.html?sdfsdf=sdfs&sldf1=sdf HTTP/12.34\r\n"
        "From: someuser@jmarshall.com\r\n"
        "User-Agent: HTTPTool/1.0  \r\n"  // intended ending spaces
        "Content-Type: json\r\n"
        "Content-Length: 19\r\n"
        "Log-ID: 456\r\n"
        "Host: myhost\r\n"
        "Correlation-ID: 123\r\n"
        "Authorization: test\r\n"
        "Accept: */*\r\n"
        "\r\n"
        "Message Body sdfsdf\r\n"
    ;
    var::HttpMessage http_message;
    ASSERT_EQ((ssize_t)strlen(http_request), 
              http_message.ParseFromBytes(http_request, strlen(http_request)));
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