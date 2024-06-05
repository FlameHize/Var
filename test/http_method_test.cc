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
#include <net/http/http_method.h>

using namespace var;

class HttpMethodTest : public testing::Test {
protected:
    void SetUp() {}
    void TearDown() {}
};

TEST_F(HttpMethodTest, HttpMethod)
{
    ASSERT_STREQ("DELETE",HttpMethod2Str(HTTP_METHOD_DELETE));
    ASSERT_STREQ("GET", HttpMethod2Str(HTTP_METHOD_GET));
    ASSERT_STREQ("PUT", HttpMethod2Str(HTTP_METHOD_PUT));
    ASSERT_STREQ("POST", HttpMethod2Str(HTTP_METHOD_POST));

    HttpMethod method;
    ASSERT_TRUE(Str2HttpMethod("DELETE", &method));
    ASSERT_EQ(HTTP_METHOD_DELETE, method);
    ASSERT_TRUE(Str2HttpMethod("GET", &method));
    ASSERT_EQ(HTTP_METHOD_GET, method);
    ASSERT_TRUE(Str2HttpMethod("PUT", &method));
    ASSERT_EQ(HTTP_METHOD_PUT, method);
    ASSERT_TRUE(Str2HttpMethod("POST", &method));
    ASSERT_EQ(HTTP_METHOD_POST, method);

    ASSERT_TRUE(Str2HttpMethod("delete", &method));
    ASSERT_EQ(HTTP_METHOD_DELETE, method);
    ASSERT_TRUE(Str2HttpMethod("get", &method));
    ASSERT_EQ(HTTP_METHOD_GET, method);
    ASSERT_TRUE(Str2HttpMethod("put", &method));
    ASSERT_EQ(HTTP_METHOD_PUT, method);
    ASSERT_TRUE(Str2HttpMethod("post", &method));
    ASSERT_EQ(HTTP_METHOD_POST, method);

    ASSERT_FALSE(Str2HttpMethod("Del", &method));
    ASSERT_FALSE(Str2HttpMethod("GoT ", &method));
    ASSERT_FALSE(Str2HttpMethod("PUT ", &method));
    ASSERT_FALSE(Str2HttpMethod("POST ", &method));
}
