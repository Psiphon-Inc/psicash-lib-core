/*
 * Copyright (c) 2022, Psiphon Inc.
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "utils.hpp"

using namespace std;
using namespace utils;
using namespace testing;

TEST(TestStringer, SingleValue) {
    auto s = Stringer("s");
    ASSERT_EQ(s, "s");

    s = Stringer(123);
    ASSERT_EQ(s, "123");
}

TEST(TestStringer, MultiValue) {
    auto s = Stringer("one", 2, "three", 4, '5', '!');
    ASSERT_EQ(s, "one2three45!");
}

TEST(TestRandomID, Simple) {
    auto s = RandomID();
    ASSERT_EQ(s.length(), 48);
}

TEST(TestFindHeaderValue, Simple) {
    map<string, vector<string>> headers;

    headers = {{"a", {"xyz"}}, {"Date", {"expected", "second"}}, {"c", {"abc", "def"}}};
    auto s = FindHeaderValue(headers, "Date");
    ASSERT_EQ(s, "expected");

    headers = {{"date", {"expected", "second"}}, {"a", {"xyz"}}, {"c", {"abc", "def"}}};
    s = FindHeaderValue(headers, "Date");
    ASSERT_EQ(s, "expected");

    headers = {{"a", {"xyz"}}, {"c", {"abc", "def"}}, {"DATE", {"expected", "second"}}};
    s = FindHeaderValue(headers, "Date");
    ASSERT_EQ(s, "expected");

    headers = {{"a", {"xyz"}}, {"c", {"abc", "def"}}, {"DATE", {"expected", "second"}}};
    s = FindHeaderValue(headers, "Nope");
    ASSERT_EQ(s, "");
}

TEST(TestGetCookies, Simple) {
    map<string, vector<string>> headers;

    headers = {{"a", {"xyz"}}, {"set-COOKIE", {"AWSALBCORS=qxg5PeVRnxutG8kvdnISQvQM+PWqFzqoVZGJcyZh9c6su3O+u1121WEFwZ6DAEtVaKq6ufOzUIfAL8qRmUuSya5ODUxJOC9m3+006HBi71pSk6T88oiMgva0IOvi; Expires=Mon, 02 May 2022 20:53:02 GMT; Path=/; SameSite=None; Secure", "k1=v1", "k2=v2;"}}};
    auto v = GetCookies(headers);
    ASSERT_EQ(v, "AWSALBCORS=qxg5PeVRnxutG8kvdnISQvQM+PWqFzqoVZGJcyZh9c6su3O+u1121WEFwZ6DAEtVaKq6ufOzUIfAL8qRmUuSya5ODUxJOC9m3+006HBi71pSk6T88oiMgva0IOvi; k1=v1; k2=v2");

    headers = {{"a", {"xyz"}}};
    v = GetCookies(headers);
    ASSERT_EQ(v, "");

    headers = {{"a", {"xyz"}}, {"Set-Cookie", {}}};
    v = GetCookies(headers);
    ASSERT_EQ(v, "");

    headers = {{"a", {"xyz"}}, {"Set-Cookie", {""}}};
    v = GetCookies(headers);
    ASSERT_EQ(v, "");

    headers = {{"a", {"xyz"}}, {"Set-Cookie", {";"}}};
    v = GetCookies(headers);
    ASSERT_EQ(v, "");

    headers = {{"a", {"xyz"}}, {"Set-Cookie", {"!;!;!"}}};
    v = GetCookies(headers);
    ASSERT_EQ(v, "!");

    headers = {{"a", {"xyz"}}, {"Set-Cookie", {" x=y "}}};
    v = GetCookies(headers);
    ASSERT_EQ(v, "x=y");
}
