#include "gtest/gtest.h"
#include "test_helpers.hpp"
#include "userdata.hpp"
#include "vendor/nlohmann/json.hpp"
using json = nlohmann::json;

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

TEST_F(TestUserData, Persistence)
{
    auto want_server_time_diff = datetime::Duration(54321);
    AuthTokens want_auth_tokens = {{"k1", "v1"}, {"k2", "v2"}};
    bool want_is_account = true;
    int64_t want_balance = 54321;
    PurchasePrices want_purchase_prices = {{"tc1", "d1", 123}, {"tc2", "d2", 321}};
    Purchases want_purchases = {
        {"id1", "tc1", "d1", nullopt, nullopt, "a1"},
        {"id2", "tc2", "d2", nullopt, nullopt, "a2"}};
    string req_metadata_key = "req_metadata_key";
    string want_req_metadata_value = "want_req_metadata_value";

    auto temp_dir = GetTempDir();

    {
        UserData ud;
        auto err = ud.Init(temp_dir.c_str());
        ASSERT_FALSE(err);

        auto shifted_now = datetime::DateTime::Now().Add(want_server_time_diff);
        err = ud.SetServerTimeDiff(shifted_now);
        ASSERT_FALSE(err);

        err = ud.SetAuthTokens(want_auth_tokens, want_is_account);
        ASSERT_FALSE(err);

        err = ud.SetBalance(want_balance);
        ASSERT_FALSE(err);

        err = ud.SetPurchasePrices(want_purchase_prices);
        ASSERT_FALSE(err);

        err = ud.SetPurchases(want_purchases);
        ASSERT_FALSE(err);

        err = ud.SetRequestMetadataItem(req_metadata_key, want_req_metadata_value);
        ASSERT_FALSE(err);
    }

    {
        UserData ud;
        auto err = ud.Init(temp_dir.c_str());
        ASSERT_FALSE(err);

        auto got_server_time_diff = ud.GetServerTimeDiff();
        ASSERT_NEAR(want_server_time_diff.count(), got_server_time_diff.count(), 10);

        auto got_auth_tokens = ud.GetAuthTokens();
        ASSERT_EQ(got_auth_tokens, want_auth_tokens);

        auto got_is_account = ud.GetIsAccount();
        ASSERT_EQ(got_is_account, want_is_account);

        auto got_balance = ud.GetBalance();
        ASSERT_EQ(got_balance, want_balance);

        auto got_purchase_prices = ud.GetPurchasePrices();
        ASSERT_EQ(got_purchase_prices, want_purchase_prices);

        auto got_purchases = ud.GetPurchases();
        ASSERT_EQ(got_purchases, want_purchases);

        auto got_request_metadata = ud.GetRequestMetadata();
        ASSERT_EQ(want_req_metadata_value, got_request_metadata[req_metadata_key]);
    }
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
    ASSERT_NEAR(want.count(), got.count(), 10);
}

TEST_F(TestUserData, UpdatePurchaseLocalTimeExpiry)
{
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str());
    ASSERT_FALSE(err);

    Purchase purchase_noexpiry{
        .id = "id",
        .transaction_class = "tc",
        .distinguisher = "d"
    };
    datetime::DateTime server_expiry;
    ASSERT_TRUE(server_expiry.FromISO8601("2031-02-03T04:05:06.789Z"));
    Purchase purchase_expiry{
        .id = "id",
        .transaction_class = "tc",
        .distinguisher = "d",
        .server_time_expiry = server_expiry
    };

    // Zero server time diff
    ASSERT_EQ(ud.GetServerTimeDiff().count(), 0);
    ud.UpdatePurchaseLocalTimeExpiry(purchase_noexpiry);
    ASSERT_FALSE(purchase_noexpiry.local_time_expiry);
    ud.UpdatePurchaseLocalTimeExpiry(purchase_expiry);
    ASSERT_TRUE(purchase_expiry.local_time_expiry);
    ASSERT_EQ(*purchase_expiry.local_time_expiry, server_expiry);

    // Nonzero server time diff
    auto server_time_diff = datetime::Duration(54321);
    ASSERT_FALSE(ud.SetServerTimeDiff(datetime::DateTime::Now().Add(server_time_diff)));
    ud.UpdatePurchaseLocalTimeExpiry(purchase_noexpiry);
    ASSERT_FALSE(purchase_noexpiry.local_time_expiry);
    ud.UpdatePurchaseLocalTimeExpiry(purchase_expiry);
    ASSERT_TRUE(purchase_expiry.local_time_expiry);
    ASSERT_EQ(*purchase_expiry.local_time_expiry, server_expiry.Sub(server_time_diff));
}

TEST_F(TestUserData, AuthTokens)
{
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str());
    ASSERT_FALSE(err);

    // Check default value
    auto tokens = ud.GetAuthTokens();
    ASSERT_EQ(tokens.size(), 0);

    auto is_account = ud.GetIsAccount();
    ASSERT_EQ(is_account, false);

    // Set then get
    AuthTokens want = {{"k1", "v1"}, {"k2", "v2"}};
    err = ud.SetAuthTokens(want, false);
    ASSERT_FALSE(err);
    auto got_tokens = ud.GetAuthTokens();
    ASSERT_EQ(want, got_tokens);
    is_account = ud.GetIsAccount();
    ASSERT_EQ(is_account, false);

    err = ud.SetAuthTokens(want, true);
    ASSERT_FALSE(err);
    got_tokens = ud.GetAuthTokens();
    ASSERT_EQ(want, got_tokens);
    is_account = ud.GetIsAccount();
    ASSERT_EQ(is_account, true);

    // CullAuthTokens
    err = ud.SetAuthTokens({{"k1","v1"},{"k2","v2"},{"k3","v3"},{"k4","v4"},}, false);
    ASSERT_FALSE(err);
    std::map<std::string, bool> valid_tokens = {{"v1",true},{"v2",false},{"v3",true}};
    want = {{"k1","v1"},{"k3","v3"}};
    err = ud.CullAuthTokens(valid_tokens);
    ASSERT_FALSE(err);
    got_tokens = ud.GetAuthTokens();
    ASSERT_EQ(want, got_tokens);
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

    err = ud.SetPurchases(want);
    ASSERT_FALSE(err);
    auto got = ud.GetPurchases();
    ASSERT_EQ(got, want);

    // Test populating the local_time_expiry.
    auto serverTimeDiff = datetime::Duration(54321);
    auto local_now = datetime::DateTime::Now();
    auto server_now = local_now.Add(serverTimeDiff);
    err = ud.SetServerTimeDiff(server_now);
    ASSERT_FALSE(err);
    // Supply server time but not local time
    want.push_back({"id3", "tc3", "d3", server_now, nullopt, "a3"});
    err = ud.SetPurchases(want);
    got = ud.GetPurchases();
    ASSERT_EQ(got.size(), 3);
    ASSERT_TRUE(got[2].local_time_expiry);
    ASSERT_NEAR(got[2].local_time_expiry->MillisSinceEpoch(), local_now.MillisSinceEpoch(), 5);
}

TEST_F(TestUserData, AddPurchase)
{
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str());
    ASSERT_FALSE(err);

    // Check default value
    auto v = ud.GetPurchases();
    ASSERT_EQ(v.size(), 0);

    // Set then get
    Purchases want = {
        {"id1", "tc1", "d1", nullopt, nullopt, "a1"},
        {"id2", "tc2", "d2", nullopt, nullopt, "a2"}};

    err = ud.SetPurchases(want);
    ASSERT_FALSE(err);
    auto got = ud.GetPurchases();
    ASSERT_EQ(got, want);

    Purchase add = {"id3", "tc3", "d3", nullopt, nullopt, nullopt};
    err = ud.AddPurchase(add);
    ASSERT_FALSE(err);
    got = ud.GetPurchases();
    want.push_back(add);
    ASSERT_EQ(got, want);
    ASSERT_EQ(ud.GetLastTransactionID(), "id3");

    // Try to add the same purchase again
    err = ud.AddPurchase(add);
    ASSERT_FALSE(err);
    got = ud.GetPurchases();
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

TEST_F(TestUserData, Metadata)
{
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str());
    ASSERT_FALSE(err);

    auto v = ud.GetRequestMetadata();
    ASSERT_EQ(v, json({}));

    err = ud.SetRequestMetadataItem("k", "v");
    ASSERT_FALSE(err);
    v = ud.GetRequestMetadata();
    ASSERT_EQ(v.dump(), json({{"k", "v"}}).dump());

    err = ud.SetRequestMetadataItem("kk", 123);
    ASSERT_FALSE(err);
    v = ud.GetRequestMetadata();
    ASSERT_EQ(v.dump(), json({{"k", "v"}, {"kk", 123}}).dump());

    err = ud.SetRequestMetadataItem("k", "v2");
    ASSERT_FALSE(err);
    v = ud.GetRequestMetadata();
    ASSERT_EQ(v.dump(), json({{"k", "v2"}, {"kk", 123}}).dump());

    // Make sure modifying the result doesn't modify the internal structure
    v["temp"] = "temp";
    v = ud.GetRequestMetadata();
    ASSERT_EQ(v.dump(), json({{"k", "v2"}, {"kk", 123}}).dump());

    // Empty key is an error
    err = ud.SetRequestMetadataItem("", "v");
    ASSERT_TRUE(err);

    err = ud.SetRequestMetadataItem("k", nullptr);
    ASSERT_FALSE(err);
    v = ud.GetRequestMetadata();
    ASSERT_TRUE(v["k"].is_null());
}
