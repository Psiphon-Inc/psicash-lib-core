#include "gtest/gtest.h"
#include "error.hpp"

using namespace std;
using namespace error;

TEST(TestError, Construction)
{
  Error e_default;
  ASSERT_FALSE(e_default);

  Error e_noncritical(false, "e_noncritical_message", "e_noncritical_filename", "e_noncritical_function", 123);
  ASSERT_TRUE(e_noncritical);
  ASSERT_NE(e_noncritical.ToString().find("e_noncritical_message"), string::npos);
  ASSERT_FALSE(e_noncritical.Critical());

  Error e_noncritical_wrapper(e_noncritical);
  ASSERT_TRUE(e_noncritical_wrapper);
  ASSERT_NE(e_noncritical_wrapper.ToString().find("e_noncritical_message"), string::npos);
  ASSERT_FALSE(e_noncritical.Critical());

  Error e_critical(true, "e_critical_message", "e_critical_filename", "e_critical_function", 123);
  ASSERT_TRUE(e_critical);
  ASSERT_NE(e_critical.ToString().find("e_critical_message"), string::npos);
  ASSERT_TRUE(e_critical.Critical());

  Error e_critical_wrapper(e_critical);
  ASSERT_TRUE(e_critical_wrapper);
  ASSERT_NE(e_critical_wrapper.ToString().find("e_critical_message"), string::npos);
  ASSERT_TRUE(e_critical_wrapper.Critical());

  auto eNullerr = nullerr;
  ASSERT_FALSE(eNullerr);
  ASSERT_EQ(eNullerr, nullerr);
}

TEST(TestError, Macros) {
  { // Noncritical
    const bool critical = false;
    auto e1 = MakeNoncriticalError("e1message");
    ASSERT_TRUE(e1);
    ASSERT_NE(e1.ToString().find("e1message"), string::npos);
    ASSERT_EQ(e1.Critical(), critical);

    auto e2 = WrapError(e1, "e2message");
    ASSERT_TRUE(e2);
    ASSERT_NE(e2.ToString().find("e2message"), string::npos);
    ASSERT_NE(e2.ToString().find("e1message"), string::npos);
    ASSERT_EQ(e2.Critical(), critical);

    auto e3 = PassError(e1);
    ASSERT_TRUE(e3);
    // Brittle. File info is in parens, so we'll check for two sets of them.
    auto first_open_paren = e3.ToString().find("(");
    auto second_open_paren = e3.ToString().rfind("(");
    ASSERT_NE(first_open_paren, second_open_paren);
    ASSERT_NE(first_open_paren, string::npos);
    ASSERT_NE(second_open_paren, string::npos);
    ASSERT_NE(e3.ToString().find("e1message"), string::npos);
    ASSERT_EQ(e3.Critical(), critical);

    auto e4 = WrapError(e3, "e3message");
    ASSERT_TRUE(e4);
    ASSERT_NE(e4.ToString().find("e3message"), string::npos);
    ASSERT_NE(e4.ToString().find("e1message"), string::npos);
    ASSERT_EQ(e4.Critical(), critical);
  }
  { // Critical
    const bool critical = true;
    auto e1 = MakeCriticalError("e1message");
    ASSERT_TRUE(e1);
    ASSERT_NE(e1.ToString().find("e1message"), string::npos);
    ASSERT_EQ(e1.Critical(), critical);

    auto e2 = WrapError(e1, "e2message");
    ASSERT_TRUE(e2);
    ASSERT_NE(e2.ToString().find("e2message"), string::npos);
    ASSERT_NE(e2.ToString().find("e1message"), string::npos);
    ASSERT_EQ(e2.Critical(), critical);

    auto e3 = PassError(e1);
    ASSERT_TRUE(e3);
    // Brittle. File info is in parens, so we'll check for two sets of them.
    auto first_open_paren = e3.ToString().find("(");
    auto second_open_paren = e3.ToString().rfind("(");
    ASSERT_NE(first_open_paren, second_open_paren);
    ASSERT_NE(first_open_paren, string::npos);
    ASSERT_NE(second_open_paren, string::npos);
    ASSERT_NE(e3.ToString().find("e1message"), string::npos);
    ASSERT_EQ(e3.Critical(), critical);

    auto e4 = WrapError(e3, "e3message");
    ASSERT_TRUE(e4);
    ASSERT_NE(e4.ToString().find("e3message"), string::npos);
    ASSERT_NE(e4.ToString().find("e1message"), string::npos);
    ASSERT_EQ(e4.Critical(), critical);
  }
}

TEST(TestError, BoolOperator)
{
  Error e1;
  ASSERT_TRUE(!e1);
  ASSERT_FALSE(e1);

  Error e2(false, "e2message", "e2filename", "e2function", 123);
  ASSERT_TRUE(e2);
  ASSERT_FALSE(!e2);
}

TEST(TestError, ToString) {
  Error e1;
  ASSERT_FALSE(e1);
  // This test is brittle. The non-error string is like "(nonerror)".
  ASSERT_EQ(e1.ToString()[0], '(');

  Error e_noncritical(false, "e_noncritical_message", "e_noncritical_filename", "e_noncritical_function", 123);
  ASSERT_TRUE(e_noncritical);
  ASSERT_NE(e_noncritical.ToString().find("e_noncritical_message"), string::npos);
  ASSERT_NE(e_noncritical.ToString().find("e_noncritical_filename"), string::npos);
  ASSERT_NE(e_noncritical.ToString().find("e_noncritical_function"), string::npos);
  ASSERT_NE(e_noncritical.ToString().find("123"), string::npos);
  ASSERT_EQ(e_noncritical.ToString().find("CRITICAL"), string::npos);

  Error e_critical(true, "e_critical_message", "e_critical_filename", "e_critical_function", 123);
  ASSERT_TRUE(e_critical);
  ASSERT_NE(e_critical.ToString().find("e_critical_message"), string::npos);
  ASSERT_NE(e_critical.ToString().find("e_critical_filename"), string::npos);
  ASSERT_NE(e_critical.ToString().find("e_critical_function"), string::npos);
  ASSERT_NE(e_critical.ToString().find("123"), string::npos);
  ASSERT_NE(e_critical.ToString().find("CRITICAL"), string::npos);
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

  Result<string> r3(MakeNoncriticalError("r3error"));
  ASSERT_FALSE(r3);
  ASSERT_NE(r3.error().ToString().find("r3error"), string::npos);

  Result<string> r4(MakeCriticalError("r4error"));
  ASSERT_FALSE(r4);
  ASSERT_NE(r4.error().ToString().find("r4error"), string::npos);
}
