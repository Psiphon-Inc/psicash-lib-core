#include "gtest/gtest.h"
#include "test_helpers.h"
#include "userdata.h"

using namespace std;
using namespace nonstd;
using namespace psicash;

class TestUserData : public ::testing::Test, public TempDir
{
public:
  TestUserData() {}
};

TEST_F(TestUserData, InitSimple)
{
  UserData ud;
  auto err = ud.Init(GetTempDir().c_str());
  ASSERT_FALSE(err);
}

TEST_F(TestUserData, InitFail)
{
  auto bad_dir = GetTempDir() + "/a/b/c/d/f/g";
  UserData ud;
  auto err = ud.Init(bad_dir.c_str());
  ASSERT_TRUE(err);
}

TEST_F(TestUserData, ServerTimeDiff)
{
  UserData ud;
  auto err = ud.Init(GetTempDir().c_str());
  ASSERT_FALSE(err);

  // Check default value
  auto v = ud.GetServerTimeDiff();
  ASSERT_EQ(v.count(), 0);

  // Set then get

  auto want = datetime::Duration(54321);
  auto shifted_now = datetime::DateTime::Now().Add(want);
  err = ud.SetServerTimeDiff(shifted_now);
  ASSERT_FALSE(err);
  auto got = ud.GetServerTimeDiff();
  ASSERT_EQ(want, got);
}

TEST_F(TestUserData, AuthTokens)
{
  UserData ud;
  auto err = ud.Init(GetTempDir().c_str());
  ASSERT_FALSE(err);

  // Check default value
  auto v = ud.GetAuthTokens();
  ASSERT_EQ(v.size(), 0);

  // Set then get
  AuthTokens want = {{"k1", "v1"}, {"k2", "v2"}};
  err = ud.SetAuthTokens(want);
  ASSERT_FALSE(err);
  auto got = ud.GetAuthTokens();
  ASSERT_EQ(want, got);
}

TEST_F(TestUserData, IsAccount)
{
  UserData ud;
  auto err = ud.Init(GetTempDir().c_str());
  ASSERT_FALSE(err);

  // Check default value
  auto v = ud.GetIsAccount();
  ASSERT_EQ(v, false);

  // Set then get
  bool want = true;
  err = ud.SetIsAccount(want);
  ASSERT_FALSE(err);
  auto got = ud.GetIsAccount();
  ASSERT_EQ(got, want);
}

TEST_F(TestUserData, Balance)
{
  UserData ud;
  auto err = ud.Init(GetTempDir().c_str());
  ASSERT_FALSE(err);

  // Check default value
  auto v = ud.GetBalance();
  ASSERT_EQ(v, 0);

  // Set then get
  int64_t want = 54321;
  err = ud.SetBalance(want);
  ASSERT_FALSE(err);
  auto got = ud.GetBalance();
  ASSERT_EQ(got, want);
}


TEST_F(TestUserData, PurchasePrices)
{
  UserData ud;
  auto err = ud.Init(GetTempDir().c_str());
  ASSERT_FALSE(err);

  // Check default value
  auto v = ud.GetPurchasePrices();
  ASSERT_EQ(v.size(), 0);

  // Set then get
  PurchasePrices want = {{"tc1", "d1", 123}, {"tc2", "d2", 321}};
  err = ud.SetPurchasePrices(want);
  ASSERT_FALSE(err);
  auto got = ud.GetPurchasePrices();
  ASSERT_EQ(got, want);
}

TEST_F(TestUserData, Purchases)
{
  UserData ud;
  auto err = ud.Init(GetTempDir().c_str());
  ASSERT_FALSE(err);

  // Check default value
  auto v = ud.GetPurchases();
  ASSERT_EQ(v.size(), 0);

  // Set then get
  auto dt1 = datetime::DateTime::Now().Add(datetime::Duration(1));
  auto dt2 = datetime::DateTime::Now().Add(datetime::Duration(2));
  Purchases want = {
    {"id1", "tc1", "d1", dt1, dt2, "a1"},
    {"id2", "tc2", "d2", nullopt, nullopt, "a2"}};

  ASSERT_EQ(want, want);

  err = ud.SetPurchases(want);
  ASSERT_FALSE(err);
  auto got = ud.GetPurchases();
  ASSERT_EQ(got, want);
}

TEST_F(TestUserData, LastTransactionID)
{
  UserData ud;
  auto err = ud.Init(GetTempDir().c_str());
  ASSERT_FALSE(err);

  // Check default value
  auto v = ud.GetLastTransactionID();
  ASSERT_EQ(v, kTransactionIDZero);

  // Set then get
  TransactionID want = "LastTransactionID";
  err = ud.SetLastTransactionID(want);
  ASSERT_FALSE(err);
  auto got = ud.GetLastTransactionID();
  ASSERT_EQ(got, want);
}
