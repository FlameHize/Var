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
#include "net/http/http_status_code.h"

using namespace net;

class HttpStatusCodeTest : public testing::Test {
protected:
    void SetUp() {}
    void TearDown() {}
};

TEST_F(HttpStatusCodeTest, HttpStatusCode)
{
    ASSERT_STREQ("Continue", HttpReasonPhrase(HTTP_STATUS_CONTINUE));
    ASSERT_STREQ("OK", HttpReasonPhrase(HTTP_STATUS_OK));
    ASSERT_STREQ("Found", HttpReasonPhrase(HTTP_STATUS_FOUND));
    ASSERT_STREQ("Not Found", HttpReasonPhrase(HTTP_STATUS_NOT_FOUND));
    ASSERT_STREQ("Bad Gateway", HttpReasonPhrase(HTTP_STATUS_BAD_GATEWAY));
}
