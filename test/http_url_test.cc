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
#include "net/http/http_url.h"

TEST(URLTest, everything)
{
    var::URL url;
    std::string url_str = " foobar://user:passwd@www.baidu.com:80/var/bthread_count?wd1=url1&wd2=url2#frag  ";
    ASSERT_EQ(0, url.ResolvedHttpURL(url_str));
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

    std::string scheme;
    std::string host_out;
    int port_out = -1;
    var::ParseURL(url_str.c_str(), &scheme, &host_out, &port_out);
    ASSERT_EQ("foobar", scheme);
    ASSERT_EQ("www.baidu.com", host_out);
    ASSERT_EQ(80, port_out);
}