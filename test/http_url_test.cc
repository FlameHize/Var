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

TEST(HttpUrlTest, everything)
{
    var::HttpUrl url;
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

TEST(HttpUrlTest, parse_url)
{
    var::HttpUrl url;
    std::string url_str = " foobar://user:passwd@www.baidu.com:80/var/bthread_count?wd1=url1&wd2=url2#frag  ";
    std::string scheme;
    std::string host_out;
    int port_out = -1;
    var::ParseURL(url_str.c_str(), &scheme, &host_out, &port_out);
    ASSERT_EQ("foobar", scheme);
    ASSERT_EQ("www.baidu.com", host_out);
    ASSERT_EQ(80, port_out);
}

TEST(HttpUrlTest, only_host) 
{
    var::HttpUrl url;
    ASSERT_EQ(0, url.ResolvedHttpURL("  foo1://www.baidu1.com?wd=uri2&nonkey=22 "));
    ASSERT_EQ("foo1", url.scheme());
    ASSERT_EQ(-1, url.port());
    ASSERT_EQ("www.baidu1.com", url.host());
    ASSERT_EQ("", url.path());
    ASSERT_EQ("", url.user_info());
    ASSERT_EQ("", url.fragment());
    ASSERT_EQ(2u, url.QueryCount());
    ASSERT_TRUE(url.GetQuery("wd"));
    ASSERT_EQ(*url.GetQuery("wd"), "uri2");
    ASSERT_TRUE(url.GetQuery("nonkey"));
    ASSERT_EQ(*url.GetQuery("nonkey"), "22");

    ASSERT_EQ(0, url.ResolvedHttpURL("foo2://www.baidu2.com:1234?wd=uri2&nonkey=22 "));
    ASSERT_EQ("foo2", url.scheme());
    ASSERT_EQ(1234, url.port());
    ASSERT_EQ("www.baidu2.com", url.host());
    ASSERT_EQ("", url.path());
    ASSERT_EQ("", url.user_info());
    ASSERT_EQ("", url.fragment());
    ASSERT_EQ(2u, url.QueryCount());
    ASSERT_TRUE(url.GetQuery("wd"));
    ASSERT_EQ(*url.GetQuery("wd"), "uri2");
    ASSERT_TRUE(url.GetQuery("nonkey"));
    ASSERT_EQ(*url.GetQuery("nonkey"), "22");

    ASSERT_EQ(0, url.ResolvedHttpURL(" www.baidu3.com:4321 "));
    ASSERT_EQ("", url.scheme());
    ASSERT_EQ(4321, url.port());
    ASSERT_EQ("www.baidu3.com", url.host());
    ASSERT_EQ("", url.path());
    ASSERT_EQ("", url.user_info());
    ASSERT_EQ("", url.fragment());
    ASSERT_EQ(0u, url.QueryCount());
    
    ASSERT_EQ(0, url.ResolvedHttpURL(" www.baidu4.com "));
    ASSERT_EQ("", url.scheme());
    ASSERT_EQ(-1, url.port());
    ASSERT_EQ("www.baidu4.com", url.host());
    ASSERT_EQ("", url.path());
    ASSERT_EQ("", url.user_info());
    ASSERT_EQ("", url.fragment());
    ASSERT_EQ(0u, url.QueryCount());
}

TEST(HttpUrlTest, no_scheme) 
{
    // http parser_paser_url() can't ignore of scheme like 'http://'
    var::HttpUrl url;
    ASSERT_EQ(0, url.ResolvedHttpURL(" user:passwd2@www.baidu1.com/s?wd=uri2&nonkey=22#frag "));
    ASSERT_EQ("", url.scheme());
    ASSERT_EQ("user:passwd2", url.user_info());
    ASSERT_EQ(-1, url.port());
    ASSERT_EQ("www.baidu1.com", url.host());
    ASSERT_EQ("/s", url.path());
    ASSERT_EQ("frag", url.fragment());
    ASSERT_TRUE(url.GetQuery("wd"));
    ASSERT_EQ(*url.GetQuery("wd"), "uri2");
    ASSERT_TRUE(url.GetQuery("nonkey"));
    ASSERT_EQ(*url.GetQuery("nonkey"), "22");
}

TEST(HttpUrlTest, no_scheme_and_user_info) 
{
    var::HttpUrl url;
    ASSERT_EQ(0, url.ResolvedHttpURL(" www.baidu2.com/s?wd=uri2&nonkey=22#frag "));
    ASSERT_EQ("", url.scheme());
    ASSERT_EQ(-1, url.port());
    ASSERT_EQ("www.baidu2.com", url.host());
    ASSERT_EQ("/s", url.path());
    ASSERT_EQ("", url.user_info());
    ASSERT_EQ("frag", url.fragment());
    ASSERT_TRUE(url.GetQuery("wd"));
    ASSERT_EQ(*url.GetQuery("wd"), "uri2");
    ASSERT_TRUE(url.GetQuery("nonkey"));
    ASSERT_EQ(*url.GetQuery("nonkey"), "22");
}

TEST(HttpUrlTest, no_host) {
    var::HttpUrl url;
    ASSERT_EQ(0, url.ResolvedHttpURL(" /sb?wd=uri3#frag2 "));
    ASSERT_EQ("", url.scheme());
    ASSERT_EQ(-1, url.port());
    ASSERT_EQ("", url.host());
    ASSERT_EQ("/sb", url.path());
    ASSERT_EQ("", url.user_info());
    ASSERT_EQ("frag2", url.fragment());
    ASSERT_TRUE(url.GetQuery("wd"));
    ASSERT_EQ(*url.GetQuery("wd"), "uri3");
    ASSERT_FALSE(url.GetQuery("nonkey"));

    // set_path should do as its name says.
    url.set_path("/x/y/z/");
    ASSERT_EQ("", url.scheme());
    ASSERT_EQ(-1, url.port());
    ASSERT_EQ("", url.host());
    ASSERT_EQ("/x/y/z/", url.path());
    ASSERT_EQ("", url.user_info());
    ASSERT_EQ("frag2", url.fragment());
    ASSERT_TRUE(url.GetQuery("wd"));
    ASSERT_EQ(*url.GetQuery("wd"), "uri3");
    ASSERT_FALSE(url.GetQuery("nonkey"));
}

TEST(HttpUrlTest, consecutive_ampersand) 
{
    var::HttpUrl url;
    url.set_query("&key1=value1&&key3=value3");
    ASSERT_TRUE(url.GetQuery("key1"));
    ASSERT_TRUE(url.GetQuery("key3"));
    ASSERT_FALSE(url.GetQuery("key2"));
    ASSERT_EQ("value1", *url.GetQuery("key1"));
    ASSERT_EQ("value3", *url.GetQuery("key3"));
}

TEST(HttpUrlTest, only_equality) 
{
    var::HttpUrl url;
    url.set_query("key1=&&key2&&=&key3=value3");
    ASSERT_TRUE(url.GetQuery("key1"));
    ASSERT_EQ("", *url.GetQuery("key1"));
    ASSERT_TRUE(url.GetQuery("key2"));
    ASSERT_EQ("", *url.GetQuery("key2"));
    ASSERT_TRUE(url.GetQuery("key3"));
    ASSERT_EQ("value3", *url.GetQuery("key3"));
}

TEST(HttpUrlTest, set_query) {
    var::HttpUrl url;
    url.set_query("key1=&&key2&&=&key3=value3");
    ASSERT_TRUE(url.GetQuery("key1"));
    ASSERT_TRUE(url.GetQuery("key3"));
    ASSERT_EQ("value3", *url.GetQuery("key3"));
    ASSERT_TRUE(url.GetQuery("key2"));
    // overwrite value
    url.SetQuery("key3", "value4");
    ASSERT_EQ("value4", *url.GetQuery("key3"));

    url.SetQuery("key2", "value2");
    ASSERT_TRUE(url.GetQuery("key2"));
    ASSERT_EQ("value2", *url.GetQuery("key2"));
}

TEST(HttpUrlTest, resolved_http_path) 
{
    var::HttpUrl url;
    url.ResolvedHttpPath("/dir?key1=&&key2&&=&key3=value3");
    ASSERT_EQ("/dir", url.path());
    ASSERT_TRUE(url.GetQuery("key1"));
    ASSERT_TRUE(url.GetQuery("key2"));
    ASSERT_TRUE(url.GetQuery("key3"));
    ASSERT_EQ("value3", *url.GetQuery("key3"));

    url.ResolvedHttpPath("dir?key1=&&key2&&=&key3=value3");
    ASSERT_EQ("dir", url.path());
    ASSERT_TRUE(url.GetQuery("key1"));
    ASSERT_TRUE(url.GetQuery("key2"));
    ASSERT_TRUE(url.GetQuery("key3"));
    ASSERT_EQ("value3", *url.GetQuery("key3"));

    url.ResolvedHttpPath("/dir?key1=&&key2&&=&key3=value3#frag1");
    ASSERT_EQ("/dir", url.path());
    ASSERT_TRUE(url.GetQuery("key1"));
    ASSERT_TRUE(url.GetQuery("key2"));
    ASSERT_TRUE(url.GetQuery("key3"));
    ASSERT_EQ("value3", *url.GetQuery("key3"));
    ASSERT_EQ("frag1", url.fragment());
}

TEST(HttpUrlTest, generate_http_path) 
{
    var::HttpUrl url;
    const std::string ref1 = "/dir?key1=&&key2&&=&key3=value3";
    url.ResolvedHttpPath(ref1);
    ASSERT_EQ("/dir", url.path());
    ASSERT_EQ(3u, url.QueryCount());
    ASSERT_TRUE(url.GetQuery("key1"));
    ASSERT_TRUE(url.GetQuery("key2"));
    ASSERT_TRUE(url.GetQuery("key3"));
    ASSERT_EQ("value3", *url.GetQuery("key3"));
    std::string path1;
    url.GenerateHttpPath(&path1);
    ASSERT_EQ(ref1, path1);

    // key3=value3.3&key2 error in map range
    // url.SetQuery("key3", "value3.3");
    // ASSERT_EQ(3u, url.QueryCount());
    // ASSERT_EQ(1u, url.RemoveQuery("key1"));
    // ASSERT_EQ(2u, url.QueryCount());
    // ASSERT_EQ("key2&key3=value3.3", url.query());
    // url.GenerateHttpPath(&path1);
    // ASSERT_EQ("/dir?key2&key3=value3.3", path1);    

    const std::string ref2 = "/dir2?key1=&&key2&&=&key3=value3#frag2";
    url.ResolvedHttpPath(ref2);
    ASSERT_EQ("/dir2", url.path());
    ASSERT_TRUE(url.GetQuery("key1"));
    ASSERT_TRUE(url.GetQuery("key2"));
    ASSERT_TRUE(url.GetQuery("key3"));
    ASSERT_EQ("value3", *url.GetQuery("key3"));
    ASSERT_EQ("frag2", url.fragment());
    std::string path2;
    url.GenerateHttpPath(&path2);
    ASSERT_EQ(ref2, path2);

    const std::string ref3 = "/dir3#frag3";
    url.ResolvedHttpPath(ref3);
    ASSERT_EQ("/dir3", url.path());
    ASSERT_EQ("frag3", url.fragment());
    std::string path3;
    url.GenerateHttpPath(&path3);
    ASSERT_EQ(ref3, path3);

    const std::string ref4 = "/dir4";
    url.ResolvedHttpPath(ref4);
    ASSERT_EQ("/dir4", url.path());
    std::string path4;
    url.GenerateHttpPath(&path4);
    ASSERT_EQ(ref4, path4);
}

TEST(HttpUrlTest, only_one_key) 
{
    var::HttpUrl url;
    url.set_query("key1");
    ASSERT_TRUE(url.GetQuery("key1"));
    ASSERT_EQ("", *url.GetQuery("key1"));
}

TEST(HttpUrlTest, empty_host) 
{
    var::HttpUrl url;
    ASSERT_EQ(0, url.ResolvedHttpURL("http://"));
    ASSERT_EQ("", url.host());
    ASSERT_EQ("", url.path());
}

TEST(HttpUrlTest, invalid_query) 
{
    var::HttpUrl url;
    ASSERT_EQ(0, url.ResolvedHttpURL("http://a.b.c/?a-b-c:def"));
    ASSERT_EQ("a-b-c:def", url.query());
}

TEST(HttpUrlTest, print_url) 
{
    var::HttpUrl url;

    const std::string url1 = "http://user:passwd@a.b.c/?d=c&a=b&e=f#frg1";
    ASSERT_EQ(0, url.ResolvedHttpURL(url1));
    std::ostringstream oss;
    url.Print(oss);
    ASSERT_EQ("http://a.b.c/?d=c&a=b&e=f#frg1", oss.str());
    oss.str("");
    url.PrintWithoutHost(oss);
    ASSERT_EQ("/?d=c&a=b&e=f#frg1", oss.str());

    const std::string url2 = "http://a.b.c/?d=c&a=b&e=f#frg1";
    ASSERT_EQ(0, url.ResolvedHttpURL(url2));
    oss.str("");
    url.Print(oss);
    ASSERT_EQ(url2, oss.str());
    oss.str("");
    url.PrintWithoutHost(oss);
    ASSERT_EQ("/?d=c&a=b&e=f#frg1", oss.str());

    // // range error
    // url.SetQuery("e", "f2");
    // url.SetQuery("f", "g");
    // ASSERT_EQ((size_t)1, url.RemoveQuery("a"));
    // oss.str("");
    // url.Print(oss);
    // ASSERT_EQ("http://a.b.c/?d=c&e=f2&f=g#frg1", oss.str());
    // oss.str("");
    // url.PrintWithoutHost(oss);
    // ASSERT_EQ("/?d=c&e=f2&f=g#frg1", oss.str());
}