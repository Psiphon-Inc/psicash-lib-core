#include "gtest/gtest.h"
#include "error.hpp"

using namespace std;
using namespace error;

TEST(TestError, Construction)
{
  Error e1;
  ASSERT_FALSE(e1);

  Error e2("e2message", "e2filename", "e2function", 123);
  ASSERT_TRUE(e2);
  ASSERT_NE(e2.ToString().find("e2message"), string::npos);

  Error e3(e2);
  ASSERT_TRUE(e3);
  ASSERT_NE(e3.ToString().find("e2message"), string::npos);

  auto e4 = nullerr;
  ASSERT_FALSE(e1);
  ASSERT_EQ(e4, nullerr);
}

TEST(TestError, Macros) {
  auto e1 = MakeError("e1message");
  ASSERT_TRUE(e1);
  ASSERT_NE(e1.ToString().find("e1message"), string::npos);

  auto e2 = WrapError(e1, "e2message");
  ASSERT_TRUE(e2);
  ASSERT_NE(e2.ToString().find("e2message"), string::npos);
  ASSERT_NE(e2.ToString().find("e1message"), string::npos);

  auto e3 = PassError(e1);
  ASSERT_TRUE(e3);
  // Brittle. File info is in parens, so we'll check for two sets of them.
  auto first_open_paren = e3.ToString().find("(");
  auto second_open_paren = e3.ToString().rfind("(");
  ASSERT_NE(first_open_paren, second_open_paren);
  ASSERT_NE(first_open_paren, string::npos);
  ASSERT_NE(second_open_paren, string::npos);
  ASSERT_NE(e3.ToString().find("e1message"), string::npos);

  auto e4 = WrapError(e3, "e3message");
  ASSERT_TRUE(e4);
  ASSERT_NE(e4.ToString().find("e3message"), string::npos);
  ASSERT_NE(e4.ToString().find("e1message"), string::npos);
}

TEST(TestError, BoolOperator)
{
  Error e1;
  ASSERT_TRUE(!e1);
  ASSERT_FALSE(e1);

  Error e2("e2message", "e2filename", "e2function", 123);
  ASSERT_TRUE(e2);
  ASSERT_FALSE(!e2);
}

TEST(TestError, ToString) {
  Error e1;
  ASSERT_FALSE(e1);
  // This test is brittle. The non-error string is like "(nonerror)".
  ASSERT_EQ(e1.ToString()[0], '(');

  Error e2("e2message", "e2filename", "e2function", 123);
  ASSERT_TRUE(e2);
  ASSERT_NE(e2.ToString().find("e2message"), string::npos);
  ASSERT_NE(e2.ToString().find("e2filename"), string::npos);
  ASSERT_NE(e2.ToString().find("e2function"), string::npos);
  ASSERT_NE(e2.ToString().find("123"), string::npos);
}

TEST(TestResult, Construction) {
  // There's no default constructor, so this is a compile error:
  // Result<string> r;

  Result<string> r1("r1val");
  ASSERT_TRUE(r1);
  ASSERT_EQ(*r1, "r1val");

  Result<int> r2(321);
  ASSERT_TRUE(r2);
  ASSERT_EQ(*r2, 321);

  Result<string> r3(MakeError("r3error"));
  ASSERT_FALSE(r3);
  ASSERT_NE(r3.error().ToString().find("r3error"), string::npos);
}
