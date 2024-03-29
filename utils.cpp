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
#include <algorithm>
#include <cctype>
#include <locale>
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

static vector<string> FindHeaderValues(const map<string, vector<string>>& headers, const string& key) {
    const auto lower_key = ToLowerASCII(key);
    for (const auto& entry : headers) {
        if (lower_key == ToLowerASCII(entry.first)) {
            return entry.second;
        }
    }
    return {};
}

string FindHeaderValue(const map<string, vector<string>>& headers, const string& key) {
    auto vec = FindHeaderValues(headers, key);
    return vec.empty() ? "" : vec.front();
}

string GetCookies(const map<string, vector<string>>& headers) {
    // Set-Cookie header values are of the form:
    // AWSALB=abcxyz; Expires=Tue, 03 May 2022 19:47:19 GMT; Path=/
    // We only care about the cookie name and the value.

    stringstream res;
    bool first = true;
    for (const auto& c : FindHeaderValues(headers, "Set-Cookie")) {
        if (!first) {
            res << "; ";
        }
        first = false;

        auto semi = c.find_first_of(';');
        res << TrimCopy(c.substr(0, semi));
    }
    return res.str();
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

// From https://stackoverflow.com/a/217605/729729
void TrimLeft(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

void TrimRight(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

void Trim(std::string &s) {
    TrimLeft(s);
    TrimRight(s);
}

std::string TrimLeftCopy(std::string s) {
    TrimLeft(s);
    return s;
}

std::string TrimRightCopy(std::string s) {
    TrimRight(s);
    return s;
}

std::string TrimCopy(std::string s) {
    Trim(s);
    return s;
}

}
