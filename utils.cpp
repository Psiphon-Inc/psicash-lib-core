/*
 * Copyright (c) 2019, Psiphon Inc.
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

#include <random>
#include <iostream>
#include <fstream>
#include <limits>
#include "utils.hpp"
#include "error.hpp"

#ifdef _WIN32
   #include <io.h>
   #define access    _access_s
#else
   #include <unistd.h>
#endif

using namespace std;
using namespace psicash;

namespace utils {

// From https://stackoverflow.com/a/33486052
bool FileExists(const std::string& filename)
{
    return access(filename.c_str(), 0) == 0;
}

static const std::vector<char> kRandomStringChars =
   {
      '0','1','2','3','4',
      '5','6','7','8','9',
      'A','B','C','D','E','F',
      'G','H','I','J','K',
      'L','M','N','O','P',
      'Q','R','S','T','U',
      'V','W','X','Y','Z',
      'a','b','c','d','e','f',
      'g','h','i','j','k',
      'l','m','n','o','p',
      'q','r','s','t','u',
      'v','w','x','y','z'
   };

// From https://stackoverflow.com/a/24586587
std::string RandomID() {
   static auto& chrs = "0123456789"
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

   // A length of 48 and a space of 62 characters gives us:
   //    log(62^48) / log(2) = 285 bits of entropy
   std::string::size_type length = 48;

   thread_local static std::mt19937 rg{std::random_device{}()};
   thread_local static std::uniform_int_distribution<std::string::size_type> pick(0, sizeof(chrs) - 2);

   std::string s;

   s.reserve(length);

   while (length--) {
      s += chrs[pick(rg)];
   }

   return s;
}

// Note that this _only_ works for plain ASCII strings.
static string ToLowerASCII(const string& s) {
   ostringstream ss;
   for (auto c : s) {
       ss << tolower(c);
   }
   return ss.str();
}

string FindHeaderValue(const map<string, vector<string>>& headers, const string& key) {
    auto lower_key = ToLowerASCII(key);
    for (const auto& entry : headers) {
        if (lower_key == ToLowerASCII(entry.first)) {
            return entry.second.empty() ? "" : entry.second.front();
        }
    }
    return "";
}

// Adapted from https://stackoverflow.com/a/22986486/729729
error::Error FileSize(const string& path, uint64_t& o_size) {
    o_size = 0;

    ifstream f;
    f.open(path, ios::in | ios::binary);
    if (!f) {
        return error::MakeCriticalError(utils::Stringer("file open failed; errno=", errno));
    }

    f.ignore(numeric_limits<streamsize>::max());
    auto length = f.gcount();
    f.close();

    o_size = (uint64_t)length;
    return error::nullerr;
}

}
