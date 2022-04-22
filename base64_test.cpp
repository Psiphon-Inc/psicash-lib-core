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
#include "base64.hpp"

using namespace std;
using namespace base64;

TEST(TestBase64, Encode)
{
  // Test vectors: https://tools.ietf.org/html/rfc4648#page-12
  auto b64 = B64Encode("");
  ASSERT_EQ(b64, "");

  b64 = B64Encode("f");
  ASSERT_EQ(b64, "Zg==");

  b64 = B64Encode("fo");
  ASSERT_EQ(b64, "Zm8=");

  b64 = B64Encode("foo");
  ASSERT_EQ(b64, "Zm9v");

  b64 = B64Encode("foob");
  ASSERT_EQ(b64, "Zm9vYg==");

  b64 = B64Encode("fooba");
  ASSERT_EQ(b64, "Zm9vYmE=");

  b64 = B64Encode("foobar");
  ASSERT_EQ(b64, "Zm9vYmFy");

  // The vector overload
  vector<uint8_t> v;
  b64 = B64Encode(v);
  ASSERT_EQ(b64, "");

  uint8_t b[] = "f";
  v = vector<uint8_t>(b, b+1);
  b64 = B64Encode(v);
  ASSERT_EQ(b64, "Zg==");
}

TEST(TestBase64, Decode)
{
  vector<uint8_t> v, want;
  string s;

  s = "";
  want = vector<uint8_t>(s.c_str(), s.c_str()+s.size());
  v = B64Decode("");
  ASSERT_EQ(v, want);

  s = "fo";
  want = vector<uint8_t>(s.c_str(), s.c_str()+s.size());
  v = B64Decode("Zm8=");
  ASSERT_EQ(v, want);

  s = "foo";
  want = vector<uint8_t>(s.c_str(), s.c_str()+s.size());
  v = B64Decode("Zm9v");
  ASSERT_EQ(v, want);

  s = "foob";
  want = vector<uint8_t>(s.c_str(), s.c_str()+s.size());
  v = B64Decode("Zm9vYg==");
  ASSERT_EQ(v, want);

  s = "fooba";
  want = vector<uint8_t>(s.c_str(), s.c_str()+s.size());
  v = B64Decode("Zm9vYmE=");
  ASSERT_EQ(v, want);

  s = "foobar";
  want = vector<uint8_t>(s.c_str(), s.c_str()+s.size());
  v = B64Decode("Zm9vYmFy");
  ASSERT_EQ(v, want);

  // Not padded
  s = "foob";
  want = vector<uint8_t>(s.c_str(), s.c_str()+s.size());
  v = B64Decode("Zm9vYg");
  ASSERT_EQ(v, want);
}

TEST(TestBase64, TrimPadding)
{
  ASSERT_EQ(TrimPadding("abc"), "abc");
  ASSERT_EQ(TrimPadding("abc="), "abc");
  ASSERT_EQ(TrimPadding("abc=="), "abc");
  ASSERT_EQ(TrimPadding("abc==="), "abc");
}
