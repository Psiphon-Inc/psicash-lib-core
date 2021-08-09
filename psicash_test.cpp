#include "SecretTestValues.h" // This file is in CipherShare
#include "base64.hpp"
#include "http_status_codes.h"
#include "vendor/nlohmann/json.hpp"
#include "psicash.hpp"
#include "test_helpers.hpp"
#include "url.hpp"
#include "userdata.hpp"
#include "psicash_tester.hpp"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include <regex>
#include <thread>
using json = nlohmann::json;

// Requires `apt install libssl-dev`
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "vendor/httplib.h"

using namespace std;
using namespace psicash;
using namespace testing;

constexpr int64_t MAX_STARTING_BALANCE = 100000000000LL;

class TestPsiCash : public ::testing::Test, public TempDir {
  public:
    TestPsiCash() { }

    static string UserAgent() { return "Psiphon-PsiCash-iOS"; }

    static HTTPResult HTTPRequester(const HTTPParams& params) {
        httplib::Params query_params;
        for (const auto& qp : params.query) {
            query_params.emplace(qp.first, qp.second);
        }

        stringstream url;
        url << params.scheme << "://" << params.hostname << ":" << params.port;
        httplib::Client http_client(url.str());

        httplib::Headers headers;
        for (const auto& h : params.headers) {
            headers.emplace(h.first, h.second);
        }

        stringstream path_query;
        path_query << params.path << "?" << httplib::detail::params_to_query_str(query_params);

        nonstd::optional<httplib::Result> res;
        if (params.method == "GET") {
            res = http_client.Get(path_query.str().c_str(), headers);
        }
        else if (params.method == "POST") {
            res = http_client.Post(path_query.str().c_str(), headers, params.body.c_str(), params.body.length(), "application/json");
        }
        else if (params.method == "PUT") {
            res = http_client.Put(path_query.str().c_str(), headers, params.body.c_str(), params.body.length(), "application/json");
        }
        else {
            throw std::invalid_argument("unsupported request method: "s + params.method);
        }

        HTTPResult result;
        if (res->error() != httplib::Error::Success) {
            stringstream err;
            err << "request error: " << res->error();
            result.error = err.str();
            return result;
        }
        else if (!*res) {
            result.error = "request failed utterly";
            return result;
        }

        result.code = (*res)->status;
        result.body = (*res)->body;

        for (const auto& h : (*res)->headers) {
            // This won't cope correctly with multiple headers of the same name
            vector<string> v = {h.second};
            result.headers.emplace(h.first, v);
        }

        return result;
    }

    // Return a specific result for a HTTP request
    static psicash::MakeHTTPRequestFn FakeHTTPRequester(const HTTPResult& result) {
        return [=](const HTTPParams& params) -> HTTPResult {
            return result;
        };
    }
};

#define MAKE_1T_REWARD(pc, count) (pc.MakeRewardRequests(TEST_CREDIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, count))


TEST_F(TestPsiCash, InitSimple) {
    {
        PsiCashTester pc;
        ASSERT_FALSE(pc.Initialized());
        // Force Init to test=false to test that path (you should typically not do this in tests)
        auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false, false);
        ASSERT_FALSE(err);
        ASSERT_TRUE(pc.Initialized());
    }

    {
        PsiCashTester pc;
        ASSERT_FALSE(pc.Initialized());
        // Force Init to test=true to test that path (you should typically not do this in tests)
        auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false, true);
        ASSERT_FALSE(err);
        ASSERT_TRUE(pc.Initialized());
    }
}

TEST_F(TestPsiCash, InitReset) {
    auto temp_dir = GetTempDir();
    string expected_instance_id;
    {
        // Set up some state
        PsiCashTester pc;
        ASSERT_FALSE(pc.Initialized());
        auto err = pc.Init(TestPsiCash::UserAgent(), temp_dir.c_str(), HTTPRequester, false);
        ASSERT_FALSE(err);
        ASSERT_TRUE(pc.Initialized());

        expected_instance_id = pc.user_data().GetInstanceID();

        auto res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
        ASSERT_TRUE(res_login) << res_login.error();
        ASSERT_EQ(res_login->status, Status::Success);
        ASSERT_TRUE(pc.IsAccount());
        ASSERT_TRUE(pc.HasTokens());
        ASSERT_TRUE(pc.AccountUsername());
        ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_ONE_USERNAME);
    }
    {
        // Check that the state persists
        PsiCashTester pc;
        ASSERT_FALSE(pc.Initialized());
        auto err = pc.Init(TestPsiCash::UserAgent(), temp_dir.c_str(), HTTPRequester, false);
        ASSERT_FALSE(err);
        ASSERT_TRUE(pc.Initialized());

        ASSERT_EQ(pc.user_data().GetInstanceID(), expected_instance_id);
        ASSERT_TRUE(pc.IsAccount());
        ASSERT_TRUE(pc.HasTokens());
        ASSERT_TRUE(pc.AccountUsername());
        ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_ONE_USERNAME);
    }
    {
        // Init with reset, previous state should be gone
        PsiCashTester pc;
        ASSERT_FALSE(pc.Initialized());
        auto err = pc.Init(TestPsiCash::UserAgent(), temp_dir.c_str(), HTTPRequester, true);
        ASSERT_FALSE(err);
        ASSERT_TRUE(pc.Initialized());

        ASSERT_NE(pc.user_data().GetInstanceID(), expected_instance_id);
        ASSERT_FALSE(pc.IsAccount());
        ASSERT_FALSE(pc.HasTokens());
        ASSERT_FALSE(pc.AccountUsername());
    }
}

TEST_F(TestPsiCash, InitFail) {
    {
        // Datastore directory that will not work
        auto bad_dir = GetTempDir() + "/a/b/c/d/f/g";
        PsiCashTester pc;
        auto err = pc.Init(TestPsiCash::UserAgent(), bad_dir.c_str(), nullptr, false);
        ASSERT_TRUE(err) << bad_dir;
        ASSERT_FALSE(pc.Initialized());
    }
    {
        // Empty datastore directory
        PsiCashTester pc;
        auto err = pc.Init(TestPsiCash::UserAgent(), "", nullptr, false);
        ASSERT_TRUE(err);
        ASSERT_FALSE(pc.Initialized());
    }
    {
        // Empty user agent
        PsiCashTester pc;
        auto err = pc.Init("", GetTempDir().c_str(), nullptr, false);
        ASSERT_TRUE(err);
        ASSERT_FALSE(pc.Initialized());
    }
}

TEST_F(TestPsiCash, UninitializedBehaviour) {
    {
        // No Init
        PsiCashTester pc;
        ASSERT_FALSE(pc.Initialized());

        ASSERT_EQ(pc.Balance(), 0);

        auto res = pc.RefreshState(false, {"speed-boost"});
        ASSERT_FALSE(res);
    }
    {
        // Failed Init
        auto bad_dir = GetTempDir() + "/a/b/c/d/f/g";
        PsiCashTester pc;
        auto err = pc.Init(TestPsiCash::UserAgent(), bad_dir.c_str(), nullptr, false);
        ASSERT_TRUE(err) << bad_dir;

        ASSERT_FALSE(pc.Initialized());

        ASSERT_EQ(pc.Balance(), 0);

        auto res = pc.RefreshState(false, {"speed-boost"});
        ASSERT_FALSE(res);
    }
}

TEST_F(TestPsiCash, MigrateTrackerTokens) {
    {
        // Without calling RefreshState first (so no preexisting tokens); tracker.
        PsiCashTester pc;
        auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
        ASSERT_FALSE(err);

        ASSERT_FALSE(pc.HasTokens());

        map<string, string> tokens = {{"a", "a"}, {"b", "b"}, {"c", "c"}};
        err = pc.MigrateTrackerTokens(tokens);
        ASSERT_FALSE(err);
        ASSERT_EQ(pc.user_data().ValidTokenTypes().size(), 3);
        ASSERT_FALSE(pc.IsAccount());
        ASSERT_FALSE(pc.AccountUsername());
    }
    {
        // Call RefreshState first (so preexisting tokens and user data).
        PsiCashTester pc;
        auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
        ASSERT_FALSE(err);

        ASSERT_FALSE(pc.HasTokens());

        auto res = pc.RefreshState(false, {"speed-boost"});
        ASSERT_TRUE(res) << res.error();
        ASSERT_EQ(res->status, Status::Success) << (int)res->status;
        ASSERT_FALSE(pc.IsAccount());
        ASSERT_FALSE(pc.AccountUsername());
        ASSERT_TRUE(pc.HasTokens());
        ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));
        ASSERT_GE(pc.GetPurchasePrices().size(), 2);

        map<string, string> tokens = {{"a", "a"}, {"b", "b"}, {"c", "c"}};
        AuthTokens auth_tokens = {{"a", {"a"}}, {"b", {"b"}}, {"c", {"c"}}};
        err = pc.MigrateTrackerTokens(tokens);
        ASSERT_FALSE(err);
        ASSERT_TRUE(AuthTokenSetsEqual(pc.user_data().GetAuthTokens(), auth_tokens));
        ASSERT_FALSE(pc.IsAccount());
        ASSERT_FALSE(pc.AccountUsername());
        ASSERT_EQ(pc.Balance(), 0);
        ASSERT_GE(pc.GetPurchasePrices().size(), 0);
    }
}

TEST_F(TestPsiCash, SetHTTPRequestFn) {
    {
        PsiCashTester pc;
        auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
        ASSERT_FALSE(err);
        pc.SetHTTPRequestFn(HTTPRequester);
    }

    {
        PsiCashTester pc;
        auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false);
        ASSERT_FALSE(err);
        pc.SetHTTPRequestFn(HTTPRequester);
    }
}

TEST_F(TestPsiCash, SetRequestMetadataItem) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false);
    ASSERT_FALSE(err);

    auto j = pc.user_data().GetRequestMetadata();
    ASSERT_EQ(j.size(), 0);

    err = pc.SetRequestMetadataItem("k", "v");
    ASSERT_FALSE(err);

    j = pc.user_data().GetRequestMetadata();
    ASSERT_EQ(j["k"], "v");
}

TEST_F(TestPsiCash, SetLocale) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false);
    ASSERT_FALSE(err);

    auto url = pc.GetUserSiteURL(PsiCash::UserSiteURLType::AccountSignup, false);
    ASSERT_THAT(url, EndsWith("locale="));

    err = pc.SetLocale("en-US");
    ASSERT_FALSE(err);

    url = pc.GetUserSiteURL(PsiCash::UserSiteURLType::AccountSignup, false);
    ASSERT_THAT(url, HasSubstr("locale=en-US"));
}

TEST_F(TestPsiCash, IsAccount) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false);
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

TEST_F(TestPsiCash, HasTokens) {
    {
        PsiCashTester pc;
        auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false);
        ASSERT_FALSE(err);

        ASSERT_FALSE(pc.HasTokens());

        // No expiry, bad types
        AuthTokens at = {{"a", {"a"}}, {"b", {"b"}}, {"c", {"c"}}};
        err = pc.user_data().SetAuthTokens(at, false, "");
        ASSERT_FALSE(pc.HasTokens());

        // No expiry, good types
        at = {{"earner", {"a"}}, {"indicator", {"b"}}, {"spender", {"c"}}};
        err = pc.user_data().SetAuthTokens(at, false, "");
        ASSERT_TRUE(pc.HasTokens());

        // Expiries in the future, bad types
        auto future = datetime::DateTime::Now().Add(datetime::Duration(10000));
        at = {{"a", {"a", future}}, {"b", {"b", future}}, {"c", {"c", future}}};
        err = pc.user_data().SetAuthTokens(at, true, "username");
        ASSERT_FALSE(pc.HasTokens());

        // Expiries in the future, good types
        at = {{"indicator", {"a", future}}, {"spender", {"b", future}}, {"earner", {"c", future}}, {"logout", {"c", future}}};
        err = pc.user_data().SetAuthTokens(at, true, "username");
        ASSERT_TRUE(pc.HasTokens());

        // One expiry in the past, good types
        auto past = datetime::DateTime::Now().Sub(datetime::Duration(10000));
        at = {{"indicator", {"a", past}}, {"spender", {"b", future}}, {"earner", {"c", future}}, {"logout", {"c", future}}};
        err = pc.user_data().SetAuthTokens(at, true, "username");
        ASSERT_TRUE(pc.HasTokens());
    }
    {
        PsiCashTester pc;
        auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
        ASSERT_FALSE(err);
        ASSERT_FALSE(pc.HasTokens());

        auto res = pc.RefreshState(false, {"speed-boost"});
        ASSERT_TRUE(res) << res.error();
        ASSERT_EQ(res->status, Status::Success) << (int)res->status;
        ASSERT_TRUE(pc.HasTokens());
    }
    {
        PsiCashTester pc;
        auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
        ASSERT_FALSE(err);
        ASSERT_FALSE(pc.HasTokens());

        auto res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
        ASSERT_TRUE(res_login) << res_login.error();
        ASSERT_EQ(res_login->status, Status::Success);
        ASSERT_TRUE(pc.HasTokens());
    }
}

TEST_F(TestPsiCash, Balance) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false);
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
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false);
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
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false);
    ASSERT_FALSE(err);

    auto v = pc.GetPurchases();
    ASSERT_EQ(v.size(), 0);

    auto auth_res = DecodeAuthorization("eyJBdXRob3JpemF0aW9uIjp7IklEIjoiMFYzRXhUdmlBdFNxTGZOd2FpQXlHNHpaRUJJOGpIYnp5bFdNeU5FZ1JEZz0iLCJBY2Nlc3NUeXBlIjoic3BlZWQtYm9vc3QtdGVzdCIsIkV4cGlyZXMiOiIyMDE5LTAxLTE0VDE3OjIyOjIzLjE2ODc2NDEyOVoifSwiU2lnbmluZ0tleUlEIjoiUUNZTzV2clIvZGhjRDZ6M2FMQlVNeWRuZlJyZFNRL1RWYW1IUFhYeTd0TT0iLCJTaWduYXR1cmUiOiJQL2NrenloVUJoSk5RQ24zMnluM1VTdGpLencxU04xNW9MclVhTU9XaW9scXBOTTBzNVFSNURHVEVDT1FzQk13ODdQdTc1TGE1OGtJTHRIcW1BVzhDQT09In0=");
    ASSERT_TRUE(auth_res);

    Purchases ps = {
            {"id1", datetime::DateTime::Now(), "tc1", "d1", datetime::DateTime::Now(), datetime::DateTime::Now(), *auth_res},
            {"id2", datetime::DateTime::Now(), "tc2", "d2", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt}};

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

TEST_F(TestPsiCash, ActivePurchases) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false);
    ASSERT_FALSE(err);

    auto v = pc.GetPurchases();
    ASSERT_EQ(v.size(), 0);

    v = pc.ActivePurchases();
    ASSERT_EQ(v.size(), 0);

    auto created = datetime::DateTime(); // value doesn't matter here
    auto before_now = datetime::DateTime::Now().Sub(datetime::Duration(54321));
    auto after_now = datetime::DateTime::Now().Add(datetime::Duration(54321));

    Purchases ps = {{"id1", created, "tc1", "d1", before_now, nonstd::nullopt, nonstd::nullopt},
                    {"id2", created, "tc2", "d2", after_now, nonstd::nullopt, nonstd::nullopt},
                    {"id3", created, "tc3", "d3", before_now, nonstd::nullopt, nonstd::nullopt},
                    {"id4", created, "tc4", "d4", after_now, nonstd::nullopt, nonstd::nullopt},
                    {"id5", created, "tc5", "d5", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt}};

    err = pc.user_data().SetPurchases(ps);
    ASSERT_FALSE(err);

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), ps.size());
    ASSERT_EQ(v, ps);

    v = pc.ActivePurchases();
    ASSERT_EQ(v.size(), 3);
    // There's no guarantee that the order of purchases won't change, but we know they won't
    ASSERT_EQ(v[0].id, "id2");
    ASSERT_EQ(v[1].id, "id4");
    ASSERT_EQ(v[2].id, "id5");

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), ps.size());
    ASSERT_EQ(v, ps);
}

TEST_F(TestPsiCash, DecodeAuthorization) {
    const auto encoded1 = "eyJBdXRob3JpemF0aW9uIjp7IklEIjoiMFYzRXhUdmlBdFNxTGZOd2FpQXlHNHpaRUJJOGpIYnp5bFdNeU5FZ1JEZz0iLCJBY2Nlc3NUeXBlIjoic3BlZWQtYm9vc3QtdGVzdCIsIkV4cGlyZXMiOiIyMDE5LTAxLTE0VDE3OjIyOjIzLjE2ODc2NDEyOVoifSwiU2lnbmluZ0tleUlEIjoiUUNZTzV2clIvZGhjRDZ6M2FMQlVNeWRuZlJyZFNRL1RWYW1IUFhYeTd0TT0iLCJTaWduYXR1cmUiOiJQL2NrenloVUJoSk5RQ24zMnluM1VTdGpLencxU04xNW9MclVhTU9XaW9scXBOTTBzNVFSNURHVEVDT1FzQk13ODdQdTc1TGE1OGtJTHRIcW1BVzhDQT09In0=";
    const auto encoded2 = "eyJBdXRob3JpemF0aW9uIjp7IklEIjoibFRSWnBXK1d3TFJqYkpzOGxBUFVaQS8zWnhmcGdwNDFQY0dkdlI5a0RVST0iLCJBY2Nlc3NUeXBlIjoic3BlZWQtYm9vc3QtdGVzdCIsIkV4cGlyZXMiOiIyMDE5LTAxLTE0VDIxOjQ2OjMwLjcxNzI2NTkyNFoifSwiU2lnbmluZ0tleUlEIjoiUUNZTzV2clIvZGhjRDZ6M2FMQlVNeWRuZlJyZFNRL1RWYW1IUFhYeTd0TT0iLCJTaWduYXR1cmUiOiJtV1Z5Tm9ZU0pFRDNXU3I3bG1OeEtReEZza1M5ZWlXWG1lcDVvVWZBSHkwVmYrSjZaQW9WajZrN3ZVTDNrakIreHZQSTZyaVhQc3FzWENRNkx0eFdBQT09In0=";

    auto auth_res1 = psicash::DecodeAuthorization(encoded1);
    auto auth_res2 = psicash::DecodeAuthorization(encoded2);

    ASSERT_TRUE(auth_res1);
    ASSERT_TRUE(auth_res2);
    ASSERT_TRUE(*auth_res1 == *auth_res1);
    ASSERT_FALSE(*auth_res1 == *auth_res2);
    ASSERT_EQ(auth_res1->id, "0V3ExTviAtSqLfNwaiAyG4zZEBI8jHbzylWMyNEgRDg=");
    ASSERT_EQ(auth_res1->access_type, "speed-boost-test");

    psicash::datetime::DateTime want_date;
    ASSERT_TRUE(want_date.FromISO8601("2019-01-14T17:22:23.168764129Z"));
    ASSERT_EQ(auth_res1->expires, want_date) << auth_res1->expires.ToISO8601();

    auto invalid_base64 = "BAD-BASE64-$^#&*(@===============";
    auto auth_res_fail = psicash::DecodeAuthorization(invalid_base64);
    ASSERT_FALSE(auth_res_fail);
    ASSERT_TRUE(auth_res_fail.error().Critical());

    auto invalid_json = "dGhpcyBpcyBub3QgdmFsaWQgSlNPTg==";
    auth_res_fail = psicash::DecodeAuthorization(invalid_json);
    ASSERT_FALSE(auth_res_fail);
    ASSERT_TRUE(auth_res_fail.error().Critical());

    auto incorrect_json = "eyJ2YWxpZCI6ICJqc29uIiwgImJ1dCI6ICJub3QgYSB2YWxpZCBhdXRob3JpemF0aW9uIn0=";
    auth_res_fail = psicash::DecodeAuthorization(incorrect_json);
    ASSERT_FALSE(auth_res_fail);
    ASSERT_TRUE(auth_res_fail.error().Critical());
}

TEST_F(TestPsiCash, GetAuthorizations) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false);
    ASSERT_FALSE(err);

    auto purchases = pc.GetPurchases();
    ASSERT_EQ(purchases.size(), 0);

    purchases = pc.ActivePurchases();
    ASSERT_EQ(purchases.size(), 0);

    auto v = pc.GetAuthorizations(false);
    ASSERT_EQ(v.size(), 0);

    v = pc.GetAuthorizations(true);
    ASSERT_EQ(v.size(), 0);

    auto before_now = datetime::DateTime::Now().Sub(datetime::Duration(54321));
    auto after_now = datetime::DateTime::Now().Add(datetime::Duration(54321));
    auto created = datetime::DateTime(); // doesn't matter

    const auto encoded1 = "eyJBdXRob3JpemF0aW9uIjp7IklEIjoiMFYzRXhUdmlBdFNxTGZOd2FpQXlHNHpaRUJJOGpIYnp5bFdNeU5FZ1JEZz0iLCJBY2Nlc3NUeXBlIjoic3BlZWQtYm9vc3QtdGVzdCIsIkV4cGlyZXMiOiIyMDE5LTAxLTE0VDE3OjIyOjIzLjE2ODc2NDEyOVoifSwiU2lnbmluZ0tleUlEIjoiUUNZTzV2clIvZGhjRDZ6M2FMQlVNeWRuZlJyZFNRL1RWYW1IUFhYeTd0TT0iLCJTaWduYXR1cmUiOiJQL2NrenloVUJoSk5RQ24zMnluM1VTdGpLencxU04xNW9MclVhTU9XaW9scXBOTTBzNVFSNURHVEVDT1FzQk13ODdQdTc1TGE1OGtJTHRIcW1BVzhDQT09In0=";
    const auto encoded2 = "eyJBdXRob3JpemF0aW9uIjp7IklEIjoibFRSWnBXK1d3TFJqYkpzOGxBUFVaQS8zWnhmcGdwNDFQY0dkdlI5a0RVST0iLCJBY2Nlc3NUeXBlIjoic3BlZWQtYm9vc3QtdGVzdCIsIkV4cGlyZXMiOiIyMDE5LTAxLTE0VDIxOjQ2OjMwLjcxNzI2NTkyNFoifSwiU2lnbmluZ0tleUlEIjoiUUNZTzV2clIvZGhjRDZ6M2FMQlVNeWRuZlJyZFNRL1RWYW1IUFhYeTd0TT0iLCJTaWduYXR1cmUiOiJtV1Z5Tm9ZU0pFRDNXU3I3bG1OeEtReEZza1M5ZWlXWG1lcDVvVWZBSHkwVmYrSjZaQW9WajZrN3ZVTDNrakIreHZQSTZyaVhQc3FzWENRNkx0eFdBQT09In0=";

    auto auth_res1 = psicash::DecodeAuthorization(encoded1);
    auto auth_res2 = psicash::DecodeAuthorization(encoded2);
    ASSERT_TRUE(auth_res1);
    ASSERT_TRUE(auth_res2);

    purchases = {{"future_no_auth", created, "tc1", "d1", after_now, nonstd::nullopt, nonstd::nullopt},
                 {"past_auth", created, "tc2", "d2", before_now, nonstd::nullopt, *auth_res1},
                 {"future_auth", created, "tc3", "d3", after_now, nonstd::nullopt, *auth_res2}};

    err = pc.user_data().SetPurchases(purchases);
    ASSERT_FALSE(err);
    ASSERT_EQ(pc.GetPurchases().size(), purchases.size());

    v = pc.GetAuthorizations(false);
    ASSERT_EQ(v.size(), 2);
    ASSERT_THAT(v, AnyOf(Contains(*auth_res1), Contains(*auth_res2)));

    v = pc.GetAuthorizations(true);
    ASSERT_EQ(v.size(), 1);
    ASSERT_THAT(v, Contains(*auth_res2));
}

TEST_F(TestPsiCash, GetPurchasesByAuthorizationID) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false);
    ASSERT_FALSE(err);

    auto purchases = pc.GetPurchases();
    ASSERT_EQ(purchases.size(), 0);

    // Empty set of auth IDs
    purchases = pc.GetPurchasesByAuthorizationID({});
    ASSERT_EQ(purchases.size(), 0);

    auto before_now = datetime::DateTime::Now().Sub(datetime::Duration(54321));
    auto after_now = datetime::DateTime::Now().Add(datetime::Duration(54321));
    auto created = datetime::DateTime(); // doesn't matter

    const auto encoded1 = "eyJBdXRob3JpemF0aW9uIjp7IklEIjoiMFYzRXhUdmlBdFNxTGZOd2FpQXlHNHpaRUJJOGpIYnp5bFdNeU5FZ1JEZz0iLCJBY2Nlc3NUeXBlIjoic3BlZWQtYm9vc3QtdGVzdCIsIkV4cGlyZXMiOiIyMDE5LTAxLTE0VDE3OjIyOjIzLjE2ODc2NDEyOVoifSwiU2lnbmluZ0tleUlEIjoiUUNZTzV2clIvZGhjRDZ6M2FMQlVNeWRuZlJyZFNRL1RWYW1IUFhYeTd0TT0iLCJTaWduYXR1cmUiOiJQL2NrenloVUJoSk5RQ24zMnluM1VTdGpLencxU04xNW9MclVhTU9XaW9scXBOTTBzNVFSNURHVEVDT1FzQk13ODdQdTc1TGE1OGtJTHRIcW1BVzhDQT09In0=";
    const auto encoded2 = "eyJBdXRob3JpemF0aW9uIjp7IklEIjoibFRSWnBXK1d3TFJqYkpzOGxBUFVaQS8zWnhmcGdwNDFQY0dkdlI5a0RVST0iLCJBY2Nlc3NUeXBlIjoic3BlZWQtYm9vc3QtdGVzdCIsIkV4cGlyZXMiOiIyMDE5LTAxLTE0VDIxOjQ2OjMwLjcxNzI2NTkyNFoifSwiU2lnbmluZ0tleUlEIjoiUUNZTzV2clIvZGhjRDZ6M2FMQlVNeWRuZlJyZFNRL1RWYW1IUFhYeTd0TT0iLCJTaWduYXR1cmUiOiJtV1Z5Tm9ZU0pFRDNXU3I3bG1OeEtReEZza1M5ZWlXWG1lcDVvVWZBSHkwVmYrSjZaQW9WajZrN3ZVTDNrakIreHZQSTZyaVhQc3FzWENRNkx0eFdBQT09In0=";

    auto auth_res1 = psicash::DecodeAuthorization(encoded1);
    auto auth_res2 = psicash::DecodeAuthorization(encoded2);
    ASSERT_TRUE(auth_res1);
    ASSERT_TRUE(auth_res2);

    purchases = {{"future_no_auth", created, "tc1", "d1", after_now, nonstd::nullopt, nonstd::nullopt},
                 {"past_auth", created, "tc2", "d2", before_now, nonstd::nullopt, *auth_res1},
                 {"future_auth", created, "tc3", "d3", after_now, nonstd::nullopt, *auth_res2}};

    err = pc.user_data().SetPurchases(purchases);
    ASSERT_FALSE(err);
    ASSERT_EQ(pc.GetPurchases().size(), purchases.size());

    // One that matches and some that don't
    vector<string> authIDs = {"badid", auth_res2->id, "anotherbadid"};
    purchases = pc.GetPurchasesByAuthorizationID(authIDs);
    ASSERT_THAT(purchases, SizeIs(1));
    ASSERT_EQ(purchases[0].authorization->id, auth_res2->id);
}

TEST_F(TestPsiCash, NextExpiringPurchase) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false);
    ASSERT_FALSE(err);

    auto v = pc.GetPurchases();
    ASSERT_EQ(v.size(), 0);

    auto p = pc.NextExpiringPurchase();
    ASSERT_FALSE(p);

    auto first = datetime::DateTime::Now().Sub(datetime::Duration(333));
    auto second = datetime::DateTime::Now().Sub(datetime::Duration(222));
    auto third = datetime::DateTime::Now().Sub(datetime::Duration(111));
    auto created = datetime::DateTime(); // doesn't matter

    Purchases ps = {{"id1", created, "tc1", "d1", second, nonstd::nullopt, nonstd::nullopt},
                    {"id2", created, "tc2", "d2", first, nonstd::nullopt, nonstd::nullopt}, // first to expire
                    {"id3", created, "tc3", "d3", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
                    {"id4", created, "tc4", "d4", third, nonstd::nullopt, nonstd::nullopt}};

    err = pc.user_data().SetPurchases(ps);
    ASSERT_FALSE(err);

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), ps.size());
    ASSERT_EQ(v, ps);

    p = pc.NextExpiringPurchase();
    ASSERT_TRUE(p);
    ASSERT_EQ(p->id, ps[1].id);

    auto later_than_now = datetime::DateTime::Now().Add(datetime::Duration(54321));
    ps = {{"id1", created, "tc1", "d1", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
          {"id2", created, "tc2", "d2", later_than_now, nonstd::nullopt, nonstd::nullopt}, // only expiring
          {"id3", created, "tc3", "d3", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt}};

    err = pc.user_data().SetPurchases(ps);
    ASSERT_FALSE(err);

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), ps.size());
    ASSERT_EQ(v, ps);

    p = pc.NextExpiringPurchase();
    ASSERT_TRUE(p);
    ASSERT_EQ(p->id, ps[1].id);

    // None expiring
    ps = {{"id1", created, "tc1", "d1", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
          {"id2", created, "tc2", "d2", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt}};

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
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false);
    ASSERT_FALSE(err);

    auto v = pc.GetPurchases();
    ASSERT_EQ(v.size(), 0);

    auto res = pc.ExpirePurchases();
    ASSERT_TRUE(res);
    ASSERT_EQ(res->size(), 0);

    auto before_now = datetime::DateTime::Now().Sub(datetime::Duration(54321));
    auto after_now = datetime::DateTime::Now().Add(datetime::Duration(54321));
    auto created = datetime::DateTime(); // doesn't matter

    Purchases ps = {{"id1", created, "tc1", "d1", after_now, nonstd::nullopt, nonstd::nullopt},
                    {"id2", created, "tc2", "d2", before_now, nonstd::nullopt, nonstd::nullopt},
                    {"id3", created, "tc3", "d3", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
                    {"id4", created, "tc4", "d4", before_now, nonstd::nullopt, nonstd::nullopt}};
    Purchases expired = {ps[1], ps[3]};
    Purchases nonexpired = {ps[0], ps[2]};

    err = pc.user_data().SetPurchases(ps);
    ASSERT_FALSE(err);

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), ps.size());
    ASSERT_EQ(v, ps);

    res = pc.ExpirePurchases();
    ASSERT_TRUE(res);
    ASSERT_EQ(res->size(), expired.size());
    ASSERT_EQ(*res, expired);

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), nonexpired.size());
    ASSERT_EQ(v, nonexpired);

    // No expired purchases left
    res = pc.ExpirePurchases();
    ASSERT_TRUE(res);
    ASSERT_EQ(res->size(), 0);

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), nonexpired.size());
    ASSERT_EQ(v, nonexpired);
}

TEST_F(TestPsiCash, RemovePurchases) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false);
    ASSERT_FALSE(err);

    auto v = pc.GetPurchases();
    ASSERT_EQ(v.size(), 0);

    auto created = datetime::DateTime(); // doesn't matter

    Purchases ps = {{"id1", created, "tc1", "d1", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
                    {"id2", created, "tc2", "d2", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
                    {"id3", created, "tc3", "d3", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
                    {"id4", created, "tc4", "d4", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt}};
    vector<TransactionID> remove_ids = {ps[1].id, ps[3].id};
    Purchases remaining = {ps[0], ps[2]};

    err = pc.user_data().SetPurchases(ps);
    ASSERT_FALSE(err);

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), ps.size());
    ASSERT_EQ(v, ps);

    auto removed = pc.RemovePurchases(remove_ids);
    ASSERT_TRUE(removed);
    ASSERT_EQ(remove_ids.size(), removed->size());
    ASSERT_TRUE((VectorSetsMatch<TransactionID, Purchase>(
        remove_ids, *removed, [](const Purchase& p) -> TransactionID { return p.id; })));

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), remaining.size());
    ASSERT_EQ(v, remaining);

    // remove_ids are not present now
    removed = pc.RemovePurchases(remove_ids);
    ASSERT_TRUE(removed);
    ASSERT_EQ(0, removed->size());

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), remaining.size());
    ASSERT_EQ(v, remaining);

    // empty array
    removed = pc.RemovePurchases({});
    ASSERT_TRUE(removed);
    ASSERT_EQ(0, removed->size());

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), remaining.size());
    ASSERT_EQ(v, remaining);

    // totally fake IDs
    removed = pc.RemovePurchases({"invalid1", "invalid2"});
    ASSERT_TRUE(removed);
    ASSERT_EQ(0, removed->size());

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), remaining.size());
    ASSERT_EQ(v, remaining);
}

// Returns empty string on match
string TokenPayloadsMatch(const string &got_base64, const json& want_incomplete) {
    auto want = want_incomplete;
    want["v"] = 1;
    want["metadata"]["v"] = 1;
    want["metadata"]["user_agent"] = TestPsiCash::UserAgent();

    // Extract the timestamp we got, then remove so we can compare the rest
    auto got = json::parse(base64::B64Decode(got_base64));
    datetime::DateTime got_tokens_timestamp;
    if (!got_tokens_timestamp.FromISO8601(got.at("timestamp").get<string>())) {
        return "failed to extract timestamp from got_base64";
    }
    got.erase("timestamp");

    if (got != want) {
        return utils::Stringer("got!=want; got: >>", got, "<<; want: >>", want, "<<");
    }

    auto now = datetime::DateTime::Now();
    auto timestamp_diff_ms = now.MillisSinceEpoch() - got_tokens_timestamp.MillisSinceEpoch();
    if (timestamp_diff_ms > 1000) {
        return utils::Stringer("timestamps differ too much; now: ", now.MillisSinceEpoch(), "; got: ", got_tokens_timestamp.MillisSinceEpoch(), "; diff: ", timestamp_diff_ms);
    }

    return "";
}

TEST_F(TestPsiCash, ModifyLandingPage) {
    PsiCashTester pc;
    // Pass false for test so that we don't get "dev" and "debug" in all the params
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false, false);
    ASSERT_FALSE(err);

    const string key_part = "psicash=";
    const string and_key_part = "&" + key_part;
    URL url_in, url_out;

    //
    // No tokens: no error
    //

    url_in = {"https://asdf.sadf.gf", "", ""};
    auto res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.fragment_, url_in.fragment_);
    ASSERT_THAT(TokenPayloadsMatch(url_out.query_.substr(key_part.length()), R"({"tokens":null})"_json), IsEmpty());

    //
    // Add tokens
    //

    // Some tokens, but no earner token (different code path)
    AuthTokens auth_tokens = {{kSpenderTokenType, {"kSpenderTokenType"}},
                              {kIndicatorTokenType, {"kIndicatorTokenType"}}};
    err = pc.user_data().SetAuthTokens(auth_tokens, false, "");
    ASSERT_FALSE(err);
    url_in = {"https://asdf.sadf.gf", "", ""};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.fragment_, url_in.fragment_);
    ASSERT_THAT(TokenPayloadsMatch(url_out.query_.substr(key_part.length()), R"({"tokens":null})"_json), IsEmpty());

    // All tokens
    auth_tokens = {{kSpenderTokenType, {"kSpenderTokenType"}},
                   {kEarnerTokenType, {"kEarnerTokenType"}},
                   {kIndicatorTokenType, {"kIndicatorTokenType"}}};
    err = pc.user_data().SetAuthTokens(auth_tokens, false, "");
    ASSERT_FALSE(err);
    url_in = {"https://asdf.sadf.gf", "", ""};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.fragment_, url_in.fragment_);
    ASSERT_THAT(TokenPayloadsMatch(url_out.query_.substr(key_part.length()), R"({"tokens":"kEarnerTokenType"})"_json), IsEmpty());

    //
    // No metadata set
    //

    url_in = {"https://asdf.sadf.gf", "", ""};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res) << res.error();
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.fragment_, url_in.fragment_);
    ASSERT_THAT(TokenPayloadsMatch(url_out.query_.substr(key_part.length()), R"({"tokens":"kEarnerTokenType"})"_json), IsEmpty());

    url_in = {"https://asdf.sadf.gf", "gfaf=asdf", ""};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.fragment_, url_in.fragment_);
    ASSERT_THAT(TokenPayloadsMatch(url_out.query_.substr((url_in.query_+and_key_part).length()), R"({"tokens":"kEarnerTokenType"})"_json), IsEmpty());

    url_in = {"https://asdf.sadf.gf/asdfilj/adf", "gfaf=asdf", ""};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.fragment_, url_in.fragment_);
    ASSERT_THAT(TokenPayloadsMatch(url_out.query_.substr((url_in.query_+and_key_part).length()), R"({"tokens":"kEarnerTokenType"})"_json), IsEmpty());

    url_in = {"https://asdf.sadf.gf/asdfilj/adf.html", "gfaf=asdf", ""};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.fragment_, url_in.fragment_);
    ASSERT_THAT(TokenPayloadsMatch(url_out.query_.substr((url_in.query_+and_key_part).length()), R"({"tokens":"kEarnerTokenType"})"_json), IsEmpty());

    url_in = {"https://asdf.sadf.gf/asdfilj/adf.html", "", "regffd"};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_THAT(TokenPayloadsMatch(url_out.query_.substr(key_part.length()), R"({"tokens":"kEarnerTokenType"})"_json), IsEmpty());
    ASSERT_EQ(url_out.fragment_, url_in.fragment_);

    url_in = {"https://asdf.sadf.gf/asdfilj/adf.html", "adfg=asdf&vfjnk=fadjn", "regffd"};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_THAT(TokenPayloadsMatch(url_out.query_.substr((url_in.query_+and_key_part).length()), R"({"tokens":"kEarnerTokenType"})"_json), IsEmpty());
    ASSERT_EQ(url_out.fragment_, url_in.fragment_);

    //
    // With metadata
    //

    err = pc.SetRequestMetadataItem("k", "v");
    ASSERT_FALSE(err);
    url_in = {"https://asdf.sadf.gf", "", ""};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.fragment_, url_in.fragment_);
    ASSERT_THAT(TokenPayloadsMatch(url_out.query_.substr(key_part.length()), R"({"metadata":{"k":"v"},"tokens":"kEarnerTokenType"})"_json), IsEmpty());

    //
    // Errors
    //

    res = pc.ModifyLandingPage("#$%^&");
    ASSERT_FALSE(res);
}

TEST_F(TestPsiCash, GetBuyPsiURL) {
    // Most of the logic for GetBuyPsiURL is in ModifyLandingPage, so we don't need to
    // repeat all of those tests.

    PsiCashTester pc;
    // Pass false for test so that we don't get "dev" and "debug" in all the params
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false, false);
    ASSERT_FALSE(err);

    const string bang_key_part = "!psicash=";
    URL url_out;

    string buy_scheme_host_path = "https://buy.psi.cash/";

    //
    // No tokens: error
    //

    auto res = pc.GetBuyPsiURL();
    ASSERT_FALSE(res);

    //
    // No earner token: error
    //

    // Some tokens, but no earner token (different code path)
    AuthTokens auth_tokens = {{kSpenderTokenType, {"kSpenderTokenType"}},
                              {kIndicatorTokenType, {"kIndicatorTokenType"}}};
    err = pc.user_data().SetAuthTokens(auth_tokens, false, "");
    ASSERT_FALSE(err);
    res = pc.GetBuyPsiURL();
    ASSERT_FALSE(res);

    //
    // With tokens
    //

    auth_tokens = {{kSpenderTokenType, {"kSpenderTokenType"}},
                   {kEarnerTokenType, {"kEarnerTokenType"}},
                   {kIndicatorTokenType, {"kIndicatorTokenType"}}};
    err = pc.user_data().SetAuthTokens(auth_tokens, false, "");
    ASSERT_FALSE(err);
    res = pc.GetBuyPsiURL();
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, buy_scheme_host_path);
    ASSERT_THAT(TokenPayloadsMatch(url_out.fragment_.substr(bang_key_part.length()), R"({"tokens":"kEarnerTokenType"})"_json), IsEmpty());
}

TEST_F(TestPsiCash, GetUserSiteURL) {
    // State that affects the URL: testing/dev flag, user agent, locale, account username.

    for (bool test : {true}) { // When accounts are live, we can use this: for (bool test : {false, true}) {
        for (PsiCash::UserSiteURLType url_type : {PsiCash::UserSiteURLType::AccountSignup, PsiCash::UserSiteURLType::ForgotAccount, PsiCash::UserSiteURLType::AccountManagement}) {
            for (bool webview : {false, true}) {
                for (string locale : {"", "en-US", "zh"}) {
                    PsiCashTester pc;
                    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false, test);
                    ASSERT_FALSE(err);

                    auto res_refresh = pc.RefreshState(false, {"speed-boost"});
                    ASSERT_TRUE(res_refresh) << res_refresh.error();
                    ASSERT_EQ(res_refresh->status, Status::Success);

                    err = pc.SetLocale(locale);
                    ASSERT_FALSE(err);

                    auto url = pc.GetUserSiteURL(url_type, webview);

                    ASSERT_THAT(url, StartsWith("https://"));
                    ASSERT_THAT(url, HasSubstr(TestPsiCash::UserAgent()));

                    if (test) {
                        ASSERT_THAT(url, HasSubstr("dev-"));
                    }
                    else {
                        ASSERT_THAT(url, Not(HasSubstr("dev-")));
                    }

                    if (webview) {
                        ASSERT_THAT(url, HasSubstr("webview"));
                    }
                    else {
                        ASSERT_THAT(url, Not(HasSubstr("webview")));
                    }

                    ASSERT_THAT(url, HasSubstr("locale="+locale));

                    ASSERT_THAT(url, Not(HasSubstr("username=")));

                    // We're not going to log in to set the username, as it makes this test very slow (around a full minute)
                    pc.user_data().SetAccountUsername(TEST_ACCOUNT_UNICODE_USERNAME);
                    url = pc.GetUserSiteURL(url_type, webview);
                    ASSERT_THAT(url, HasSubstr("username=%E1%88%88"));

                    string too_long_username;
                    too_long_username.resize(3000, 'x');
                    pc.user_data().SetAccountUsername(too_long_username);
                    url = pc.GetUserSiteURL(url_type, webview);
                    ASSERT_THAT(url, Not(HasSubstr("username=")));
                }
            }
        }
    }
}

TEST_F(TestPsiCash, GetRewardedActivityData) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false);
    ASSERT_FALSE(err);

    // Error with no tokens
    auto res = pc.GetRewardedActivityData();
    ASSERT_FALSE(res);

    AuthTokens auth_tokens = {{kSpenderTokenType, {"kSpenderTokenType"}},
                              {kEarnerTokenType, {"kEarnerTokenType"}},
                              {kIndicatorTokenType, {"kIndicatorTokenType"}}};
    err = pc.user_data().SetAuthTokens(auth_tokens, false, "");
    ASSERT_FALSE(err);

    res = pc.GetRewardedActivityData();
    ASSERT_TRUE(res);
    ASSERT_EQ(*res, base64::B64Encode(utils::Stringer(R"({"metadata":{"user_agent":")", TestPsiCash::UserAgent(), R"(","v":1},"tokens":"kEarnerTokenType","v":1})")));

    err = pc.SetRequestMetadataItem("k", "v");
    ASSERT_FALSE(err);
    res = pc.GetRewardedActivityData();
    ASSERT_TRUE(res);
    ASSERT_EQ(*res, base64::B64Encode(utils::Stringer(R"({"metadata":{"k":"v","user_agent":")", TestPsiCash::UserAgent(), R"(","v":1},"tokens":"kEarnerTokenType","v":1})")));
}

TEST_F(TestPsiCash, GetDiagnosticInfo) {
    {
        // First do a simple test with test=false
        PsiCashTester pc;
        auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false, false);
        ASSERT_FALSE(err);

        auto want = R"|({
        "balance":0,
        "isAccount":false,
        "isLoggedOutAccount":false,
        "purchasePrices":[],
        "purchases":[],
        "serverTimeDiff":0,
        "test":false,
        "validTokenTypes":[]
        })|"_json;
        auto j = pc.GetDiagnosticInfo();
        ASSERT_EQ(j, want);
    }

    {
        // Then do the full test with test=true
        PsiCashTester pc;
        auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false, true);
        ASSERT_FALSE(err);

        auto want = R"|({
        "balance":0,
        "isAccount":false,
        "isLoggedOutAccount":false,
        "purchasePrices":[],
        "purchases":[],
        "serverTimeDiff":0,
        "test":true,
        "validTokenTypes":[]
        })|"_json;
        auto j = pc.GetDiagnosticInfo();
        ASSERT_EQ(j, want);

        pc.user_data().SetBalance(12345);
        pc.user_data().SetPurchasePrices({{"tc1", "d1", 123}, {"tc2", "d2", 321}});
        pc.user_data().SetPurchases(
                {{"id2", datetime::DateTime(), "tc2", "d2", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt}});
        pc.user_data().SetAuthTokens({{"a", {"a"}}, {"b", {"b"}}, {"c", {"c"}}}, true, "username");
        // pc.user_data().SetServerTimeDiff() // too hard to do reliably
        want = R"|({
        "balance":12345,
        "isAccount":true,
        "isLoggedOutAccount":false,
        "purchasePrices":[{"distinguisher":"d1","price":123,"class":"tc1"},{"distinguisher":"d2","price":321,"class":"tc2"}],
        "purchases":[{"class":"tc2","distinguisher":"d2"}],
        "serverTimeDiff":0,
        "test":true,
        "validTokenTypes":["a","b","c"]
        })|"_json;
        j = pc.GetDiagnosticInfo();
        ASSERT_EQ(j, want);

        pc.user_data().DeleteUserData(/*is_logged_out_account=*/true);
        want = R"|({
        "balance":0,
        "isAccount":true,
        "isLoggedOutAccount":true,
        "purchasePrices":[],
        "purchases":[],
        "serverTimeDiff":0,
        "test":true,
        "validTokenTypes":[]
        })|"_json;
        j = pc.GetDiagnosticInfo();
        ASSERT_EQ(j, want);
    }
}

TEST_F(TestPsiCash, RefreshState) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
    ASSERT_FALSE(err) << err;
    ASSERT_FALSE(pc.HasTokens());

    // Basic NewTracker success
    auto res = pc.RefreshState(false, {"speed-boost"});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success) << (int)res->status;
    ASSERT_FALSE(res->reconnect_required);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_FALSE(pc.AccountUsername());
    ASSERT_TRUE(pc.HasTokens());
    ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));
    ASSERT_GE(pc.GetPurchasePrices().size(), 2);

    // Test with existing tracker
    auto want_tokens = pc.user_data().GetAuthTokens();
    res = pc.RefreshState(false, {"speed-boost"});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    ASSERT_FALSE(res->reconnect_required);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_FALSE(pc.AccountUsername());
    ASSERT_TRUE(pc.HasTokens());
    ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));
    auto speed_boost_purchase_prices = pc.GetPurchasePrices();
    ASSERT_GE(pc.GetPurchasePrices().size(), 2);
    ASSERT_TRUE(AuthTokenSetsEqual(want_tokens, pc.user_data().GetAuthTokens()));

    // Multiple purchase classes
    pc.user_data().Clear();
    res = pc.RefreshState(false, {"speed-boost", TEST_DEBIT_TRANSACTION_CLASS});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    ASSERT_FALSE(res->reconnect_required);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_FALSE(pc.AccountUsername());
    ASSERT_TRUE(pc.HasTokens());
    ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));
    ASSERT_GT(pc.GetPurchasePrices().size(), speed_boost_purchase_prices.size());

    // No purchase classes
    pc.user_data().Clear();
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    ASSERT_FALSE(res->reconnect_required);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_FALSE(pc.AccountUsername());
    ASSERT_TRUE(pc.HasTokens());
    ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));
    ASSERT_EQ(pc.GetPurchasePrices().size(), 0); // we didn't ask for any

    // Purchase classes, then none; verify that previous aren't lost
    pc.user_data().Clear();
    res = pc.RefreshState(false, {"speed-boost"}); // with class
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    ASSERT_FALSE(res->reconnect_required);
    speed_boost_purchase_prices = pc.GetPurchasePrices();
    ASSERT_GT(speed_boost_purchase_prices.size(), 3);
    res = pc.RefreshState(false, {}); // without class
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    ASSERT_EQ(pc.GetPurchasePrices().size(), speed_boost_purchase_prices.size());

    // Balance increase
    pc.user_data().Clear();
    res = pc.RefreshState(false, {"speed-boost"}); // with class
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    ASSERT_FALSE(res->reconnect_required);
    auto starting_balance = pc.Balance();
    err = MAKE_1T_REWARD(pc, 1);
    ASSERT_FALSE(err) << err;
    res = pc.RefreshState(false, {"speed-boost"}); // with class
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    ASSERT_EQ(pc.Balance(), starting_balance + ONE_TRILLION);
}

TEST_F(TestPsiCash, RefreshStateAccount) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
    ASSERT_FALSE(err);

    // Test bad is-account state. This is a local sanity check failure that will occur
    // after the request to the server sees an illegal is-account state. It should
    // result in a local data reset.
    pc.user_data().Clear();
    auto res_refresh = pc.RefreshState(false, {"speed-boost"});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_FALSE(res_refresh->reconnect_required);
    // We're setting "isAccount" with tracker tokens. This is not okay and shouldn't happen.
    pc.user_data().SetIsAccount(true);
    res_refresh = pc.RefreshState(false, {});
    ASSERT_FALSE(res_refresh);

    // Test is-account with no tokens
    pc.user_data().Clear();                 // blow away existing tokens
    pc.user_data().SetIsAccount(true);      // force is-account
    res_refresh = pc.RefreshState(false, {"speed-boost"});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_FALSE(res_refresh->reconnect_required);
    ASSERT_TRUE(pc.IsAccount());  // should still be is-account
    ASSERT_FALSE(pc.HasTokens()); // but no tokens
    ASSERT_EQ(pc.Balance(), 0);
    ASSERT_EQ(pc.GetPurchasePrices().size(), 0); // shouldn't get any, because no valid indicator token

    // Successful login and refresh
    auto res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login) << res_login.error();
    ASSERT_EQ(res_login->status, Status::Success);
    ASSERT_FALSE(res_login->last_tracker_merge);

    res_refresh = pc.RefreshState(false, {"speed-boost"});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_FALSE(res_refresh->reconnect_required);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.HasTokens());
    ASSERT_TRUE(pc.AccountUsername());
    ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_ONE_USERNAME);
    ASSERT_GT(pc.Balance(), 0); // Our test accounts don't have zero balance
    ASSERT_GT(pc.GetPurchasePrices().size(), 0);

    // In order to test the username retrieval, we'll set it to an incorrect value
    // and then refresh to overwrite it.
    pc.user_data().SetAccountUsername("not-real-username");
    ASSERT_TRUE(pc.AccountUsername());
    ASSERT_EQ(*pc.AccountUsername(), "not-real-username");
    res_refresh = pc.RefreshState(false, {"speed-boost"});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_TRUE(pc.AccountUsername());
    ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_ONE_USERNAME);

    // Log out and try to refresh
    auto res_logout = pc.AccountLogout();
    ASSERT_TRUE(res_logout);
    ASSERT_TRUE(pc.IsAccount());               // should still be is-account
    ASSERT_FALSE(pc.HasTokens());
    res_refresh = pc.RefreshState(false, {"speed-boost"});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_FALSE(res_refresh->reconnect_required);
    ASSERT_TRUE(pc.IsAccount());        // should still be is-account
    ASSERT_FALSE(pc.HasTokens());       // but no tokens
    ASSERT_FALSE(pc.AccountUsername()); // and no username
    ASSERT_EQ(pc.Balance(), 0);
    ASSERT_EQ(pc.GetPurchasePrices().size(), 0); // shouldn't get any, because no valid indicator token

    // Force invalid tokens, forcing logged-out state
    if (pc.MutatorsEnabled()) {
        // Successful login and refresh
        auto res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
        ASSERT_TRUE(res_login);
        ASSERT_EQ(res_login->status, Status::Success);
        ASSERT_FALSE(res_login->last_tracker_merge);

        res_refresh = pc.RefreshState(false, {"speed-boost"});
        ASSERT_TRUE(res_refresh) << res_refresh.error();
        ASSERT_EQ(res_refresh->status, Status::Success);
        ASSERT_FALSE(res_refresh->reconnect_required);
        ASSERT_TRUE(pc.IsAccount());
        ASSERT_TRUE(pc.HasTokens());
        ASSERT_TRUE(pc.AccountUsername());
        ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_ONE_USERNAME);
        ASSERT_GT(pc.Balance(), 0); // Our test accounts don't have zero balance
        ASSERT_GT(pc.GetPurchasePrices().size(), 0);

        // Try again with invalid tokens
        pc.SetRequestMutators({"InvalidTokens"});
        res_refresh = pc.RefreshState(false, {"speed-boost"});
        ASSERT_TRUE(res_refresh) << res_refresh.error();
        ASSERT_EQ(res_refresh->status, Status::Success);
        ASSERT_FALSE(res_refresh->reconnect_required);
        ASSERT_TRUE(pc.IsAccount());        // should still be is-account
        ASSERT_FALSE(pc.HasTokens());       // but no tokens
        ASSERT_FALSE(pc.AccountUsername()); // and no username
        ASSERT_EQ(pc.Balance(), 0);
        ASSERT_EQ(pc.GetPurchasePrices().size(), 0); // shouldn't get any, because no valid indicator token
    }

    // Test Account token expiry while Authorization active, requiring tunnel reconnect
    if (pc.MutatorsEnabled()) {
        // Force reset to get rid of any other purchases
        auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, true);
        ASSERT_FALSE(err);

        // Successful login and refresh
        auto res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
        ASSERT_TRUE(res_login);
        ASSERT_EQ(res_login->status, Status::Success);
        ASSERT_FALSE(res_login->last_tracker_merge);

        res_refresh = pc.RefreshState(false, {});
        ASSERT_TRUE(res_refresh) << res_refresh.error();
        ASSERT_EQ(res_refresh->status, Status::Success);
        ASSERT_FALSE(res_refresh->reconnect_required);
        ASSERT_TRUE(pc.IsAccount());
        ASSERT_TRUE(pc.HasTokens());

        err = MAKE_1T_REWARD(pc, 1);
        ASSERT_FALSE(err) << err;

        // Make a purchase that produces an authorization
        auto purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_WITH_AUTHORIZATION_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MINUTE_DISTINGUISHER, ONE_TRILLION);
        ASSERT_TRUE(purchase_result);
        ASSERT_EQ(purchase_result->status, Status::Success);
        ASSERT_TRUE(purchase_result->purchase->authorization);

        // Force our tokens to be invalid, emulating expiry
        pc.SetRequestMutators({"InvalidTokens"});
        res_refresh = pc.RefreshState(false, {});
        ASSERT_TRUE(res_refresh) << res_refresh.error();
        ASSERT_EQ(res_refresh->status, Status::Success);
        ASSERT_TRUE(res_refresh->reconnect_required); // need to remove the Authorization from the tunnel
        ASSERT_TRUE(pc.IsAccount());        // should still be is-account
        ASSERT_FALSE(pc.HasTokens());       // but no tokens
    }
}

TEST_F(TestPsiCash, RefreshStateRetrievePurchases) {
    // We'll go through a set of tests twice -- once with a tracker, once with an account

    for (auto i = 0; i < 2; i++) {
        Purchases expected_purchases;

        PsiCashTester pc;
        auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
        ASSERT_FALSE(err);

        if (i == 0) {
            // Get a tracker
            auto res_refresh = pc.RefreshState(false, {"speed-boost"});
            ASSERT_TRUE(res_refresh) << res_refresh.error();
            ASSERT_EQ(res_refresh->status, Status::Success);
            ASSERT_FALSE(res_refresh->reconnect_required);
        }
        else {
            // Log in as an account
            auto res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
            ASSERT_TRUE(res_login);
            ASSERT_EQ(res_login->status, Status::Success);
            auto res_refresh = pc.RefreshState(false, {});
            ASSERT_TRUE(res_refresh);
            ASSERT_EQ(res_refresh->status, Status::Success);
            ASSERT_TRUE(pc.IsAccount());
            // Should have reset everything
            ASSERT_EQ(pc.GetPurchases().size(), 0);
            ASSERT_EQ(pc.user_data().GetLastTransactionID(), "");
        }

        err = MAKE_1T_REWARD(pc, 3);
        ASSERT_FALSE(err) << err;

        // We have no purchases yet
        ASSERT_EQ(pc.GetPurchases().size(), 0);

        // Make a couple sufficiently long-lived purchases
        auto purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, ONE_TRILLION);
        ASSERT_TRUE(purchase_result);
        ASSERT_EQ(purchase_result->status, Status::Success) << static_cast<int>(purchase_result->status);
        expected_purchases.push_back(*purchase_result->purchase);
        ASSERT_EQ(pc.GetPurchases(), expected_purchases) << (pc.IsAccount() ? "account: " : "tracker: ") << pc.GetPurchases().size() << " vs " << expected_purchases.size() << ": " << json(pc.GetPurchases()) << " vs " << json(expected_purchases);
        ASSERT_EQ(pc.user_data().GetLastTransactionID(), purchase_result->purchase->id);

        purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MINUTE_DISTINGUISHER, ONE_TRILLION); // if this lifetime is too long for other tests, we should create an 11-second distinguisher to use (kind of thing)
        ASSERT_TRUE(purchase_result);
        ASSERT_EQ(purchase_result->status, Status::Success) << static_cast<int>(purchase_result->status);
        expected_purchases.push_back(*purchase_result->purchase);
        ASSERT_EQ(pc.GetPurchases(), expected_purchases);
        ASSERT_EQ(pc.user_data().GetLastTransactionID(), purchase_result->purchase->id);

        // "Lose" our purchases, but not the LastTransactionID, so no retrieval will occur
        pc.user_data().SetPurchases({});
        ASSERT_EQ(pc.GetPurchases().size(), 0);
        ASSERT_EQ(pc.user_data().GetLastTransactionID(), expected_purchases.back().id);

        // Refresh
        auto res_refresh = pc.RefreshState(false, {"speed-boost"});
        ASSERT_TRUE(res_refresh) << res_refresh.error();
        ASSERT_EQ(res_refresh->status, Status::Success);

        // We didn't get the purchases back
        ASSERT_EQ(pc.GetPurchases().size(), 0);
        ASSERT_FALSE(res_refresh->reconnect_required);

        // Clear the LastTransactionID value and try again
        pc.user_data().SetLastTransactionID("");
        res_refresh = pc.RefreshState(false, {"speed-boost"});
        ASSERT_TRUE(res_refresh) << res_refresh.error();
        ASSERT_EQ(res_refresh->status, Status::Success);

        // We got our purchases back
        ASSERT_EQ(pc.GetPurchases(), expected_purchases) << pc.GetPurchases().size() << " vs " << expected_purchases.size() << ": " << json(pc.GetPurchases()) << " vs " << json(expected_purchases);
        ASSERT_EQ(pc.user_data().GetLastTransactionID(), expected_purchases.back().id);
        ASSERT_FALSE(res_refresh->reconnect_required); // Not speed-boost, so no reconnect required

        // Clear the purchases and set the LastTransactionID to garbage, which will also trigger a full retrieval
        pc.user_data().SetPurchases({});
        pc.user_data().SetLastTransactionID("");
        ASSERT_EQ(pc.GetPurchases().size(), 0);
        ASSERT_EQ(pc.user_data().GetLastTransactionID(), ""s);
        res_refresh = pc.RefreshState(false, {"speed-boost"});
        ASSERT_TRUE(res_refresh) << res_refresh.error();
        ASSERT_EQ(res_refresh->status, Status::Success);

        // We got our purchases back
        ASSERT_EQ(pc.GetPurchases(), expected_purchases);
        ASSERT_EQ(pc.user_data().GetLastTransactionID(), expected_purchases.back().id);
        ASSERT_FALSE(res_refresh->reconnect_required); // Not speed-boost, so no reconnect required

        // Lose the second purchase, but not the first
        pc.user_data().SetPurchases({});
        pc.user_data().SetLastTransactionID("");
        pc.user_data().AddPurchase(expected_purchases[0]);
        ASSERT_EQ(pc.GetPurchases().size(), 1);
        ASSERT_EQ(pc.user_data().GetLastTransactionID(), expected_purchases[0].id);
        res_refresh = pc.RefreshState(false, {"speed-boost"});
        ASSERT_TRUE(res_refresh) << res_refresh.error();
        ASSERT_EQ(res_refresh->status, Status::Success);

        // We got our purchases back
        ASSERT_EQ(pc.GetPurchases(), expected_purchases);
        ASSERT_EQ(pc.user_data().GetLastTransactionID(), expected_purchases.back().id);
        ASSERT_FALSE(res_refresh->reconnect_required); // Not speed-boost, so no reconnect required

        // Make a purchase that produces an authorization, so we can test the reconnect-required flag
        purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_WITH_AUTHORIZATION_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MINUTE_DISTINGUISHER, ONE_TRILLION);
        ASSERT_TRUE(purchase_result);
        ASSERT_EQ(purchase_result->status, Status::Success) << static_cast<int>(purchase_result->status);
        ASSERT_TRUE(purchase_result->purchase->authorization);
        expected_purchases.push_back(*purchase_result->purchase);
        ASSERT_EQ(pc.GetPurchases(), expected_purchases);
        ASSERT_EQ(pc.user_data().GetLastTransactionID(), purchase_result->purchase->id);

        // Refresh state, but won't have retrieved anything.
        res_refresh = pc.RefreshState(false, {"speed-boost"});
        ASSERT_TRUE(res_refresh) << res_refresh.error();
        ASSERT_EQ(res_refresh->status, Status::Success);
        ASSERT_FALSE(res_refresh->reconnect_required);

        // Clear the purchases and set the LastTransactionID to garbage, which will also trigger a full retrieval
        pc.user_data().SetPurchases({});
        pc.user_data().SetLastTransactionID("");
        ASSERT_EQ(pc.GetPurchases().size(), 0);
        ASSERT_EQ(pc.user_data().GetLastTransactionID(), ""s);
        res_refresh = pc.RefreshState(false, {"speed-boost"});
        ASSERT_TRUE(res_refresh) << res_refresh.error();
        ASSERT_EQ(res_refresh->status, Status::Success);
        // Should have got our Speed Boost, so reconnect is required
        ASSERT_TRUE(res_refresh->reconnect_required);

        // Account-only tests
        if (i == 1) {
            ASSERT_GT(expected_purchases.size(), 0);
            ASSERT_EQ(pc.GetPurchases(), expected_purchases);

            // Logout
            auto res_logout = pc.AccountLogout();
            ASSERT_TRUE(res_logout);
            ASSERT_TRUE(pc.IsAccount()); // should still be is-account
            ASSERT_FALSE(pc.HasTokens());
            ASSERT_EQ(pc.GetPurchases().size(), 0);
            // Log back in
            auto res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
            ASSERT_TRUE(res_login);
            ASSERT_EQ(res_login->status, Status::Success);
            // Retrieve our purchases
            auto res_refresh = pc.RefreshState(false, {});
            ASSERT_TRUE(res_refresh);
            ASSERT_EQ(res_refresh->status, Status::Success);
            ASSERT_TRUE(pc.IsAccount());
            ASSERT_EQ(pc.GetPurchases(), expected_purchases) << pc.GetPurchases().size() << " vs " << expected_purchases.size() << ": " << json(pc.GetPurchases()) << " vs " << json(expected_purchases);
        }
    }
}

TEST_F(TestPsiCash, RefreshStateLocalOnly) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
    ASSERT_FALSE(err) << err;
    ASSERT_FALSE(pc.HasTokens());

    const bool REMOTE = false, LOCAL_ONLY = true;

    auto expire_tokens_fn = [&] {
        auto past = datetime::DateTime::Now().Sub(datetime::Duration(10000));
        auto auth_tokens = pc.user_data().GetAuthTokens();
        for (auto& at : auth_tokens) {
            at.second.server_time_expiry = past;
        }
        auto err = pc.user_data().SetAuthTokens(auth_tokens, true, *pc.AccountUsername());
        ASSERT_FALSE(err);
    };

    // If called local-only before there's any tokens, it does nothing.
    auto res_refresh = pc.RefreshState(LOCAL_ONLY, {});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_FALSE(res_refresh->reconnect_required);
    ASSERT_FALSE(pc.HasTokens());
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_FALSE(pc.AccountUsername());

    // Basic NewTracker success
    res_refresh = pc.RefreshState(REMOTE, {"speed-boost"});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success) << (int)res_refresh->status;
    ASSERT_FALSE(res_refresh->reconnect_required);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_FALSE(pc.AccountUsername());
    ASSERT_TRUE(pc.HasTokens());
    ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));
    ASSERT_GE(pc.GetPurchasePrices().size(), 2);

    // Test with tracker
    res_refresh = pc.RefreshState(LOCAL_ONLY, {});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_FALSE(res_refresh->reconnect_required);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_FALSE(pc.AccountUsername());
    ASSERT_TRUE(pc.HasTokens());
    ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));
    auto speed_boost_purchase_prices = pc.GetPurchasePrices();
    ASSERT_GE(pc.GetPurchasePrices().size(), 2);

    // Get account tokens, to expire them
    auto res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login);
    ASSERT_EQ(res_login->status, Status::Success);
    res_refresh = pc.RefreshState(false, {});
    ASSERT_TRUE(res_refresh);
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.HasTokens());
    ASSERT_TRUE(pc.AccountUsername());
    ASSERT_THAT(*pc.AccountUsername(), Not(IsEmpty()));
    res_refresh = pc.RefreshState(REMOTE, {"speed-boost"});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success) << (int)res_refresh->status;

    res_refresh = pc.RefreshState(true, {"speed-boost"}); // local-only
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success);

    // Force the account tokens to be expired
    expire_tokens_fn();
    // Refresh local
    res_refresh = pc.RefreshState(LOCAL_ONLY, {});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_FALSE(res_refresh->reconnect_required);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_FALSE(pc.HasTokens());       // should be gone
    ASSERT_FALSE(pc.AccountUsername()); // should be gone

    // Log in again, get authorization
    res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login);
    ASSERT_EQ(res_login->status, Status::Success);
    res_refresh = pc.RefreshState(false, {});
    ASSERT_TRUE(res_refresh);
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.HasTokens());
    ASSERT_TRUE(pc.AccountUsername());
    ASSERT_THAT(*pc.AccountUsername(), Not(IsEmpty()));
    res_refresh = pc.RefreshState(REMOTE, {"speed-boost"});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success) << (int)res_refresh->status;
    // Get some credit
    err = MAKE_1T_REWARD(pc, 1);
    ASSERT_FALSE(err) << err;
    // Buy something that gives an authorization
    auto purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_WITH_AUTHORIZATION_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MINUTE_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::Success);
    ASSERT_TRUE(purchase_result->purchase->authorization);
    // Force tokens to be expired
    expire_tokens_fn();
    // Refresh local
    res_refresh = pc.RefreshState(LOCAL_ONLY, {});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_TRUE(res_refresh->reconnect_required); // now true
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_FALSE(pc.HasTokens());       // should be gone
    ASSERT_FALSE(pc.AccountUsername()); // should be gone
}

TEST_F(TestPsiCash, RefreshStateMutators) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
    ASSERT_FALSE(err) << err;

    if (!pc.MutatorsEnabled()) {
      // Can't proceed with these tests
      return;
    }

    // Tracker with invalid tokens
    pc.user_data().Clear();
    auto res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    auto prev_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(prev_tokens.size(), 3);
    err = pc.user_data().SetBalance(12345); // force a nonzero balance
    ASSERT_FALSE(err);
    ASSERT_GT(pc.Balance(), 0);
    // We have tokens; force the server to consider them invalid
    pc.SetRequestMutators({"InvalidTokens"});
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    // We should have brand new tokens now.
    auto next_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(next_tokens.size(), 3);
    ASSERT_FALSE(AuthTokenSetsEqual(prev_tokens, next_tokens));
    // And a reset balance
    ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));

    // Account with invalid tokens
    pc.user_data().Clear();
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    ASSERT_GE(pc.user_data().GetAuthTokens().size(), 3);
    // We're setting "isAccount" with tracker tokens. This is not okay, but the server is
    // going to blindly consider them invalid anyway.
    pc.user_data().SetIsAccount(true);
    // We have tokens; force the server to consider them invalid
    pc.SetRequestMutators({"InvalidTokens"});
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    // Accounts won't get new tokens by refreshing, so now we should have none
    ASSERT_GE(pc.user_data().GetAuthTokens().size(), 0);

    // Tracker with always-invalid tokens (impossible to get valid ones)
    pc.user_data().Clear();
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    ASSERT_GE(pc.user_data().GetAuthTokens().size(), 3);
    // We have tokens; force the server to consider them invalid
    pc.SetRequestMutators({
            "InvalidTokens", // RefreshState
            "",              // NewTracker
            "InvalidTokens", // RefreshState
    });
    res = pc.RefreshState(false, {});
    // Should have failed utterly
    ASSERT_FALSE(res) << static_cast<int>(res->status);
    // Should be no tokens
    ASSERT_EQ(pc.user_data().GetAuthTokens().size(), 0);
    ASSERT_EQ(pc.Balance(), 0);

    // No server response for NewTracker
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // First request is NewTracker, sleep for 11 secs
    pc.SetRequestMutators({"Timeout:11"});
    res = pc.RefreshState(false, {});
    ASSERT_FALSE(res) << static_cast<int>(res->status);
    // Should be no tokens
    ASSERT_EQ(pc.user_data().GetAuthTokens().size(), 0);
    ASSERT_EQ(pc.Balance(), 0);

    // No server response for RefreshState
    pc.user_data().Clear();
    // Do an initial request to get Tracker tokens
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    auto auth_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(auth_tokens.size(), 3);
    // Because we have tokens the first request will be RefreshState; sleep for 11 secs
    pc.SetRequestMutators({"Timeout:11"});
    res = pc.RefreshState(false, {});
    ASSERT_FALSE(res) << static_cast<int>(res->status);
    // Tokens should be unchanged
    ASSERT_TRUE(AuthTokenSetsEqual(auth_tokens, pc.user_data().GetAuthTokens()));

    // NewTracker response with no data
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // First request is NewTracker; force an empty response
    pc.SetRequestMutators({"Response:code=200,body=none"});
    res = pc.RefreshState(false, {});
    ASSERT_FALSE(res) << static_cast<int>(res->status);
    // Should be no tokens
    ASSERT_EQ(pc.user_data().GetAuthTokens().size(), 0);
    ASSERT_EQ(pc.Balance(), 0);

    // Empty server response for RefreshState
    pc.user_data().Clear();
    // Do an initial request to get Tracker tokens
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    auth_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(auth_tokens.size(), 3);
    // Because we have tokens the first request will be RefreshState; force an empty response
    pc.SetRequestMutators({"Response:code=200,body=none"});
    res = pc.RefreshState(false, {});
    ASSERT_FALSE(res) << static_cast<int>(res->status);
    // Tokens should be unchanged
    ASSERT_TRUE(AuthTokenSetsEqual(auth_tokens, pc.user_data().GetAuthTokens()));

    // NewTracker response with bad JSON
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // First request is NewTracker; force a response with bad JSON
    pc.SetRequestMutators({"BadJSON:200"});
    res = pc.RefreshState(false, {});
    ASSERT_FALSE(res) << static_cast<int>(res->status);
    // Should be no tokens
    ASSERT_EQ(pc.user_data().GetAuthTokens().size(), 0);
    ASSERT_EQ(pc.Balance(), 0);

    // RefreshState response with bad JSON
    pc.user_data().Clear();
    // Do an initial request to get Tracker tokens
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    auth_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(auth_tokens.size(), 3);
    // Because we have tokens the first request will be RefreshState; force a response with bad JSON
    pc.SetRequestMutators({"BadJSON:200"});
    res = pc.RefreshState(false, {});
    ASSERT_FALSE(res) << static_cast<int>(res->status);
    // Tokens should be unchanged
    ASSERT_TRUE(AuthTokenSetsEqual(auth_tokens, pc.user_data().GetAuthTokens()));

    // 1 NewTracker response is 500 (retry succeeds)
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // First request is NewTracker;
    pc.SetRequestMutators({"Response:code=500"});
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success) << static_cast<int>(res->status);
    ASSERT_GE(pc.user_data().GetAuthTokens().size(), 3);

    // 2 NewTracker responses are 500 (retry succeeds)
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // First request is NewTracker
    pc.SetRequestMutators({"Response:code=500", "Response:code=500"});
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success) << static_cast<int>(res->status);
    ASSERT_GE(pc.user_data().GetAuthTokens().size(), 3);

    // 3 NewTracker responses are 500 (retry fails)
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // First request is NewTracker
    pc.SetRequestMutators({"Response:code=500", "Response:code=500", "Response:code=500"});
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::ServerError) << static_cast<int>(res->status);
    // Should be no tokens
    ASSERT_EQ(pc.user_data().GetAuthTokens().size(), 0);
    ASSERT_EQ(pc.Balance(), 0);

    // 1 RefreshState response is 500 (retry succeeds)
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // Do an initial request to get Tracker tokens
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    auth_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(auth_tokens.size(), 3);
    // Because we have tokens the first request will be RefreshState
    pc.SetRequestMutators({"Response:code=500"});
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success) << static_cast<int>(res->status);
    ASSERT_GE(pc.user_data().GetAuthTokens().size(), 3);

    // 2 RefreshState responses are 500 (retry succeeds)
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // Do an initial request to get Tracker tokens
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    auth_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(auth_tokens.size(), 3);
    // Because we have tokens the first request will be RefreshState
    pc.SetRequestMutators({"Response:code=500", "Response:code=500"});
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success) << static_cast<int>(res->status);
    ASSERT_GE(pc.user_data().GetAuthTokens().size(), 3);

    // 3 RefreshState responses are 500 (retry fails)
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // Do an initial request to get Tracker tokens
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    auth_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(auth_tokens.size(), 3);
    // Because we have tokens the first request will be RefreshState
    pc.SetRequestMutators({"Response:code=500", "Response:code=500", "Response:code=500"});
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::ServerError) << static_cast<int>(res->status);
    // Tokens should be unchanged
    ASSERT_TRUE(AuthTokenSetsEqual(auth_tokens, pc.user_data().GetAuthTokens()));

    // RefreshState response with status code indicating invalid tokens
    pc.user_data().Clear();
    // Do an initial request to get Tracker tokens
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    auth_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(auth_tokens.size(), 3);
    // Because we have tokens the first request will be RefreshState
    pc.SetRequestMutators({"Response:code=401"});
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::InvalidTokens) << static_cast<int>(res->status);
    // UserData should be cleared
    ASSERT_EQ(pc.user_data().GetAuthTokens().size(), 0);

    // NewTracker response with unknown status code
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // First request is NewTracker
    pc.SetRequestMutators({"Response:code=666"});
    res = pc.RefreshState(false, {});
    ASSERT_FALSE(res) << static_cast<int>(res->status);
    // Should be no tokens
    ASSERT_EQ(pc.user_data().GetAuthTokens().size(), 0);
    ASSERT_EQ(pc.Balance(), 0);

    // RefreshState response with unknown status code
    pc.user_data().Clear();
    // Do an initial request to get Tracker tokens
    res = pc.RefreshState(false, {});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(res->status, Status::Success);
    auth_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(auth_tokens.size(), 3);
    // Because we have tokens the first request will be RefreshState
    pc.SetRequestMutators({"Response:code=666"});
    res = pc.RefreshState(false, {});
    ASSERT_FALSE(res) << static_cast<int>(res->status);
    // Tokens should be unchanged
    ASSERT_TRUE(AuthTokenSetsEqual(auth_tokens, pc.user_data().GetAuthTokens()));
}

TEST_F(TestPsiCash, RefreshStateOffline) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
    ASSERT_FALSE(err) << err;

    bool request_attempted = false;
    const MakeHTTPRequestFn noHTTPRequester = [&request_attempted](const HTTPParams&) -> HTTPResult {
        request_attempted = true;
        return HTTPResult();
    };
    const MakeHTTPRequestFn errorHTTPRequester = [&request_attempted](const HTTPParams&) -> HTTPResult {
        auto res = HTTPResult();
        res.code = HTTPResult::RECOVERABLE_ERROR;
        res.error = "test";
        return res;
    };


    pc.SetHTTPRequestFn(noHTTPRequester);

}

TEST_F(TestPsiCash, NewExpiringPurchase) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
    ASSERT_FALSE(err) << err;

    // Simple success
    pc.user_data().Clear();
    auto refresh_result = pc.RefreshState(false, {});
    ASSERT_TRUE(refresh_result) << refresh_result.error();
    ASSERT_EQ(refresh_result->status, Status::Success);
    auto initial_balance = pc.Balance();
    err = MAKE_1T_REWARD(pc, 1);
    ASSERT_FALSE(err) << err;
    refresh_result = pc.RefreshState(false, {});
    ASSERT_TRUE(refresh_result) << refresh_result.error();
    ASSERT_EQ(refresh_result->status, Status::Success);
    ASSERT_EQ(pc.Balance(), initial_balance + ONE_TRILLION);
    // Note: this puchase will be valid for 1 second
    auto purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_SECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result) << purchase_result.error();
    ASSERT_EQ(purchase_result->status, Status::Success);
    ASSERT_TRUE(purchase_result->purchase);
    ASSERT_GT(purchase_result->purchase->id.size(), 0);
    ASSERT_EQ(purchase_result->purchase->transaction_class, TEST_DEBIT_TRANSACTION_CLASS);
    ASSERT_EQ(purchase_result->purchase->distinguisher, TEST_ONE_TRILLION_ONE_SECOND_DISTINGUISHER);
    ASSERT_FALSE(purchase_result->purchase->authorization); // our test purchase doesn't produce an authorization
    ASSERT_TRUE(purchase_result->purchase->server_time_expiry);
    ASSERT_TRUE(purchase_result->purchase->local_time_expiry);
    auto local_now = datetime::DateTime::Now();
    ASSERT_NEAR(purchase_result->purchase->local_time_expiry->MillisSinceEpoch(), local_now.MillisSinceEpoch(), 5000);
    ASSERT_NEAR(purchase_result->purchase->server_time_expiry->MillisSinceEpoch(), local_now.MillisSinceEpoch(), 5000) << "Try resyncing your local clock";
    ASSERT_NEAR(purchase_result->purchase->server_time_created.MillisSinceEpoch(), local_now.MillisSinceEpoch(), 5000) << "Try resyncing your local clock";
    // Check UserData -- purchase should still be valid
    ASSERT_EQ(pc.user_data().GetLastTransactionID(), purchase_result->purchase->id);
    auto purchases = pc.GetPurchases();
    ASSERT_EQ(purchases.size(), 1);
    ASSERT_EQ(purchases[0], purchase_result->purchase);
    purchases = pc.ActivePurchases();
    ASSERT_EQ(purchases.size(), 1);
    ASSERT_EQ(purchases[0], purchase_result->purchase);
    auto purchase_opt = pc.NextExpiringPurchase();
    ASSERT_TRUE(purchase_opt);
    ASSERT_EQ(*purchase_opt, purchase_result->purchase);
    auto expire_result = pc.ExpirePurchases();
    ASSERT_TRUE(expire_result);
    ASSERT_EQ(expire_result->size(), 0);
    purchases = pc.GetPurchases();
    ASSERT_EQ(purchases.size(), 1);
    // Sleep long enough for purchase to expire, then check again
    this_thread::sleep_for(chrono::milliseconds(2000));
    purchases = pc.GetPurchases();
    ASSERT_EQ(purchases.size(), 1);
    ASSERT_EQ(purchases[0], purchase_result->purchase);
    purchases = pc.ActivePurchases();
    ASSERT_EQ(purchases.size(), 0);
    purchase_opt = pc.NextExpiringPurchase();
    ASSERT_TRUE(purchase_opt);
    ASSERT_EQ(*purchase_opt, purchase_result->purchase);
    expire_result = pc.ExpirePurchases();
    ASSERT_TRUE(expire_result);
    ASSERT_EQ(expire_result->size(), 1);
    ASSERT_EQ(expire_result->at(0), purchase_result->purchase);
    purchases = pc.GetPurchases();
    ASSERT_EQ(purchases.size(), 0);
    purchase_opt = pc.NextExpiringPurchase();
    ASSERT_FALSE(purchase_opt);

    // Multiple purchases
    initial_balance = pc.Balance();
    err = MAKE_1T_REWARD(pc, 3);
    ASSERT_FALSE(err) << err;
    refresh_result = pc.RefreshState(false, {});
    ASSERT_TRUE(refresh_result) << refresh_result.error();
    ASSERT_EQ(refresh_result->status, Status::Success);
    ASSERT_EQ(pc.Balance(), initial_balance + 3*ONE_TRILLION);
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::Success) << static_cast<int>(purchase_result->status);
    ASSERT_EQ(pc.Balance(), initial_balance + 2*ONE_TRILLION);
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::Success) << static_cast<int>(purchase_result->status);
    ASSERT_EQ(pc.Balance(), initial_balance + 1*ONE_TRILLION);
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::Success);
    ASSERT_EQ(pc.Balance(), initial_balance);
    purchases = pc.GetPurchases();
    ASSERT_EQ(purchases.size(), 3);
    // Sleep long enough for the short purchases to expire (but not so long that the long one will)
    this_thread::sleep_for(chrono::milliseconds(5000));
    purchases = pc.GetPurchases();
    ASSERT_EQ(purchases.size(), 3);
    purchase_opt = pc.NextExpiringPurchase();
    ASSERT_TRUE(purchase_opt);
    // This might be brittle due to server-client clock differences (if so, just remove it)
    ASSERT_EQ(purchase_opt->distinguisher, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER);
    purchases = pc.ActivePurchases();
    ASSERT_EQ(purchases.size(), 1);
    ASSERT_EQ(purchases[0].distinguisher, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER);
    expire_result = pc.ExpirePurchases();
    ASSERT_TRUE(expire_result);
    ASSERT_EQ(expire_result->size(), 2);
    purchases = pc.GetPurchases();
    ASSERT_EQ(purchases.size(), 1);
    ASSERT_EQ(purchases[0].distinguisher, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER);
    purchase_opt = pc.NextExpiringPurchase();
    ASSERT_TRUE(purchase_opt);
    ASSERT_EQ(purchase_opt->distinguisher, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER);
    // Sleep long enough for all purchases to expire; might be brittle depending on clock skew
    this_thread::sleep_for(chrono::milliseconds(10000));
    purchases = pc.GetPurchases();
    ASSERT_EQ(purchases.size(), 1);
    purchase_opt = pc.NextExpiringPurchase();
    ASSERT_TRUE(purchase_opt);
    ASSERT_EQ(purchase_opt->distinguisher, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER);
    purchases = pc.ActivePurchases();
    ASSERT_EQ(purchases.size(), 0);
    expire_result = pc.ExpirePurchases();
    ASSERT_TRUE(expire_result);
    ASSERT_EQ(expire_result->size(), 1);
    ASSERT_EQ(expire_result->at(0).distinguisher, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER);
    purchases = pc.GetPurchases();
    ASSERT_EQ(purchases.size(), 0);
    expire_result = pc.ExpirePurchases();
    ASSERT_TRUE(expire_result);
    ASSERT_EQ(expire_result->size(), 0);

    // Falure: existing transaction
    err = MAKE_1T_REWARD(pc, 2);
    ASSERT_FALSE(err) << err;
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::Success) << static_cast<int>(purchase_result->status);
    // Same again
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::ExistingTransaction) << static_cast<int>(purchase_result->status);

    // Failure: insufficient balance
    pc.user_data().Clear();
    refresh_result = pc.RefreshState(false, {});
    ASSERT_TRUE(refresh_result) << refresh_result.error();
    ASSERT_EQ(refresh_result->status, Status::Success);
    ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));
    // We have no credit for this
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::InsufficientBalance);

    // Failure: transaction amount mismatch
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, 12345); // not correct price
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::TransactionAmountMismatch) << static_cast<int>(purchase_result->status);

    // Failure: transaction type not found
    purchase_result = pc.NewExpiringPurchase("invalid-class", TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result) << purchase_result.error();
    ASSERT_EQ(purchase_result->status, Status::TransactionTypeNotFound) << static_cast<int>(purchase_result->status);
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, "invalid-distinguisher", ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::TransactionTypeNotFound) << static_cast<int>(purchase_result->status);

    // Using an account
    auto res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login);
    ASSERT_EQ(res_login->status, Status::Success);
    refresh_result = pc.RefreshState(false, {});
    ASSERT_TRUE(refresh_result);
    ASSERT_EQ(refresh_result->status, Status::Success);
    ASSERT_TRUE(pc.IsAccount());
    initial_balance = pc.Balance();
    err = MAKE_1T_REWARD(pc, 1);
    ASSERT_FALSE(err) << err;
    refresh_result = pc.RefreshState(false, {});
    ASSERT_TRUE(refresh_result);
    ASSERT_EQ(refresh_result->status, Status::Success);
    ASSERT_EQ(pc.Balance(), initial_balance + ONE_TRILLION);
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_SECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::Success);
    ASSERT_EQ(pc.Balance(), initial_balance);
}

TEST_F(TestPsiCash, NewExpiringPurchasePauserCommitBug) {
    // Bug test: When a kHTTPStatusTooManyRequests response (or any non-success, but
    // especially that one) was received, the updated balance received in the response
    // would be written to the datastore, but the WritePauser would not be committed, so
    // the change would be lost and the UI wouldn't update until a RefreshState request
    // was made.

    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
    ASSERT_FALSE(err);
    auto res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login);
    ASSERT_EQ(res_login->status, Status::Success);
    auto refresh_result = pc.RefreshState(false, {});
    ASSERT_TRUE(refresh_result);
    ASSERT_EQ(refresh_result->status, Status::Success);
    ASSERT_TRUE(pc.IsAccount());
    auto balance = pc.Balance();
    err = MAKE_1T_REWARD(pc, 2); // make sure we have enough balance for two purchases
    ASSERT_FALSE(err) << err;
    refresh_result = pc.RefreshState(false, {});
    ASSERT_TRUE(refresh_result);
    ASSERT_EQ(refresh_result->status, Status::Success);
    ASSERT_EQ(pc.Balance(), balance + ONE_TRILLION + ONE_TRILLION);

    // Make a purchase that's valid long enough for us to have a conflict.
    balance = pc.Balance();
    auto purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result) << purchase_result.error();
    ASSERT_EQ(purchase_result->status, Status::Success);
    ASSERT_EQ(pc.Balance(), balance - ONE_TRILLION);

    // Mess with the balance so we can see that it gets updated by the 429 purchase attempt
    balance = pc.Balance();
    pc.user_data().SetBalance(1);
    ASSERT_EQ(pc.Balance(), 1);

    // Now make a conflicting purchase
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::ExistingTransaction);

    // Make sure the balance was updated
    ASSERT_EQ(pc.Balance(), balance);
}

TEST_F(TestPsiCash, NewExpiringPurchaseMutators) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
    ASSERT_FALSE(err);

    if (!pc.MutatorsEnabled()) {
      // Can't proceed with these tests
      return;
    }

    // Failure: invalid tokens
    auto refresh_result = pc.RefreshState(false, {});
    ASSERT_TRUE(refresh_result) << refresh_result.error();
    ASSERT_EQ(refresh_result->status, Status::Success);
    pc.SetRequestMutators({"InvalidTokens"});
    auto purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::InvalidTokens);

    // Failure: no server response
    pc.SetRequestMutators({"Timeout:11"});
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_FALSE(purchase_result);

    // Failure: no data in response
    pc.SetRequestMutators({"Response:code=200,body=none"});
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_FALSE(purchase_result);

    // Failure: bad JSON
    pc.SetRequestMutators({"BadJSON:200"});
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_FALSE(purchase_result);

    // Success: One 500 response (sucessful retry)
    err = MAKE_1T_REWARD(pc, 1);
    ASSERT_FALSE(err) << err;
    pc.SetRequestMutators({"Response:code=500"});
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result) << purchase_result.error();
    ASSERT_EQ(purchase_result->status, Status::Success);

    // Success: Two 500 response (sucessful retry)
    err = MAKE_1T_REWARD(pc, 1);
    ASSERT_FALSE(err) << err;
    pc.SetRequestMutators({"Response:code=500", "Response:code=500"});
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::Success);

    // Failure: Three 500 responses (exceed retry)
    err = MAKE_1T_REWARD(pc, 1);
    ASSERT_FALSE(err) << err;
    pc.SetRequestMutators({"Response:code=500", "Response:code=500", "Response:code=500"});
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::ServerError);

    // Failure: unknown response code
    pc.SetRequestMutators({"Response:code=666"});
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_FALSE(purchase_result);
}

TEST_F(TestPsiCash, HTTPRequestBadResult) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), nullptr, false);
    ASSERT_FALSE(err);

    // This isn't a "bad" result, exactly, but we'll force an error code and message.
    auto want_error_message = "my RECOVERABLE_ERROR message"s;
    HTTPResult errResult;
    errResult.code = HTTPResult::RECOVERABLE_ERROR;
    errResult.error = want_error_message;
    pc.SetHTTPRequestFn(FakeHTTPRequester(errResult));
    auto refresh_result = pc.RefreshState(false, {});
    ASSERT_FALSE(refresh_result);
    ASSERT_NE(refresh_result.error().ToString().find(want_error_message), string::npos) << refresh_result.error().ToString();

    want_error_message = "my CRITICAL_ERROR message"s;
    errResult.code = HTTPResult::CRITICAL_ERROR;
    errResult.error = want_error_message;
    pc.SetHTTPRequestFn(FakeHTTPRequester(errResult));
    refresh_result = pc.RefreshState(false, {});
    ASSERT_FALSE(refresh_result);
    ASSERT_NE(refresh_result.error().ToString().find(want_error_message), string::npos) << refresh_result.error().ToString();
}

TEST_F(TestPsiCash, AccountLoginSimple) {
    // The initial internal release doesn't have Logout, so the tests need to be constrained.

    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
    ASSERT_FALSE(err);

    // Empty username and password
    auto res_login = pc.AccountLogin("", "");
    ASSERT_TRUE(res_login) << res_login.error();
    ASSERT_EQ(res_login->status, Status::BadRequest);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_FALSE(pc.HasTokens());
    ASSERT_FALSE(pc.AccountUsername());

    // Bad username
    auto rand = utils::RandomID(); // ensure we don't match a real user
    res_login = pc.AccountLogin(rand, "this is a bad password");
    ASSERT_TRUE(res_login) << res_login.error();
    ASSERT_EQ(res_login->status, Status::InvalidCredentials);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_FALSE(pc.HasTokens());
    ASSERT_FALSE(pc.AccountUsername());

    // Good username, bad password
    res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, "this is a bad password");
    ASSERT_TRUE(res_login);
    ASSERT_EQ(res_login->status, Status::InvalidCredentials);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_FALSE(pc.HasTokens());
    ASSERT_FALSE(pc.AccountUsername());

    // Good credentials
    res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login) << res_login.error();
    ASSERT_EQ(res_login->status, Status::Success);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.AccountUsername());
    ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_ONE_USERNAME);
    ASSERT_EQ(pc.user_data().ValidTokenTypes().size(), 4);
    ASSERT_THAT(pc.user_data().ValidTokenTypes(), AllOf(Contains("spender"), Contains("earner"), Contains("indicator"), Contains("logout")));
    ASSERT_EQ(pc.Balance(), 0); // we haven't called RefreshState yet
    auto prev_earner_token = pc.user_data().GetAuthTokens()["earner"].id;

    auto res_refresh = pc.RefreshState(false, {});
    ASSERT_TRUE(res_refresh);
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_GT(pc.Balance(), 0); // Our test accounts don't have zero balance

    // Try to log in again with bad creds
    res_login = pc.AccountLogin(rand, "this is a bad password");
    ASSERT_TRUE(res_login) << res_login.error();
    ASSERT_EQ(res_login->status, Status::InvalidCredentials);
    // Login state should not have changed
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.AccountUsername());
    ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_ONE_USERNAME);
    ASSERT_EQ(pc.user_data().ValidTokenTypes().size(), 4);
    ASSERT_THAT(pc.user_data().ValidTokenTypes(), AllOf(Contains("spender"), Contains("earner"), Contains("indicator"), Contains("logout")));

    // Good username, bad password
    res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, "this is a bad password");
    ASSERT_TRUE(res_login);
    ASSERT_EQ(res_login->status, Status::InvalidCredentials);
    // Login state should not have changed
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.AccountUsername());
    ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_ONE_USERNAME);
    ASSERT_EQ(pc.user_data().ValidTokenTypes().size(), 4);
    ASSERT_THAT(pc.user_data().ValidTokenTypes(), AllOf(Contains("spender"), Contains("earner"), Contains("indicator"), Contains("logout")));

    // Log in again with the same account
    res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login);
    ASSERT_EQ(res_login->status, Status::Success);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.AccountUsername());
    ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_ONE_USERNAME);
    ASSERT_EQ(pc.user_data().ValidTokenTypes().size(), 4);
    ASSERT_THAT(pc.user_data().ValidTokenTypes(), AllOf(Contains("spender"), Contains("earner"), Contains("indicator"), Contains("logout")));
    ASSERT_NE(pc.user_data().GetAuthTokens()["earner"].id, prev_earner_token); // should get a different token
    ASSERT_EQ(pc.Balance(), 0); // we haven't yet done a RefreshState

    // Different account, good credentials
    res_login = pc.AccountLogin(TEST_ACCOUNT_TWO_USERNAME, TEST_ACCOUNT_TWO_PASSWORD);
    ASSERT_TRUE(res_login);
    ASSERT_EQ(res_login->status, Status::Success);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.AccountUsername());
    ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_TWO_USERNAME);
    ASSERT_EQ(pc.user_data().ValidTokenTypes().size(), 4);
    ASSERT_THAT(pc.user_data().ValidTokenTypes(), AllOf(Contains("spender"), Contains("earner"), Contains("indicator"), Contains("logout")));
    ASSERT_NE(pc.user_data().GetAuthTokens()["earner"].id, prev_earner_token);
    ASSERT_EQ(pc.Balance(), 0); // we haven't yet done a RefreshState

    res_refresh = pc.RefreshState(false, {});
    ASSERT_TRUE(res_refresh);
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_GT(pc.Balance(), 0); // Our test accounts don't have zero balance

    // Different account, non-ASCII username and password
    res_login = pc.AccountLogin(TEST_ACCOUNT_UNICODE_USERNAME, TEST_ACCOUNT_UNICODE_PASSWORD);
    ASSERT_TRUE(res_login) << res_login.error();
    ASSERT_EQ(res_login->status, Status::Success);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.AccountUsername());
    ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_UNICODE_USERNAME);
    ASSERT_EQ(pc.user_data().ValidTokenTypes().size(), 4);
    ASSERT_THAT(pc.user_data().ValidTokenTypes(), AllOf(Contains("spender"), Contains("earner"), Contains("indicator"), Contains("logout")));
    ASSERT_NE(pc.user_data().GetAuthTokens()["earner"].id, prev_earner_token);
    ASSERT_EQ(pc.Balance(), 0); // we haven't yet done a RefreshState

    res_refresh = pc.RefreshState(false, {});
    ASSERT_TRUE(res_refresh);
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_GT(pc.Balance(), 0); // Our test accounts don't have zero balance
}

TEST_F(TestPsiCash, AccountLoginMerge) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
    ASSERT_FALSE(err);

    auto res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login);
    ASSERT_EQ(res_login->status, Status::Success);
    ASSERT_FALSE(res_login->last_tracker_merge);

    // Post-login RefreshState is required
    auto res_refresh = pc.RefreshState(false, {});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.HasTokens());

    auto account_starting_balance = pc.Balance();

    // Log out and reset so we can get a tracker
    auto res_logout = pc.AccountLogout();
    ASSERT_TRUE(res_logout);
    err = pc.ResetUser();
    ASSERT_FALSE(err);

    // Get a new tracker
    res_refresh = pc.RefreshState(false, {});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_TRUE(pc.HasTokens());
    ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));

    // Get some balance with which to make purchases
    err = MAKE_1T_REWARD(pc, 3);
    ASSERT_FALSE(err) << err;

    // Make a purchase
    auto purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::Success);
    auto expected_purchases = pc.GetPurchases();
    ASSERT_THAT(expected_purchases, SizeIs(1));

    auto tracker_balance = pc.Balance();
    auto expected_balance = account_starting_balance + tracker_balance;
    ASSERT_GT(expected_balance, account_starting_balance); // If it's not greater, then we're not really testing it

    // Log in, with bad username; should be no change to tracker
    res_login = pc.AccountLogin("badusername", TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login);
    ASSERT_EQ(res_login->status, Status::InvalidCredentials);
    res_refresh = pc.RefreshState(false, {});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_TRUE(pc.HasTokens());
    ASSERT_EQ(pc.Balance(), tracker_balance);

    // Log in, with bad password; should be no change to tracker
    res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, "badpassword");
    ASSERT_TRUE(res_login);
    ASSERT_EQ(res_login->status, Status::InvalidCredentials);
    res_refresh = pc.RefreshState(false, {});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_TRUE(pc.HasTokens());
    ASSERT_EQ(pc.Balance(), tracker_balance);

    // Log in, with merge
    res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login);
    ASSERT_EQ(res_login->status, Status::Success);
    ASSERT_TRUE(res_login->last_tracker_merge);
    ASSERT_FALSE(*res_login->last_tracker_merge); // this tracker has near-infinite merges

    res_refresh = pc.RefreshState(false, {});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.HasTokens());

    // The tracker merge should have given the account the tracker's balance...
    ASSERT_EQ(pc.Balance(), expected_balance);
    // ...and the tracker's purchases, which should have been retrieved
    ASSERT_EQ(pc.GetPurchases(), expected_purchases);

    // Force a "last tracker merge"
    if (pc.MutatorsEnabled()) {
        account_starting_balance = pc.Balance();

        // Log out and reset so we can get a tracker
        auto res_logout = pc.AccountLogout();
        ASSERT_TRUE(res_logout);
        err = pc.ResetUser();
        ASSERT_FALSE(err);

        // Get a new tracker
        res_refresh = pc.RefreshState(false, {});
        ASSERT_TRUE(res_refresh) << res_refresh.error();
        ASSERT_EQ(res_refresh->status, Status::Success);
        ASSERT_FALSE(pc.IsAccount());
        ASSERT_TRUE(pc.HasTokens());
        ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));

        expected_balance = account_starting_balance + pc.Balance();

        // Log in, with merge and mutator to force "last tracker merge"
        pc.SetRequestMutators({"EditBody:response,TrackerMergesRemaining=0"});
        res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
        ASSERT_TRUE(res_login);
        ASSERT_EQ(res_login->status, Status::Success);
        ASSERT_TRUE(res_login->last_tracker_merge);
        ASSERT_TRUE(*res_login->last_tracker_merge); // forced

        res_refresh = pc.RefreshState(false, {});
        ASSERT_TRUE(res_refresh) << res_refresh.error();
        ASSERT_EQ(res_refresh->status, Status::Success);
        ASSERT_TRUE(pc.IsAccount());
        ASSERT_TRUE(pc.HasTokens());

        ASSERT_EQ(pc.Balance(), expected_balance);
    }

    //
    // Force invalid tracker tokens to merge (will be rejected by server)
    //

    // Log out and reset so we can get a tracker
    res_logout = pc.AccountLogout();
    ASSERT_TRUE(res_logout);
    err = pc.ResetUser();
    ASSERT_FALSE(err);

    // Get a new tracker
    res_refresh = pc.RefreshState(false, {});
    ASSERT_TRUE(res_refresh) << res_refresh.error();
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_TRUE(pc.HasTokens());
    ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));

    // Log in, forcing tracker tokens to be invalid.
    // Note that the tokens are not passed via the auth header, so we can't use the `"InvalidTokens"` mutator.
    auto at = pc.user_data().GetAuthTokens();
    for (const auto& t : at) {
        at[t.first].id = at[t.first].id + "-INVALID";
    }
    pc.user_data().SetAuthTokens(at, false, "");

    res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login);
    ASSERT_EQ(res_login->status, Status::BadRequest);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_TRUE(pc.HasTokens());
}

TEST_F(TestPsiCash, AccountLogout) {
    // This also tests the combination of logging in and out

    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
    ASSERT_FALSE(err);

    // The instance ID should not change throughout this
    auto instance_id = pc.user_data().GetInstanceID();

    // Try to log out before logging in
    auto res_logout = pc.AccountLogout();
    ASSERT_FALSE(res_logout);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_FALSE(pc.HasTokens());
    ASSERT_FALSE(pc.AccountUsername());
    ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);

    // RefreshState to get a tracker and try to log out again
    auto res_refresh = pc.RefreshState(false, {});
    ASSERT_TRUE(res_refresh);
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_EQ(pc.user_data().ValidTokenTypes().size(), 3);
    ASSERT_THAT(pc.user_data().ValidTokenTypes(), AllOf(Contains("spender"), Contains("earner"), Contains("indicator")));
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);

    res_logout = pc.AccountLogout();
    ASSERT_FALSE(res_logout); // fails and has no effect
    ASSERT_EQ(pc.user_data().ValidTokenTypes().size(), 3);
    ASSERT_THAT(pc.user_data().ValidTokenTypes(), AllOf(Contains("spender"), Contains("earner"), Contains("indicator")));
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);

    // Log in with good credentials
    auto res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login) << res_login.error();
    ASSERT_EQ(res_login->status, Status::Success);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.AccountUsername());
    ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_ONE_USERNAME);
    ASSERT_EQ(pc.user_data().ValidTokenTypes().size(), 4);
    ASSERT_THAT(pc.user_data().ValidTokenTypes(), AllOf(Contains("spender"), Contains("earner"), Contains("indicator"), Contains("logout")));
    ASSERT_EQ(pc.Balance(), 0); // we haven't called RefreshState yet
    ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);
    auto prev_earner_token = pc.user_data().GetAuthTokens()["earner"].id;

    res_refresh = pc.RefreshState(false, {});
    ASSERT_TRUE(res_refresh);
    ASSERT_EQ(res_refresh->status, Status::Success);
    ASSERT_GT(pc.Balance(), 0); // Our test accounts don't have zero balance
    ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);

    res_logout = pc.AccountLogout();
    ASSERT_TRUE(res_logout) << res_logout.error();
    ASSERT_FALSE(res_logout->reconnect_required);
    // This is the state we should be in after a logout
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_FALSE(pc.HasTokens());
    ASSERT_FALSE(pc.AccountUsername());
    ASSERT_EQ(pc.Balance(), 0);
    ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);

    // We're in the was-account-logged-in state. Try to log out again.
    res_logout = pc.AccountLogout();
    ASSERT_FALSE(res_logout); // should fail
    // No change to this state
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_FALSE(pc.HasTokens());
    ASSERT_FALSE(pc.AccountUsername());
    ASSERT_EQ(pc.Balance(), 0);
    ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);

    // Log in again with the same user
    res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login) << res_login.error();
    ASSERT_EQ(res_login->status, Status::Success);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.AccountUsername());
    ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_ONE_USERNAME);
    ASSERT_EQ(pc.user_data().ValidTokenTypes().size(), 4);
    ASSERT_THAT(pc.user_data().ValidTokenTypes(), AllOf(Contains("spender"), Contains("earner"), Contains("indicator"), Contains("logout")));
    ASSERT_EQ(pc.Balance(), 0); // we haven't called RefreshState yet
    ASSERT_NE(pc.user_data().GetAuthTokens()["earner"].id, prev_earner_token); // different token
    ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);
    prev_earner_token = pc.user_data().GetAuthTokens()["earner"].id;

    // Log out, reset, log in with a different user (without first getting a new tracker)
    res_logout = pc.AccountLogout();
    ASSERT_TRUE(res_logout);
    ASSERT_FALSE(res_logout->reconnect_required);
    // This is the state we should be in after a logout
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_FALSE(pc.HasTokens());
    ASSERT_FALSE(pc.AccountUsername());
    ASSERT_EQ(pc.Balance(), 0);
    ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);

    err = pc.ResetUser();
    ASSERT_FALSE(err);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_FALSE(pc.HasTokens());
    ASSERT_FALSE(pc.AccountUsername());
    ASSERT_EQ(pc.Balance(), 0);
    ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);

    // Log in with a different user
    res_login = pc.AccountLogin(TEST_ACCOUNT_TWO_USERNAME, TEST_ACCOUNT_TWO_PASSWORD);
    ASSERT_TRUE(res_login);
    ASSERT_EQ(res_login->status, Status::Success);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.HasTokens());
    ASSERT_TRUE(pc.AccountUsername());
    ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_TWO_USERNAME);
    ASSERT_NE(pc.user_data().GetAuthTokens()["earner"].id, prev_earner_token); // different token
    ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);

    res_logout = pc.AccountLogout();
    ASSERT_TRUE(res_logout);
    ASSERT_FALSE(res_logout->reconnect_required);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_FALSE(pc.HasTokens());
    ASSERT_FALSE(pc.AccountUsername());
    ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);

    err = pc.ResetUser();
    ASSERT_FALSE(err);
    ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);

    // Should be back to not being an account
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_FALSE(pc.HasTokens());
    ASSERT_FALSE(pc.AccountUsername());

    // Ensure we can log in again
    res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login);
    ASSERT_EQ(res_login->status, Status::Success);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.HasTokens());
    ASSERT_TRUE(pc.AccountUsername());
    ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_ONE_USERNAME);
    ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);

    res_logout = pc.AccountLogout();
    ASSERT_TRUE(res_logout);
    ASSERT_FALSE(res_logout->reconnect_required);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_FALSE(pc.HasTokens());
    ASSERT_FALSE(pc.AccountUsername());
    ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);

    // Logging out twice is an error
    res_logout = pc.AccountLogout();
    ASSERT_FALSE(res_logout);
    ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);

    // We silently fall back to a local-only logout, so we're going to specifically check
    // that the remote logout is occurring (in the absence of an error).
    // First, get tokens.
    res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login);
    ASSERT_EQ(res_login->status, Status::Success);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.HasTokens());
    // Save them for later.
    auto auth_tokens = pc.user_data().GetAuthTokens();
    // Logout should invalidate the tokens remotely.
    res_logout = pc.AccountLogout();
    ASSERT_TRUE(res_logout);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_FALSE(pc.HasTokens());
    // Now restore the tokens and try to use them (note that we're in a pretty broken
    // state here but it should be good enough for this test).
    ASSERT_FALSE(pc.user_data().SetAuthTokens(auth_tokens, true, TEST_ACCOUNT_ONE_USERNAME));
    auto purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    // Server should respond with InvalidTokens
    ASSERT_EQ(purchase_result->status, Status::InvalidTokens);

    // Test logging out with invalid tokens.
    if (pc.MutatorsEnabled()) {
        // First log in normally.
        res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
        ASSERT_TRUE(res_login);
        ASSERT_EQ(res_login->status, Status::Success);
        ASSERT_TRUE(pc.IsAccount());
        ASSERT_TRUE(pc.HasTokens());
        ASSERT_TRUE(pc.AccountUsername());
        ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_ONE_USERNAME);
        ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);
        // Then log out with invalid tokens. It should succeed even though nothing was done
        // server-side, and have nuked our local state.
        pc.SetRequestMutators({"InvalidTokens"});
        res_logout = pc.AccountLogout();
        ASSERT_TRUE(res_logout);
        ASSERT_FALSE(res_logout->reconnect_required);
        ASSERT_TRUE(pc.IsAccount());
        ASSERT_FALSE(pc.HasTokens());
        ASSERT_FALSE(pc.AccountUsername());
        ASSERT_EQ(pc.user_data().GetInstanceID(), instance_id);
    }
}

TEST_F(TestPsiCash, AccountLogoutNeedReconnect) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
    ASSERT_FALSE(err);

    // Log in with good credentials
    auto res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login) << res_login.error();
    ASSERT_EQ(res_login->status, Status::Success);
    ASSERT_TRUE(pc.IsAccount());
    ASSERT_TRUE(pc.AccountUsername());
    ASSERT_EQ(*pc.AccountUsername(), TEST_ACCOUNT_ONE_USERNAME);
    auto res_refresh = pc.RefreshState(false, {});
    ASSERT_TRUE(res_refresh);
    ASSERT_EQ(res_refresh->status, Status::Success);

    // Ensure we have no active purchases when starting this test
    ASSERT_EQ(pc.ActivePurchases().size(), 0);

    // Get some balance with which to make purchases
    err = MAKE_1T_REWARD(pc, 3);
    ASSERT_FALSE(err) << err;

    // Log out with no active purchases
    auto res_logout = pc.AccountLogout();
    ASSERT_TRUE(res_logout) << res_logout.error();
    // No purchase, so no reconnect required
    ASSERT_FALSE(res_logout->reconnect_required);

    // Log in again with the same user
    res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login) << res_login.error();
    ASSERT_EQ(res_login->status, Status::Success);
    res_refresh = pc.RefreshState(false, {});
    ASSERT_TRUE(res_refresh);
    ASSERT_EQ(res_refresh->status, Status::Success);

    // Make a purchase that does _not_ produce an authorization
    auto purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::Success);
    ASSERT_FALSE(purchase_result->purchase->authorization);
    ASSERT_EQ(pc.ActivePurchases().size(), 1);

    // Log out, should still be no reconnect required
    res_logout = pc.AccountLogout();
    ASSERT_TRUE(res_logout);
    ASSERT_FALSE(res_logout->reconnect_required);

    // Log in again with the same user
    res_login = pc.AccountLogin(TEST_ACCOUNT_ONE_USERNAME, TEST_ACCOUNT_ONE_PASSWORD);
    ASSERT_TRUE(res_login) << res_login.error();
    ASSERT_EQ(res_login->status, Status::Success);
    res_refresh = pc.RefreshState(false, {});
    ASSERT_TRUE(res_refresh);
    ASSERT_EQ(res_refresh->status, Status::Success);

    // Make a purchase that _does_ produce an authorization
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_WITH_AUTHORIZATION_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::Success);
    ASSERT_TRUE(purchase_result->purchase->authorization);
    ASSERT_EQ(pc.ActivePurchases().size(), 2) << json(pc.ActivePurchases());

    // Log out; now that we have a purchase with authorization, we should need reconnect
    res_logout = pc.AccountLogout();
    ASSERT_TRUE(res_logout);
    ASSERT_TRUE(res_logout->reconnect_required);
}

TEST_F(TestPsiCash, PurchaseFromJSON) {
    PsiCashTester pc;
    auto err = pc.Init(TestPsiCash::UserAgent(), GetTempDir().c_str(), HTTPRequester, false);
    ASSERT_FALSE(err);

    // Basically no diff
    ASSERT_FALSE(pc.user_data().SetServerTimeDiff(datetime::DateTime::Now()));

    auto j = R"|({
        "TransactionID": "txid",
        "Created": "2001-01-01T01:01:01.001Z",
        "Class": "txclass",
        "Distinguisher": "txdistinguisher",
        "Authorization": "eyJBdXRob3JpemF0aW9uIjp7IklEIjoiMFYzRXhUdmlBdFNxTGZOd2FpQXlHNHpaRUJJOGpIYnp5bFdNeU5FZ1JEZz0iLCJBY2Nlc3NUeXBlIjoic3BlZWQtYm9vc3QtdGVzdCIsIkV4cGlyZXMiOiIyMDE5LTAxLTE0VDE3OjIyOjIzLjE2ODc2NDEyOVoifSwiU2lnbmluZ0tleUlEIjoiUUNZTzV2clIvZGhjRDZ6M2FMQlVNeWRuZlJyZFNRL1RWYW1IUFhYeTd0TT0iLCJTaWduYXR1cmUiOiJQL2NrenloVUJoSk5RQ24zMnluM1VTdGpLencxU04xNW9MclVhTU9XaW9scXBOTTBzNVFSNURHVEVDT1FzQk13ODdQdTc1TGE1OGtJTHRIcW1BVzhDQT09In0=",
        "TransactionResponse": {
            "Type": "expected_type",
            "Values": {
                "Expires": "2001-01-01T02:01:01.001Z"
            }
        }
    })|"_json;

    // Simple success
    auto res = pc.PurchaseFromJSON(j);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->id, "txid");
    ASSERT_EQ(res->transaction_class, "txclass");
    ASSERT_EQ(res->distinguisher, "txdistinguisher");
    ASSERT_TRUE(res->authorization);
    datetime::DateTime dt;
    ASSERT_TRUE(dt.FromISO8601("2001-01-01T01:01:01.001Z"));
    ASSERT_EQ(dt, res->server_time_created);
    ASSERT_TRUE(dt.FromISO8601("2001-01-01T02:01:01.001Z"));
    ASSERT_EQ(dt, *res->server_time_expiry);
    ASSERT_NEAR(res->local_time_expiry->MillisSinceEpoch(), dt.MillisSinceEpoch(), 500);

    // Expected type mismatch
    res = pc.PurchaseFromJSON(j, "won't match");
    ASSERT_FALSE(res);

    // Bad created date
    auto prev_val = j["Created"];
    j["Created"] = "nope";
    res = pc.PurchaseFromJSON(j);
    ASSERT_FALSE(res);
    j["Created"] = prev_val; // put it back for following tests

    // Bad expiry date
    prev_val = j["/TransactionResponse/Values/Expires"_json_pointer];
    j["/TransactionResponse/Values/Expires"_json_pointer] = "nope";
    res = pc.PurchaseFromJSON(j);
    ASSERT_FALSE(res);
    j["/TransactionResponse/Values/Expires"_json_pointer] = prev_val;

    // Authorization decode fail
    prev_val = j["Authorization"];
    j["Authorization"] = "nope";
    res = pc.PurchaseFromJSON(j);
    ASSERT_FALSE(res);
    j["Authorization"] = prev_val;

    // Missing expected JSON field
    prev_val = j["TransactionID"];
    j["TransactionID"] = nullptr;
    res = pc.PurchaseFromJSON(j);
    ASSERT_FALSE(res);
    j["TransactionID"] = prev_val;

    prev_val = j["TransactionID"];
    j.erase("TransactionID");
    res = pc.PurchaseFromJSON(j);
    ASSERT_FALSE(res);
    j["TransactionID"] = prev_val;
}
