#include "gtest/gtest.h"
#include "test_helpers.h"
#include "psicash.h"
#include "userdata.h"

using namespace std;
using namespace psicash;


class TestPsiCash : public ::testing::Test, public TempDir
{
public:
  TestPsiCash() {}

  static string HTTPReq_Stub(const string& params) {
    return "ok";
  }
};

// Subclass psicash::PsiCash to get access to private members for testing.
// This would probably be done more cleanly with dependency injection, but that adds a
// bunch of overhead for little gain.
class PsiCashTester : public psicash::PsiCash {
public:
  UserData& user_data() { return *user_data_; }
};


TEST_F(TestPsiCash, InitSimple) {
  {
    PsiCash pc;
    auto err = pc.Init(GetTempDir().c_str(), HTTPReq_Stub);
    ASSERT_FALSE(err);
  }

  {
    PsiCash pc;
    auto err = pc.Init(GetTempDir().c_str(), nullptr);
    ASSERT_FALSE(err);
  }
}

TEST_F(TestPsiCash, InitFail)
{
  auto bad_dir = GetTempDir() + "/a/b/c/d/f/g";
  PsiCash pc;
  auto err = pc.Init(bad_dir.c_str(), nullptr);
  ASSERT_TRUE(err);
}

TEST_F(TestPsiCash, SetHTTPRequestFn)
{
  {
    PsiCash pc;
    auto err = pc.Init(GetTempDir().c_str(), HTTPReq_Stub);
    ASSERT_FALSE(err);
    pc.SetHTTPRequestFn(HTTPReq_Stub);
  }

  {
    PsiCash pc;
    auto err = pc.Init(GetTempDir().c_str(), nullptr);
    ASSERT_FALSE(err);
    pc.SetHTTPRequestFn(HTTPReq_Stub);
  }
}

TEST_F(TestPsiCash, IsAccount) {
  PsiCashTester pc;
  auto err = pc.Init(GetTempDir().c_str(), nullptr);
  ASSERT_FALSE(err);

  // Check the default
  auto v = pc.IsAccount();
  ASSERT_FALSE(v);

  err = pc.user_data().SetIsAccount(true);
  ASSERT_FALSE(err);

  v = pc.IsAccount();
  ASSERT_TRUE(v);

  err = pc.user_data().SetIsAccount(false);
  ASSERT_FALSE(err);

  v = pc.IsAccount();
  ASSERT_FALSE(v);
}

TEST_F(TestPsiCash, ValidTokenTypes) {
  PsiCashTester pc;
  auto err = pc.Init(GetTempDir().c_str(), nullptr);
  ASSERT_FALSE(err);

  auto vtt = pc.ValidTokenTypes();
  ASSERT_EQ(vtt.size(), 0);

  AuthTokens at = {{"a", "a"}, {"b", "b"}, {"c", "c"}};
  err = pc.user_data().SetAuthTokens(at);
  vtt = pc.ValidTokenTypes();
  ASSERT_EQ(vtt.size(), 3);
  for (const auto& k : vtt) {
    ASSERT_EQ(at.count(k), 1);
    at.erase(k);
  }
  ASSERT_EQ(at.size(), 0); // we should have erase all items

  AuthTokens empty;
  err = pc.user_data().SetAuthTokens(empty);
  vtt = pc.ValidTokenTypes();
  ASSERT_EQ(vtt.size(), 0);
}
