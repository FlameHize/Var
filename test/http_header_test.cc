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
#include "net/http/http_header.h"

using namespace var;
using namespace var::net;

TEST(HttpHeaderTest, http_header)
{
    HttpHeader header;

    header.set_version(1, 2);
    ASSERT_EQ(1, header.major_version());
    ASSERT_EQ(2, header.minor_version());

    ASSERT_TRUE(header.content_type().empty());
    header.set_content_type("text/plain");
    ASSERT_EQ("text/plain", header.content_type());
    ASSERT_FALSE(header.GetHeader("content-type"));
    header.set_content_type("application/json");
    ASSERT_EQ("application/json", header.content_type());
    ASSERT_FALSE(header.GetHeader("content-type"));

    ASSERT_FALSE(header.GetHeader("key1"));
    header.AppendHeader("key1", "value1");
    const std::string* value = header.GetHeader("key1");
    ASSERT_TRUE(value && *value == "value1");
    header.AppendHeader("key1", "value2");
    value = header.GetHeader("key1");
    ASSERT_TRUE(value && *value == "value1,value2");
    header.SetHeader("key1", "value3");
    value = header.GetHeader("key1");
    ASSERT_TRUE(value && *value == "value3");
    header.RemoveHeader("key1");
    ASSERT_FALSE(header.GetHeader("key1"));

    header.set_unresolved_path("Foo/Bar");
    ASSERT_EQ("Foo/Bar", header.unresolved_path());

    ASSERT_EQ(HTTP_METHOD_GET, header.method());
    header.set_method(HTTP_METHOD_POST);
    ASSERT_EQ(HTTP_METHOD_POST, header.method());

    ASSERT_EQ(HTTP_STATUS_OK, header.status_code());
    ASSERT_STREQ(HttpReasonPhrase(header.status_code()),
                 header.reason_phrase());
    header.set_status_code(HTTP_STATUS_CONTINUE);
    ASSERT_EQ(HTTP_STATUS_CONTINUE, header.status_code());
    ASSERT_STREQ(HttpReasonPhrase(header.status_code()),
                 header.reason_phrase());
    
    header.set_status_code(HTTP_STATUS_GONE);
    ASSERT_EQ(HTTP_STATUS_GONE, header.status_code());
    ASSERT_STREQ(HttpReasonPhrase(header.status_code()),
                 header.reason_phrase());
}