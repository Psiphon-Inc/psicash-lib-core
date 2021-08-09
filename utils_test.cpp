#include "gtest/gtest.h"
#include "utils.hpp"

using namespace std;
using namespace utils;

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
}
