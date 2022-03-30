#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "test_helpers.hpp"
#include "userdata.hpp"
#include "vendor/nlohmann/json.hpp"
using json = nlohmann::json;

using namespace std;
using namespace nonstd;
using namespace psicash;
using namespace testing;

constexpr auto dev = true;

class TestUserData : public ::testing::Test, public TempDir
{
  public:
    TestUserData() {}
};

TEST_F(TestUserData, InitSimple)
{
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str(), dev);
    ASSERT_FALSE(err);
}

TEST_F(TestUserData, InitFail)
{
    auto bad_dir = GetTempDir() + "/a/b/c/d/f/g";
    UserData ud;
    auto err = ud.Init(bad_dir.c_str(), dev);
    ASSERT_TRUE(err);
}

TEST_F(TestUserData, InitUpgrade)
{
    auto dsDir = GetTempDir();

    // Write a v1 file.
    auto ok = TempDir::Write(dsDir, dev, R"({"IsAccount":false,"authTokens":{"earner":"earnertoken","indicator":"indicatortoken","spender":"spendertoken"},"balance":125000000000,"lastTransactionID":"boosttransid","purchasePrices":[{"class":"speed-boost","distinguisher":"1hr","price":100000000000},{"class":"speed-boost","distinguisher":"2hr","price":200000000000},{"class":"speed-boost","distinguisher":"3hr","price":300000000000},{"class":"speed-boost","distinguisher":"4hr","price":400000000000},{"class":"speed-boost","distinguisher":"5hr","price":500000000000},{"class":"speed-boost","distinguisher":"6hr","price":600000000000},{"class":"speed-boost","distinguisher":"7hr","price":700000000000},{"class":"speed-boost","distinguisher":"8hr","price":800000000000},{"class":"speed-boost","distinguisher":"9hr","price":900000000000},{"class":"speed-boost","distinguisher":"24hr","price":800000000000}],"purchases":[{"authorization":{"AccessType":"speed-boost-test","Encoded":"boostauth","Expires":"2020-07-27T15:14:30.986Z","ID":"boostauthid"},"class":"speed-boost","distinguisher":"1hr","id":"boosttransid","localTimeExpiry":"2020-07-27T15:14:32.878Z","serverTimeExpiry":"2020-07-27T15:14:30.986Z"}],"requestMetadata":{"client_region":"CA","client_version":"999","propagation_channel_id":"ABCD1234","sponsor_id":"ABCD1234"},"serverTimeDiff":-2584,"v":1})");
    ASSERT_TRUE(ok) << errno;

    // Now load the file and upgrade
    UserData ud;
    auto err = ud.Init(dsDir.c_str(), dev);
    ASSERT_FALSE(err);

    ASSERT_GT(ud.GetInstanceID().length(), 0);
    ASSERT_FALSE(ud.GetIsLoggedOutAccount());
    ASSERT_EQ(ud.GetServerTimeDiff().count(), -2584);
    AuthTokens want_tokens = {{"earner",{"earnertoken",nullopt}},{"indicator",{"indicatortoken",nullopt}},{"spender",{"spendertoken",nullopt}}};
    ASSERT_TRUE(AuthTokenSetsEqual(ud.GetAuthTokens(), want_tokens));
    ASSERT_FALSE(ud.GetIsAccount());
    ASSERT_EQ(ud.GetAccountUsername(), "");
    ASSERT_EQ(ud.GetBalance(), 125000000000L);
    ASSERT_EQ(ud.GetPurchasePrices().size(), 10);
    ASSERT_EQ(ud.GetPurchases().size(), 1);
    ASSERT_EQ(ud.GetLastTransactionID(), "boosttransid");
    ASSERT_EQ(ud.GetRequestMetadata().size(), 4);
}

TEST_F(TestUserData, InitBadVersion)
{
    auto dsDir = GetTempDir();

    // Write a datastore file with a bad (too future) version: 999999.
    auto ok = TempDir::Write(dsDir, dev, R"({"IsAccount":false,"authTokens":{"earner":"earnertoken","indicator":"indicatortoken","spender":"spendertoken"},"balance":125000000000,"lastTransactionID":"boosttransid","purchasePrices":[{"class":"speed-boost","distinguisher":"1hr","price":100000000000},{"class":"speed-boost","distinguisher":"2hr","price":200000000000},{"class":"speed-boost","distinguisher":"3hr","price":300000000000},{"class":"speed-boost","distinguisher":"4hr","price":400000000000},{"class":"speed-boost","distinguisher":"5hr","price":500000000000},{"class":"speed-boost","distinguisher":"6hr","price":600000000000},{"class":"speed-boost","distinguisher":"7hr","price":700000000000},{"class":"speed-boost","distinguisher":"8hr","price":800000000000},{"class":"speed-boost","distinguisher":"9hr","price":900000000000},{"class":"speed-boost","distinguisher":"24hr","price":800000000000}],"purchases":[{"authorization":{"AccessType":"speed-boost-test","Encoded":"boostauth","Expires":"2020-07-27T15:14:30.986Z","ID":"boostauthid"},"class":"speed-boost","distinguisher":"1hr","id":"boosttransid","localTimeExpiry":"2020-07-27T15:14:32.878Z","serverTimeExpiry":"2020-07-27T15:14:30.986Z","serverTimeCreated":"2020-07-27T15:14:30.986Z"}],"requestMetadata":{"client_region":"CA","client_version":"999","propagation_channel_id":"ABCD1234","sponsor_id":"ABCD1234"},"serverTimeDiff":-2584,"v":999999})");
    ASSERT_TRUE(ok) << errno;

    // Now load the file and upgrade
    UserData ud;
    auto err = ud.Init(dsDir.c_str(), dev);
    ASSERT_TRUE(err);
}

TEST_F(TestUserData, Persistence)
{
    auto want_server_time_diff_ms = 54321;
    auto want_server_time_diff = datetime::Duration(want_server_time_diff_ms);
    auto future = datetime::DateTime::Now().Add(datetime::Duration(want_server_time_diff_ms*2)); // if this is less than want_server_time_diff_ms, the tokens will be expired already
    auto past = datetime::DateTime::Now().Sub(datetime::Duration(want_server_time_diff_ms*2));
    AuthTokens want_auth_tokens = {{"k1", {"v1", nullopt}}, {"k2", {"v2", future}}, {"k3", {"v3", past}}};
    bool want_is_account = true;
    string want_account_username = "account-username"s;
    int64_t want_balance = 12345;
    PurchasePrices want_purchase_prices = {{"tc1", "d1", 123}, {"tc2", "d2", 321}};
    Purchases want_purchases = {
        {"id1", datetime::DateTime(), "tc1", "d1", nullopt, nullopt, nullopt},
        {"id2", datetime::DateTime(), "tc2", "d2", nullopt, nullopt, nullopt}};
    string req_metadata_key = "req_metadata_key";
    string want_req_metadata_value = "want_req_metadata_value";

    auto temp_dir = GetTempDir();

    {
        // Set a bunch of values to persist
        UserData ud;
        auto err = ud.Init(temp_dir.c_str(), dev);
        ASSERT_FALSE(err);

        auto shifted_now = datetime::DateTime::Now().Add(want_server_time_diff);
        err = ud.SetServerTimeDiff(shifted_now);
        ASSERT_FALSE(err);

        err = ud.SetAuthTokens(want_auth_tokens, want_is_account, want_account_username);
        ASSERT_FALSE(err);

        err = ud.SetBalance(want_balance);
        ASSERT_FALSE(err);

        err = ud.SetPurchasePrices(want_purchase_prices);
        ASSERT_FALSE(err);

        err = ud.SetPurchases(want_purchases);
        ASSERT_FALSE(err);

        err = ud.SetRequestMetadataItem(req_metadata_key, want_req_metadata_value);
        ASSERT_FALSE(err);
        // close the datastore
    }
    {
        // Re-open the datastore and check values
        UserData ud;
        auto err = ud.Init(temp_dir.c_str(), dev);
        ASSERT_FALSE(err);

        auto got_server_time_diff = ud.GetServerTimeDiff();
        ASSERT_NEAR(want_server_time_diff.count(), got_server_time_diff.count(), 10);

        auto got_auth_tokens = ud.GetAuthTokens();
        ASSERT_TRUE(AuthTokenSetsEqual(got_auth_tokens, want_auth_tokens));

        auto got_is_account = ud.GetIsAccount();
        ASSERT_EQ(got_is_account, want_is_account);

        auto got_account_username = ud.GetAccountUsername();
        ASSERT_EQ(got_account_username, want_account_username);

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

TEST_F(TestUserData, DeleteUserData)
{
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str(), dev);
    ASSERT_FALSE(err);

    ASSERT_EQ(ud.GetBalance(), 0);
    ASSERT_FALSE(ud.GetIsLoggedOutAccount());

    ASSERT_FALSE(ud.SetBalance(1234L));
    ASSERT_EQ(ud.GetBalance(), 1234L);

    ASSERT_FALSE(ud.DeleteUserData(/*is_logged_out_account=*/true));
    ASSERT_EQ(ud.GetBalance(), 0);
    ASSERT_TRUE(ud.GetIsLoggedOutAccount());
}

TEST_F(TestUserData, GetInstanceID)
{
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str(), dev);
    ASSERT_FALSE(err);

    string prefix = "instanceid_";

    auto v1 = ud.GetInstanceID();
    ASSERT_EQ(v1.length(), prefix.length() + 48);
    ASSERT_EQ(v1.substr(0, prefix.length()), prefix);

    auto v1again = ud.GetInstanceID();
    ASSERT_EQ(v1, v1again);

    // Clear and expect to get a different ID

    ud.Clear();

    auto v2 = ud.GetInstanceID();
    ASSERT_EQ(v2.length(), prefix.length() + 48);
    ASSERT_EQ(v2.substr(0, prefix.length()), prefix);
    ASSERT_NE(v1, v2);
}

TEST_F(TestUserData, HasInstanceID)
{
    auto temp_dir = GetTempDir();

    {
        UserData ud;
        auto err = ud.Init(temp_dir.c_str(), dev);
        ASSERT_FALSE(err);
        ASSERT_TRUE(ud.HasInstanceID());

        // Clear should generate a new instance ID
        ud.Clear();
        ASSERT_TRUE(ud.HasInstanceID());
    }

    {
        auto instance_id_ptr = "/instance/instanceID"_json_pointer;

        Datastore ds;
        auto err = ds.Init(temp_dir, GetSuffix(dev));
        ASSERT_FALSE(err);

        auto instance_id = ds.Get<string>(instance_id_ptr);
        ASSERT_TRUE(instance_id);
        ASSERT_THAT(*instance_id, Not(IsEmpty()));

        // Clear the instanceID so that it's absent later
        err = ds.Set(instance_id_ptr, nullptr);
        ASSERT_FALSE(err);
    }

    {
        // Now we shouldn't have an instance ID
        UserData ud;
        auto err = ud.Init(temp_dir.c_str(), dev);
        ASSERT_FALSE(err);
        ASSERT_FALSE(ud.HasInstanceID());
    }
}

TEST_F(TestUserData, IsLoggedOutAccount)
{
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str(), dev);
    ASSERT_FALSE(err);

    // Check default value
    auto v = ud.GetIsLoggedOutAccount();
    ASSERT_EQ(v, false);

    // Set then get
    bool want = true;
    err = ud.SetIsLoggedOutAccount(want);
    ASSERT_FALSE(err);
    auto got = ud.GetIsLoggedOutAccount();
    ASSERT_EQ(got, want);

    want = false;
    err = ud.SetIsLoggedOutAccount(want);
    ASSERT_FALSE(err);
    got = ud.GetIsLoggedOutAccount();
    ASSERT_EQ(got, want);
}

TEST_F(TestUserData, ServerTimeDiff)
{
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str(), dev);
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
    auto err = ud.Init(GetTempDir().c_str(), dev);
    ASSERT_FALSE(err);

    Purchase purchase_noexpiry{
        .id = "id",
        .server_time_created = datetime::DateTime(),
        .transaction_class = "tc",
        .distinguisher = "d"
    };
    datetime::DateTime server_expiry;
    ASSERT_TRUE(server_expiry.FromISO8601("2031-02-03T04:05:06.789Z"));
    Purchase purchase_expiry{
        .id = "id",
        .server_time_created = datetime::DateTime(),
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
    auto err = ud.Init(GetTempDir().c_str(), dev);
    ASSERT_FALSE(err);

    // Check default value
    auto tokens = ud.GetAuthTokens();
    ASSERT_EQ(tokens.size(), 0);

    auto is_account = ud.GetIsAccount();
    ASSERT_EQ(is_account, false);

    auto past = datetime::DateTime::Now().Sub(datetime::Duration(10000));
    auto future = datetime::DateTime::Now().Add(datetime::Duration(10000));

    AuthTokens want = {{"k1", {"v1", future}}, {"k2", {"v2", past}}};
    err = ud.SetAuthTokens(want, false, "");
    ASSERT_FALSE(err);
    auto got_tokens = ud.GetAuthTokens();
    ASSERT_TRUE(AuthTokenSetsEqual(want, got_tokens));

    is_account = ud.GetIsAccount();
    ASSERT_EQ(is_account, false);
    ASSERT_EQ(ud.GetAccountUsername(), "");

    err = ud.SetAuthTokens(want, true, "tokens-username");
    ASSERT_FALSE(err);
    got_tokens = ud.GetAuthTokens();
    ASSERT_TRUE(AuthTokenSetsEqual(want, got_tokens));
    is_account = ud.GetIsAccount();
    ASSERT_EQ(is_account, true);
    ASSERT_EQ(ud.GetAccountUsername(), "tokens-username");
}

TEST_F(TestUserData, CullAuthTokens)
{
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str(), dev);
    ASSERT_FALSE(err);

    AuthTokens auth_tokens = {{"k1",{"v1"}},{"k2",{"v2"}},{"k3",{"v3"}},{"k4",{"v4"}},};

    // All good
    err = ud.SetAuthTokens(auth_tokens, false, "");
    ASSERT_FALSE(err);
    std::map<std::string, bool> valid_tokens = {{"v1",true},{"v2",true},{"v3",true},{"v4",true}};
    err = ud.CullAuthTokens(valid_tokens);
    ASSERT_FALSE(err);
    ASSERT_TRUE(AuthTokenSetsEqual(auth_tokens, ud.GetAuthTokens()));

    // All present, one invalid
    err = ud.SetAuthTokens(auth_tokens, false, "");
    ASSERT_FALSE(err);
    valid_tokens = {{"v1",true},{"v2",false},{"v3",true},{"v4",true}};
    err = ud.CullAuthTokens(valid_tokens);
    ASSERT_FALSE(err);
    ASSERT_THAT(ud.GetAuthTokens(), IsEmpty());

    // All present, all invalid
    err = ud.SetAuthTokens(auth_tokens, false, "");
    ASSERT_FALSE(err);
    valid_tokens = {{"v1",false},{"v2",false},{"v3",false},{"v4",false}};
    err = ud.CullAuthTokens(valid_tokens);
    ASSERT_FALSE(err);
    ASSERT_THAT(ud.GetAuthTokens(), IsEmpty());

    // All valid, one missing
    err = ud.SetAuthTokens(auth_tokens, false, "");
    ASSERT_FALSE(err);
    valid_tokens = {{"v1",true},{"v3",true},{"v4",true}};
    err = ud.CullAuthTokens(valid_tokens);
    ASSERT_FALSE(err);
    ASSERT_THAT(ud.GetAuthTokens(), IsEmpty());

    // All missing
    err = ud.SetAuthTokens(auth_tokens, false, "");
    ASSERT_FALSE(err);
    valid_tokens = {};
    err = ud.CullAuthTokens(valid_tokens);
    ASSERT_FALSE(err);
    ASSERT_THAT(ud.GetAuthTokens(), IsEmpty());
}

TEST_F(TestUserData, ValidTokenTypes) {
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str(), dev);
    ASSERT_FALSE(err);

    ASSERT_EQ(ud.ValidTokenTypes().size(), 0);

    AuthTokens at = {{"a", {"a"}}, {"b", {"b"}}, {"c", {"c"}}};
    err = ud.SetAuthTokens(at, false, "");
    auto vtt = ud.ValidTokenTypes();
    ASSERT_EQ(vtt.size(), 3);
    for (const auto& k : vtt) {
        ASSERT_EQ(at.count(k), 1);
        at.erase(k);
    }
    ASSERT_EQ(at.size(), 0); // we should have erased all items

    AuthTokens empty;
    err = ud.SetAuthTokens(empty, false, "");
    vtt = ud.ValidTokenTypes();
    ASSERT_EQ(vtt.size(), 0);
}

TEST_F(TestUserData, IsAccount)
{
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str(), dev);
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

TEST_F(TestUserData, AccountUsername)
{
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str(), dev);
    ASSERT_FALSE(err);

    // Check default value
    auto v = ud.GetAccountUsername();
    ASSERT_EQ(v, "");

    // Set then get
    string want = "account-username";
    err = ud.SetAccountUsername(want);
    ASSERT_FALSE(err);
    auto got = ud.GetAccountUsername();
    ASSERT_EQ(got, want);

    // Set via SetAuthTokens
    want = "account-username";
    auto future = datetime::DateTime::Now().Add(datetime::Duration(10000));
    AuthTokens at = {{"a", {"a", future}}, {"b", {"b", future}}, {"c", {"c", future}}};
    err = ud.SetAuthTokens(at, true, want);
    got = ud.GetAccountUsername();
    ASSERT_EQ(got, want);

    // With tracker tokens -- so no username
    want = "";
    at = {{"a", {"a"}}, {"b", {"b"}}, {"c", {"c"}}};
    err = ud.SetAuthTokens(at, true, want);
    got = ud.GetAccountUsername();
    ASSERT_EQ(got, want);
}

TEST_F(TestUserData, Balance)
{
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str(), dev);
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
    auto err = ud.Init(GetTempDir().c_str(), dev);
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
    auto err = ud.Init(GetTempDir().c_str(), dev);
    ASSERT_FALSE(err);

    // Check default value
    auto v = ud.GetPurchases();
    ASSERT_EQ(v.size(), 0);

    // Set then get
    auto dt1 = datetime::DateTime::Now().Add(datetime::Duration(1));
    auto dt2 = datetime::DateTime::Now().Add(datetime::Duration(2));
    auto created1 = datetime::DateTime::Now().Sub(datetime::Duration(3));
    auto created2 = datetime::DateTime::Now().Sub(datetime::Duration(4));
    auto auth_res1 = psicash::DecodeAuthorization("eyJBdXRob3JpemF0aW9uIjp7IklEIjoibFRSWnBXK1d3TFJqYkpzOGxBUFVaQS8zWnhmcGdwNDFQY0dkdlI5a0RVST0iLCJBY2Nlc3NUeXBlIjoic3BlZWQtYm9vc3QtdGVzdCIsIkV4cGlyZXMiOiIyMDE5LTAxLTE0VDIxOjQ2OjMwLjcxNzI2NTkyNFoifSwiU2lnbmluZ0tleUlEIjoiUUNZTzV2clIvZGhjRDZ6M2FMQlVNeWRuZlJyZFNRL1RWYW1IUFhYeTd0TT0iLCJTaWduYXR1cmUiOiJtV1Z5Tm9ZU0pFRDNXU3I3bG1OeEtReEZza1M5ZWlXWG1lcDVvVWZBSHkwVmYrSjZaQW9WajZrN3ZVTDNrakIreHZQSTZyaVhQc3FzWENRNkx0eFdBQT09In0=");
    ASSERT_TRUE(auth_res1);

    Purchases want = {
        {"id1", created1, "tc1", "d1", dt1, dt2, *auth_res1},
        {"id2", created2, "tc2", "d2", nullopt, nullopt, nullopt}};

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
    want.push_back({"id3", created1, "tc3", "d3", server_now, nullopt, nullopt});
    err = ud.SetPurchases(want);
    got = ud.GetPurchases();
    ASSERT_EQ(got.size(), 3);
    ASSERT_TRUE(got[2].local_time_expiry);
    ASSERT_NEAR(got[2].local_time_expiry->MillisSinceEpoch(), local_now.MillisSinceEpoch(), 5);
}

TEST_F(TestUserData, AddPurchase)
{
    // This also tests Get/SetLastTransactionID (as Set isn't public)

    UserData ud;
    auto err = ud.Init(GetTempDir().c_str(), dev);
    ASSERT_FALSE(err);

    // Check default value
    auto v = ud.GetPurchases();
    ASSERT_EQ(v.size(), 0);

    // We need to check the behaviour for duplicates and out-of-chrono-order addition

    Purchases final_want = {
        {"id0", datetime::DateTime::Now().Sub(datetime::Duration(6)), "tc0", "d0", nullopt, nullopt, nullopt},
        {"id1", datetime::DateTime::Now().Sub(datetime::Duration(5)), "tc1", "d1", nullopt, nullopt, nullopt},
        {"id2", datetime::DateTime::Now().Sub(datetime::Duration(4)), "tc2", "d2", nullopt, nullopt, nullopt},
        {"id3", datetime::DateTime::Now().Sub(datetime::Duration(3)), "tc3", "d3", nullopt, nullopt, nullopt}};

    // Start with a subset
    Purchases want = {final_want[0], final_want[2]};
    err = ud.AddPurchase(want[0]);
    ASSERT_FALSE(err);
    err = ud.AddPurchase(want[1]);
    ASSERT_FALSE(err);
    auto got = ud.GetPurchases();
    ASSERT_EQ(got, want);
    ASSERT_EQ(ud.GetLastTransactionID(), "id2");

    // Add a later purchase
    want = {final_want[0], final_want[2], final_want[3]};
    err = ud.AddPurchase(final_want[3]);
    ASSERT_FALSE(err);
    got = ud.GetPurchases();
    ASSERT_EQ(got, want);
    ASSERT_EQ(ud.GetLastTransactionID(), "id3");

    // Add a purchase in the middle
    want = {final_want[0], final_want[1], final_want[2], final_want[3]};
    err = ud.AddPurchase(final_want[1]);
    ASSERT_FALSE(err);
    got = ud.GetPurchases();
    ASSERT_EQ(got, want);
    // Even though id1 is not the newest, it was added last and therefore will be the LastTransactionID. See comment in AddPurchase for details.
    ASSERT_EQ(ud.GetLastTransactionID(), "id1");

    // Add a duplicate purchase
    want = {final_want[0], final_want[1], final_want[2], final_want[3]};
    err = ud.AddPurchase(final_want[2]);
    ASSERT_FALSE(err);
    got = ud.GetPurchases();
    ASSERT_EQ(got, want);
    ASSERT_EQ(ud.GetLastTransactionID(), "id2");
}

TEST_F(TestUserData, Metadata)
{
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str(), dev);
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

TEST_F(TestUserData, StashMetadata)
{
    auto ds_dir = GetTempDir();

    {
        UserData ud;
        auto err = ud.Init(ds_dir.c_str(), dev);
        ASSERT_FALSE(err);

        ASSERT_EQ(ud.GetBalance(), 0);
        ASSERT_FALSE(ud.GetIsLoggedOutAccount());
        ASSERT_TRUE(ud.GetRequestMetadata().empty());

        ASSERT_FALSE(ud.SetBalance(1234L));
        ASSERT_EQ(ud.GetBalance(), 1234L);
        ASSERT_FALSE(ud.SetRequestMetadataItem("key1", "val1"));
        auto val1 = ud.GetRequestMetadata().at("key1").get<string>();
        ASSERT_EQ(val1, "val1");
        ASSERT_FALSE(ud.DeleteUserData(/*is_logged_out_account=*/true));

        ASSERT_EQ(ud.GetBalance(), 0);
        ASSERT_TRUE(ud.GetIsLoggedOutAccount());
        // We still have stashed metadata that should be accessible
        val1 = ud.GetRequestMetadata().at("key1").get<string>();
        ASSERT_EQ(val1, "val1");

        // Adding new metadata should result in the new item stored but not the stash
        ASSERT_FALSE(ud.SetRequestMetadataItem("key2", "val2"));
        auto val2 = ud.GetRequestMetadata().at("key2").get<string>();
        ASSERT_EQ(val2, "val2");
        // Ensure the original value is still available
        val1 = ud.GetRequestMetadata().at("key1").get<string>();
        ASSERT_EQ(val1, "val1");
    }

    // Close and reopen the datastore
    {
        UserData ud;
        auto err = ud.Init(ds_dir.c_str(), dev);
        ASSERT_FALSE(err);

        ASSERT_EQ(ud.GetBalance(), 0);
        ASSERT_TRUE(ud.GetIsLoggedOutAccount());

        // Our stashed metadata is gone, but the stored metadata is retained
        auto val2 = ud.GetRequestMetadata().at("key2").get<string>();
        ASSERT_EQ(val2, "val2");
        ASSERT_FALSE(ud.GetRequestMetadata().contains("val1"));

        ASSERT_FALSE(ud.SetBalance(1234L));
        ASSERT_EQ(ud.GetBalance(), 1234L);
        ASSERT_FALSE(ud.SetRequestMetadataItem("key1", "val1"));
        auto val1 = ud.GetRequestMetadata().at("key1").get<string>();
        ASSERT_EQ(val1, "val1");

        // Delete the user data again
        ASSERT_FALSE(ud.DeleteUserData(/*is_logged_out_account=*/true));

        ASSERT_EQ(ud.GetBalance(), 0);
        ASSERT_TRUE(ud.GetIsLoggedOutAccount());
        // We still have stashed metadata that should be accessible
        val1 = ud.GetRequestMetadata().at("key1").get<string>();
        ASSERT_EQ(val1, "val1");
        val2 = ud.GetRequestMetadata().at("key2").get<string>();
        ASSERT_EQ(val2, "val2");

        // Setting the auth tokens should make the stashed metadata permanent
        err = ud.SetAuthTokens({{"k1", {"v1", datetime::DateTime::Now()}}}, false, "");
    }

    // Close and reopen the datastore again
    {
        UserData ud;
        auto err = ud.Init(ds_dir.c_str(), dev);
        ASSERT_FALSE(err);

        // This time our request metadata should have persisted
        auto val1 = ud.GetRequestMetadata().at("key1").get<string>();
        ASSERT_EQ(val1, "val1");
        auto val2 = ud.GetRequestMetadata().at("key2").get<string>();
        ASSERT_EQ(val2, "val2");
    }
}

TEST_F(TestUserData, Locale)
{
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str(), dev);
    ASSERT_FALSE(err);

    auto v = ud.GetLocale();
    ASSERT_THAT(v, IsEmpty());

    err = ud.SetLocale("en-US");
    ASSERT_FALSE(err);
    v = ud.GetLocale();
    ASSERT_EQ(v, "en-US");

    err = ud.SetLocale("");
    ASSERT_FALSE(err);
    v = ud.GetLocale();
    ASSERT_EQ(v, "");
}

TEST_F(TestUserData, Transaction)
{
    UserData ud;
    auto err = ud.Init(GetTempDir().c_str(), dev);
    ASSERT_FALSE(err);

    // We're only using specific accessors for easy testing
    err = ud.SetLocale("1");
    ASSERT_FALSE(err);
    auto v = ud.GetLocale();
    ASSERT_EQ(v, "1");

    {
        UserData::Transaction udt(ud);
        err = ud.SetLocale("2");
        ASSERT_FALSE(err);
        auto v = ud.GetLocale();
        ASSERT_EQ(v, "2");
        // dtor rollback
    }
    v = ud.GetLocale();
    ASSERT_EQ(v, "1");

    {
        UserData::Transaction udt(ud);
        err = ud.SetLocale("3");
        ASSERT_FALSE(err);
        auto err = udt.Commit();
        ASSERT_FALSE(err);
    }
    v = ud.GetLocale();
    ASSERT_EQ(v, "3");

    {
        UserData::Transaction udt(ud);
        err = ud.SetLocale("4");
        ASSERT_FALSE(err);
        auto err = udt.Rollback();
        ASSERT_FALSE(err);
    }
    v = ud.GetLocale();
    ASSERT_EQ(v, "3");

    {
        UserData::Transaction udt(ud);
        err = ud.SetLocale("5");
        ASSERT_FALSE(err);
        auto err = udt.Commit();
        ASSERT_FALSE(err);
        err = udt.Rollback(); // does nothing
        ASSERT_FALSE(err);
        err = udt.Commit(); // does nothing
        ASSERT_FALSE(err);
    }
    v = ud.GetLocale();
    ASSERT_EQ(v, "5");

    {
        UserData::Transaction udt(ud);
        auto err = udt.Rollback();
        ASSERT_FALSE(err);
        // Modify _after_ rollback
        err = ud.SetLocale("6");
        ASSERT_FALSE(err);
        // Extra rollback should do nothing
        err = udt.Rollback();
        ASSERT_FALSE(err);
    }
    v = ud.GetLocale();
    ASSERT_EQ(v, "6");

    {
        UserData::Transaction udt(ud);

        {
            UserData::Transaction udt(ud);

            {
                UserData::Transaction udt(ud);
                err = ud.SetLocale("7");
                ASSERT_FALSE(err);
                // inner commit does nothing
                auto err = udt.Commit();
                ASSERT_FALSE(err);
            }

            auto v = ud.GetIsAccount();
            ASSERT_EQ(v, false);
            err = ud.SetIsAccount(true);
            ASSERT_FALSE(err);
            // inner rollback does nothing
            auto err = udt.Rollback();
            ASSERT_FALSE(err);
        }

        // We have committed one inner transaction and rolled back the another, but they have no effect on the outer.
        // Now we're commit the outer transaction.
        udt.Commit();
    }
    v = ud.GetLocale();
    ASSERT_EQ(v, "7");
    auto b = ud.GetIsAccount();
    ASSERT_EQ(b, true);
}
