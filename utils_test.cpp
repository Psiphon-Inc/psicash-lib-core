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
