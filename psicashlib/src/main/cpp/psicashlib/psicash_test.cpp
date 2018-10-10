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
  {
    auto bad_dir = GetTempDir() + "/a/b/c/d/f/g";
    PsiCash pc;
    auto err = pc.Init(bad_dir.c_str(), nullptr);
    ASSERT_TRUE(err);
  }
  {
    PsiCash pc;
    auto err = pc.Init(nullptr, nullptr);
    ASSERT_TRUE(err);
  }
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
  ASSERT_EQ(v, false);

  err = pc.user_data().SetIsAccount(true);
  ASSERT_FALSE(err);

  v = pc.IsAccount();
  ASSERT_EQ(v, true);

  err = pc.user_data().SetIsAccount(false);
  ASSERT_FALSE(err);

  v = pc.IsAccount();
  ASSERT_EQ(v, false);
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

TEST_F(TestPsiCash, Balance) {
  PsiCashTester pc;
  auto err = pc.Init(GetTempDir().c_str(), nullptr);
  ASSERT_FALSE(err);

  // Check the default
  auto v = pc.Balance();
  ASSERT_EQ(v, 0);

  err = pc.user_data().SetBalance(123);
  ASSERT_FALSE(err);

  v = pc.Balance();
  ASSERT_EQ(v, 123);

  err = pc.user_data().SetBalance(0);
  ASSERT_FALSE(err);

  v = pc.Balance();
  ASSERT_EQ(v, 0);
}

TEST_F(TestPsiCash, GetPurchasePrices) {
  PsiCashTester pc;
  auto err = pc.Init(GetTempDir().c_str(), nullptr);
  ASSERT_FALSE(err);

  auto v = pc.GetPurchasePrices();
  ASSERT_EQ(v.size(), 0);

  PurchasePrices pps = {{"tc1", "d1", 123}, {"tc2", "d2", 321}};
  err = pc.user_data().SetPurchasePrices(pps);
  ASSERT_FALSE(err);

  v = pc.GetPurchasePrices();
  ASSERT_EQ(v.size(), 2);
  ASSERT_EQ(v, pps);

  err = pc.user_data().SetPurchasePrices({});
  ASSERT_FALSE(err);

  v = pc.GetPurchasePrices();
  ASSERT_EQ(v.size(), 0);
}

TEST_F(TestPsiCash, GetPurchases) {
  PsiCashTester pc;
  auto err = pc.Init(GetTempDir().c_str(), nullptr);
  ASSERT_FALSE(err);

  auto v = pc.GetPurchases();
  ASSERT_EQ(v.size(), 0);

  Purchases ps = {
    {"id1", "tc1", "d1", datetime::DateTime::Now(), datetime::DateTime::Now(), "a1"},
    {"id2", "tc2", "d2", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt}};

  err = pc.user_data().SetPurchases(ps);
  ASSERT_FALSE(err);

  v = pc.GetPurchases();
  ASSERT_EQ(v.size(), 2);
  ASSERT_EQ(v, ps);

  err = pc.user_data().SetPurchases({});
  ASSERT_FALSE(err);

  v = pc.GetPurchases();
  ASSERT_EQ(v.size(), 0);
}

TEST_F(TestPsiCash, ValidPurchases) {
  PsiCashTester pc;
  auto err = pc.Init(GetTempDir().c_str(), nullptr);
  ASSERT_FALSE(err);

  auto v = pc.GetPurchases();
  ASSERT_EQ(v.size(), 0);

  v = pc.ValidPurchases();
  ASSERT_EQ(v.size(), 0);

  auto before_now = datetime::DateTime::Now().Sub(datetime::Duration(54321));
  auto after_now = datetime::DateTime::Now().Add(datetime::Duration(54321));

  Purchases ps = {
    {"id1", "tc1", "d1", before_now, nonstd::nullopt, "a1"},
    {"id2", "tc2", "d2", after_now, nonstd::nullopt, "a2"},
    {"id3", "tc3", "d3", before_now, nonstd::nullopt, "a3"},
    {"id4", "tc4", "d4", after_now, nonstd::nullopt, "a4"},
    {"id5", "tc5", "d5", nonstd::nullopt, nonstd::nullopt, "a5"}};

  err = pc.user_data().SetPurchases(ps);
  ASSERT_FALSE(err);

  v = pc.GetPurchases();
  ASSERT_EQ(v.size(), ps.size());
  ASSERT_EQ(v, ps);

  v = pc.ValidPurchases();
  ASSERT_EQ(v.size(), 3);
  // There's no guarantee that the order of purchases won't change, but we know they won't
  ASSERT_EQ(v[0].id, "id2");
  ASSERT_EQ(v[1].id, "id4");
  ASSERT_EQ(v[2].id, "id5");

  v = pc.GetPurchases();
  ASSERT_EQ(v.size(), ps.size());
  ASSERT_EQ(v, ps);
}

TEST_F(TestPsiCash, NextExpiringPurchase) {
  PsiCashTester pc;
  auto err = pc.Init(GetTempDir().c_str(), nullptr);
  ASSERT_FALSE(err);

  auto v = pc.GetPurchases();
  ASSERT_EQ(v.size(), 0);

  auto p = pc.NextExpiringPurchase();
  ASSERT_FALSE(p);

  auto first = datetime::DateTime::Now().Sub(datetime::Duration(333));
  auto second = datetime::DateTime::Now().Sub(datetime::Duration(222));
  auto third = datetime::DateTime::Now().Sub(datetime::Duration(111));

  Purchases ps = {
    {"id1", "tc1", "d1", second, nonstd::nullopt, "a1"},
    {"id2", "tc2", "d2", first, nonstd::nullopt, "a2"}, // first to expire
    {"id3", "tc3", "d3", nonstd::nullopt, nonstd::nullopt, "a3"},
    {"id4", "tc4", "d4", third, nonstd::nullopt, "a4"}};

  err = pc.user_data().SetPurchases(ps);
  ASSERT_FALSE(err);

  v = pc.GetPurchases();
  ASSERT_EQ(v.size(), ps.size());
  ASSERT_EQ(v, ps);

  p = pc.NextExpiringPurchase();
  ASSERT_TRUE(p);
  ASSERT_EQ(p->id, ps[1].id);

  auto later_than_now = datetime::DateTime::Now().Add(datetime::Duration(54321));
  ps = {
    {"id1", "tc1", "d1", nonstd::nullopt, nonstd::nullopt, "a1"},
    {"id2", "tc2", "d2", later_than_now, nonstd::nullopt, "a2"}, // only expiring
    {"id3", "tc3", "d3", nonstd::nullopt, nonstd::nullopt, "a3"}};

  err = pc.user_data().SetPurchases(ps);
  ASSERT_FALSE(err);

  v = pc.GetPurchases();
  ASSERT_EQ(v.size(), ps.size());
  ASSERT_EQ(v, ps);

  p = pc.NextExpiringPurchase();
  ASSERT_TRUE(p);
  ASSERT_EQ(p->id, ps[1].id);

  // None expiring
  ps = {
    {"id1", "tc1", "d1", nonstd::nullopt, nonstd::nullopt, "a1"},
    {"id2", "tc2", "d2", nonstd::nullopt, nonstd::nullopt, "a3"}};

  err = pc.user_data().SetPurchases(ps);
  ASSERT_FALSE(err);

  v = pc.GetPurchases();
  ASSERT_EQ(v.size(), ps.size());
  ASSERT_EQ(v, ps);

  p = pc.NextExpiringPurchase();
  ASSERT_FALSE(p);
}

TEST_F(TestPsiCash, ExpirePurchases) {
  PsiCashTester pc;
  auto err = pc.Init(GetTempDir().c_str(), nullptr);
  ASSERT_FALSE(err);

  auto v = pc.GetPurchases();
  ASSERT_EQ(v.size(), 0);

  v = pc.ExpirePurchases();
  ASSERT_EQ(v.size(), 0);

  auto before_now = datetime::DateTime::Now().Sub(datetime::Duration(54321));
  auto after_now = datetime::DateTime::Now().Add(datetime::Duration(54321));

  Purchases ps = {
    {"id1", "tc1", "d1", after_now, nonstd::nullopt, "a1"},
    {"id2", "tc2", "d2", before_now, nonstd::nullopt, "a2"},
    {"id3", "tc3", "d3", nonstd::nullopt, nonstd::nullopt, "a3"},
    {"id4", "tc4", "d4", before_now, nonstd::nullopt, "a4"}};
  Purchases expired = {ps[1], ps[3]};
  Purchases nonexpired = {ps[0], ps[2]};

  err = pc.user_data().SetPurchases(ps);
  ASSERT_FALSE(err);

  v = pc.GetPurchases();
  ASSERT_EQ(v.size(), ps.size());
  ASSERT_EQ(v, ps);

  v = pc.ExpirePurchases();
  ASSERT_EQ(v.size(), expired.size());
  ASSERT_EQ(v, expired);

  v = pc.GetPurchases();
  ASSERT_EQ(v.size(), nonexpired.size());
  ASSERT_EQ(v, nonexpired);

  // No expired purchases left
  v = pc.ExpirePurchases();
  ASSERT_EQ(v.size(), 0);

  v = pc.GetPurchases();
  ASSERT_EQ(v.size(), nonexpired.size());
  ASSERT_EQ(v, nonexpired);
}

TEST_F(TestPsiCash, RemovePurchases) {
  PsiCashTester pc;
  auto err = pc.Init(GetTempDir().c_str(), nullptr);
  ASSERT_FALSE(err);

  auto v = pc.GetPurchases();
  ASSERT_EQ(v.size(), 0);

  Purchases ps = {
    {"id1", "tc1", "d1", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
    {"id2", "tc2", "d2", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
    {"id3", "tc3", "d3", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
    {"id4", "tc4", "d4", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt}};
  vector<TransactionID> removeIDs = {ps[1].id, ps[3].id};
  Purchases remaining = {ps[0], ps[2]};

  err = pc.user_data().SetPurchases(ps);
  ASSERT_FALSE(err);

  v = pc.GetPurchases();
  ASSERT_EQ(v.size(), ps.size());
  ASSERT_EQ(v, ps);

  pc.RemovePurchases(removeIDs);

  v = pc.GetPurchases();
  ASSERT_EQ(v.size(), remaining.size());
  ASSERT_EQ(v, remaining);

  // removeIDs are not present now
  pc.RemovePurchases(removeIDs);

  v = pc.GetPurchases();
  ASSERT_EQ(v.size(), remaining.size());
  ASSERT_EQ(v, remaining);

  // empty array
  pc.RemovePurchases({});

  v = pc.GetPurchases();
  ASSERT_EQ(v.size(), remaining.size());
  ASSERT_EQ(v, remaining);
}
