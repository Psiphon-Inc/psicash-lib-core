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

using namespace std;
using namespace psicash;
using namespace testing;

constexpr int64_t MAX_STARTING_BALANCE = 100000000000LL;

class TestPsiCash : public ::testing::Test, public TempDir {
  public:
    TestPsiCash() : user_agent_("Psiphon-PsiCash-iOS") {
    }

    static HTTPResult HTTPRequester(const HTTPParams& params) {
        stringstream curl;
        curl << "curl -s -i --max-time 5";
        curl << " -X " << params.method;

        for (auto it = params.headers.begin(); it != params.headers.end(); ++it) {
            curl << " -H \"" << it->first << ":" << it->second << "\"";
        }

        curl << ' ';
        curl << '"' << params.scheme << "://";
        curl << params.hostname << ":" << params.port;
        curl << params.path;

        // query is an array of 2-tuple name-value arrays
        for (auto it = params.query.begin(); it != params.query.end(); ++it) {
            if (it == params.query.begin()) {
                curl << "?";
            } else {
                curl << "&";
            }

            curl << it->first << "=" << it->second;
        }
        curl << '"';

        HTTPResult result;

        auto command = curl.str();
        string output;
        auto code = exec(command.c_str(), output);
        if (code != 0) {
            result.error = output;
            return result;
        }

        std::stringstream ss(output);
        std::string line;

        string body, full_output;
        bool done_headers = false;
        while (std::getline(ss, line, '\n')) {
            line = trim(line);
            if (line.empty()) {
                done_headers = true;
            }

            full_output += line + "\n";

            if (!done_headers) {
                smatch match_pieces;

                // Look for HTTP status code value (200, etc.)
                regex status_regex("^HTTP\\/\\d\\S* (\\d\\d\\d).*$",
                                   regex_constants::ECMAScript | regex_constants::icase);
                if (regex_match(line, match_pieces, status_regex)) {
                    result.code = stoi(match_pieces[1].str());
                }

                // Look for the Date header
                regex date_regex("^Date: (.+)$",
                                 regex_constants::ECMAScript | regex_constants::icase);
                if (regex_match(line, match_pieces, date_regex)) {
                    result.date = match_pieces[1].str();
                }
            }

            if (done_headers) {
                body += line;
            }
        }

        result.body = body;

        if (result.code < 0) {
            // Something went wrong during processing. Set the whole output as the error.
            result.error = full_output;
        }

        return result;
    }

    // Return a specific result for a HTTP request
    static psicash::MakeHTTPRequestFn FakeHTTPRequester(const HTTPResult& result) {
        return [=](const HTTPParams& params) -> HTTPResult {
            return result;
        };
    }

    const char* user_agent_;

};

#define MAKE_1T_REWARD(pc, count) (pc.MakeRewardRequests(TEST_CREDIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, count))


TEST_F(TestPsiCash, InitSimple) {
    {
        PsiCash pc;
        auto err = pc.Init(user_agent_, GetTempDir().c_str(), HTTPRequester, false);
        ASSERT_FALSE(err);
    }

    {
        PsiCash pc;
        auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
        ASSERT_FALSE(err);
    }
}

TEST_F(TestPsiCash, InitFail) {
    /* This test is flaky, and I don't know why.
    {
        // Datastore directory that will not work
        auto bad_dir = GetTempDir() + "/a/b/c/d/f/g";
        PsiCashTester pc;
        auto err = pc.Init(user_agent_, bad_dir.c_str(), nullptr, true);
        // This occasionally fails to fail, and I don't know why
        // TEMP
        if (!err) {
          err = pc.user_data().SetIsAccount(false);
          cout << "Did write work? " << err << endl;
        }
        ASSERT_TRUE(err) << bad_dir;
    }
    */
    {
        // Null datastore directory
        PsiCash pc;
        auto err = pc.Init(user_agent_, nullptr, nullptr, true);
        ASSERT_TRUE(err);
    }
    {
        // Null user agent
        PsiCash pc;
        auto err = pc.Init(nullptr, GetTempDir().c_str(), nullptr, true);
        ASSERT_TRUE(err);
    }
}

TEST_F(TestPsiCash, SetHTTPRequestFn) {
    {
        PsiCash pc;
        auto err = pc.Init(user_agent_, GetTempDir().c_str(), HTTPRequester, true);
        ASSERT_FALSE(err);
        pc.SetHTTPRequestFn(HTTPRequester);
    }

    {
        PsiCash pc;
        auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
        ASSERT_FALSE(err);
        pc.SetHTTPRequestFn(HTTPRequester);
    }
}

TEST_F(TestPsiCash, SetRequestMetadataItem) {
    PsiCashTester pc;
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
    ASSERT_FALSE(err);

    auto j = pc.user_data().GetRequestMetadata();
    ASSERT_EQ(j.size(), 0);

    err = pc.SetRequestMetadataItem("k", "v");
    ASSERT_FALSE(err);

    j = pc.user_data().GetRequestMetadata();
    ASSERT_EQ(j["k"], "v");
}

TEST_F(TestPsiCash, IsAccount) {
    PsiCashTester pc;
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
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
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
    ASSERT_FALSE(err);

    auto vtt = pc.ValidTokenTypes();
    ASSERT_EQ(vtt.size(), 0);

    AuthTokens at = {{"a", "a"}, {"b", "b"}, {"c", "c"}};
    err = pc.user_data().SetAuthTokens(at, false);
    vtt = pc.ValidTokenTypes();
    ASSERT_EQ(vtt.size(), 3);
    for (const auto& k : vtt) {
        ASSERT_EQ(at.count(k), 1);
        at.erase(k);
    }
    ASSERT_EQ(at.size(), 0); // we should have erase all items

    AuthTokens empty;
    err = pc.user_data().SetAuthTokens(empty, false);
    vtt = pc.ValidTokenTypes();
    ASSERT_EQ(vtt.size(), 0);
}

TEST_F(TestPsiCash, Balance) {
    PsiCashTester pc;
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
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
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
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
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
    ASSERT_FALSE(err);

    auto v = pc.GetPurchases();
    ASSERT_EQ(v.size(), 0);

    auto auth_res = DecodeAuthorization("eyJBdXRob3JpemF0aW9uIjp7IklEIjoiMFYzRXhUdmlBdFNxTGZOd2FpQXlHNHpaRUJJOGpIYnp5bFdNeU5FZ1JEZz0iLCJBY2Nlc3NUeXBlIjoic3BlZWQtYm9vc3QtdGVzdCIsIkV4cGlyZXMiOiIyMDE5LTAxLTE0VDE3OjIyOjIzLjE2ODc2NDEyOVoifSwiU2lnbmluZ0tleUlEIjoiUUNZTzV2clIvZGhjRDZ6M2FMQlVNeWRuZlJyZFNRL1RWYW1IUFhYeTd0TT0iLCJTaWduYXR1cmUiOiJQL2NrenloVUJoSk5RQ24zMnluM1VTdGpLencxU04xNW9MclVhTU9XaW9scXBOTTBzNVFSNURHVEVDT1FzQk13ODdQdTc1TGE1OGtJTHRIcW1BVzhDQT09In0=");
    ASSERT_TRUE(auth_res);

    Purchases ps = {
            {"id1", "tc1", "d1", datetime::DateTime::Now(), datetime::DateTime::Now(), *auth_res},
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

TEST_F(TestPsiCash, ActivePurchases) {
    PsiCashTester pc;
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
    ASSERT_FALSE(err);

    auto v = pc.GetPurchases();
    ASSERT_EQ(v.size(), 0);

    v = pc.ActivePurchases();
    ASSERT_EQ(v.size(), 0);

    auto before_now = datetime::DateTime::Now().Sub(datetime::Duration(54321));
    auto after_now = datetime::DateTime::Now().Add(datetime::Duration(54321));

    Purchases ps = {{"id1", "tc1", "d1", before_now, nonstd::nullopt, nonstd::nullopt},
                    {"id2", "tc2", "d2", after_now, nonstd::nullopt, nonstd::nullopt},
                    {"id3", "tc3", "d3", before_now, nonstd::nullopt, nonstd::nullopt},
                    {"id4", "tc4", "d4", after_now, nonstd::nullopt, nonstd::nullopt},
                    {"id5", "tc5", "d5", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt}};

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
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
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

    const auto encoded1 = "eyJBdXRob3JpemF0aW9uIjp7IklEIjoiMFYzRXhUdmlBdFNxTGZOd2FpQXlHNHpaRUJJOGpIYnp5bFdNeU5FZ1JEZz0iLCJBY2Nlc3NUeXBlIjoic3BlZWQtYm9vc3QtdGVzdCIsIkV4cGlyZXMiOiIyMDE5LTAxLTE0VDE3OjIyOjIzLjE2ODc2NDEyOVoifSwiU2lnbmluZ0tleUlEIjoiUUNZTzV2clIvZGhjRDZ6M2FMQlVNeWRuZlJyZFNRL1RWYW1IUFhYeTd0TT0iLCJTaWduYXR1cmUiOiJQL2NrenloVUJoSk5RQ24zMnluM1VTdGpLencxU04xNW9MclVhTU9XaW9scXBOTTBzNVFSNURHVEVDT1FzQk13ODdQdTc1TGE1OGtJTHRIcW1BVzhDQT09In0=";
    const auto encoded2 = "eyJBdXRob3JpemF0aW9uIjp7IklEIjoibFRSWnBXK1d3TFJqYkpzOGxBUFVaQS8zWnhmcGdwNDFQY0dkdlI5a0RVST0iLCJBY2Nlc3NUeXBlIjoic3BlZWQtYm9vc3QtdGVzdCIsIkV4cGlyZXMiOiIyMDE5LTAxLTE0VDIxOjQ2OjMwLjcxNzI2NTkyNFoifSwiU2lnbmluZ0tleUlEIjoiUUNZTzV2clIvZGhjRDZ6M2FMQlVNeWRuZlJyZFNRL1RWYW1IUFhYeTd0TT0iLCJTaWduYXR1cmUiOiJtV1Z5Tm9ZU0pFRDNXU3I3bG1OeEtReEZza1M5ZWlXWG1lcDVvVWZBSHkwVmYrSjZaQW9WajZrN3ZVTDNrakIreHZQSTZyaVhQc3FzWENRNkx0eFdBQT09In0=";

    auto auth_res1 = psicash::DecodeAuthorization(encoded1);
    auto auth_res2 = psicash::DecodeAuthorization(encoded2);
    ASSERT_TRUE(auth_res1);
    ASSERT_TRUE(auth_res2);

    purchases = {{"future_no_auth", "tc1", "d1", after_now, nonstd::nullopt, nonstd::nullopt},
                 {"past_auth", "tc2", "d2", before_now, nonstd::nullopt, *auth_res1},
                 {"future_auth", "tc3", "d3", after_now, nonstd::nullopt, *auth_res2}};

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
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
    ASSERT_FALSE(err);

    auto purchases = pc.GetPurchases();
    ASSERT_EQ(purchases.size(), 0);

    // Empty set of auth IDs
    purchases = pc.GetPurchasesByAuthorizationID({});
    ASSERT_EQ(purchases.size(), 0);

    auto before_now = datetime::DateTime::Now().Sub(datetime::Duration(54321));
    auto after_now = datetime::DateTime::Now().Add(datetime::Duration(54321));

    const auto encoded1 = "eyJBdXRob3JpemF0aW9uIjp7IklEIjoiMFYzRXhUdmlBdFNxTGZOd2FpQXlHNHpaRUJJOGpIYnp5bFdNeU5FZ1JEZz0iLCJBY2Nlc3NUeXBlIjoic3BlZWQtYm9vc3QtdGVzdCIsIkV4cGlyZXMiOiIyMDE5LTAxLTE0VDE3OjIyOjIzLjE2ODc2NDEyOVoifSwiU2lnbmluZ0tleUlEIjoiUUNZTzV2clIvZGhjRDZ6M2FMQlVNeWRuZlJyZFNRL1RWYW1IUFhYeTd0TT0iLCJTaWduYXR1cmUiOiJQL2NrenloVUJoSk5RQ24zMnluM1VTdGpLencxU04xNW9MclVhTU9XaW9scXBOTTBzNVFSNURHVEVDT1FzQk13ODdQdTc1TGE1OGtJTHRIcW1BVzhDQT09In0=";
    const auto encoded2 = "eyJBdXRob3JpemF0aW9uIjp7IklEIjoibFRSWnBXK1d3TFJqYkpzOGxBUFVaQS8zWnhmcGdwNDFQY0dkdlI5a0RVST0iLCJBY2Nlc3NUeXBlIjoic3BlZWQtYm9vc3QtdGVzdCIsIkV4cGlyZXMiOiIyMDE5LTAxLTE0VDIxOjQ2OjMwLjcxNzI2NTkyNFoifSwiU2lnbmluZ0tleUlEIjoiUUNZTzV2clIvZGhjRDZ6M2FMQlVNeWRuZlJyZFNRL1RWYW1IUFhYeTd0TT0iLCJTaWduYXR1cmUiOiJtV1Z5Tm9ZU0pFRDNXU3I3bG1OeEtReEZza1M5ZWlXWG1lcDVvVWZBSHkwVmYrSjZaQW9WajZrN3ZVTDNrakIreHZQSTZyaVhQc3FzWENRNkx0eFdBQT09In0=";

    auto auth_res1 = psicash::DecodeAuthorization(encoded1);
    auto auth_res2 = psicash::DecodeAuthorization(encoded2);
    ASSERT_TRUE(auth_res1);
    ASSERT_TRUE(auth_res2);

    purchases = {{"future_no_auth", "tc1", "d1", after_now, nonstd::nullopt, nonstd::nullopt},
                 {"past_auth", "tc2", "d2", before_now, nonstd::nullopt, *auth_res1},
                 {"future_auth", "tc3", "d3", after_now, nonstd::nullopt, *auth_res2}};

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
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
    ASSERT_FALSE(err);

    auto v = pc.GetPurchases();
    ASSERT_EQ(v.size(), 0);

    auto p = pc.NextExpiringPurchase();
    ASSERT_FALSE(p);

    auto first = datetime::DateTime::Now().Sub(datetime::Duration(333));
    auto second = datetime::DateTime::Now().Sub(datetime::Duration(222));
    auto third = datetime::DateTime::Now().Sub(datetime::Duration(111));

    Purchases ps = {{"id1", "tc1", "d1", second, nonstd::nullopt, nonstd::nullopt},
                    {"id2", "tc2", "d2", first, nonstd::nullopt, nonstd::nullopt}, // first to expire
                    {"id3", "tc3", "d3", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
                    {"id4", "tc4", "d4", third, nonstd::nullopt, nonstd::nullopt}};

    err = pc.user_data().SetPurchases(ps);
    ASSERT_FALSE(err);

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), ps.size());
    ASSERT_EQ(v, ps);

    p = pc.NextExpiringPurchase();
    ASSERT_TRUE(p);
    ASSERT_EQ(p->id, ps[1].id);

    auto later_than_now = datetime::DateTime::Now().Add(datetime::Duration(54321));
    ps = {{"id1", "tc1", "d1", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
          {"id2", "tc2", "d2", later_than_now, nonstd::nullopt, nonstd::nullopt}, // only expiring
          {"id3", "tc3", "d3", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt}};

    err = pc.user_data().SetPurchases(ps);
    ASSERT_FALSE(err);

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), ps.size());
    ASSERT_EQ(v, ps);

    p = pc.NextExpiringPurchase();
    ASSERT_TRUE(p);
    ASSERT_EQ(p->id, ps[1].id);

    // None expiring
    ps = {{"id1", "tc1", "d1", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
          {"id2", "tc2", "d2", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt}};

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
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
    ASSERT_FALSE(err);

    auto v = pc.GetPurchases();
    ASSERT_EQ(v.size(), 0);

    auto res = pc.ExpirePurchases();
    ASSERT_TRUE(res);
    ASSERT_EQ(res->size(), 0);

    auto before_now = datetime::DateTime::Now().Sub(datetime::Duration(54321));
    auto after_now = datetime::DateTime::Now().Add(datetime::Duration(54321));

    Purchases ps = {{"id1", "tc1", "d1", after_now, nonstd::nullopt, nonstd::nullopt},
                    {"id2", "tc2", "d2", before_now, nonstd::nullopt, nonstd::nullopt},
                    {"id3", "tc3", "d3", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
                    {"id4", "tc4", "d4", before_now, nonstd::nullopt, nonstd::nullopt}};
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
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
    ASSERT_FALSE(err);

    auto v = pc.GetPurchases();
    ASSERT_EQ(v.size(), 0);

    Purchases ps = {{"id1", "tc1", "d1", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
                    {"id2", "tc2", "d2", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
                    {"id3", "tc3", "d3", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt},
                    {"id4", "tc4", "d4", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt}};
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

TEST_F(TestPsiCash, ModifyLandingPage) {
    PsiCashTester pc;
    // Pass false for test so that we don't get "dev" and "debug" in all the params
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, false);
    ASSERT_FALSE(err);

    const string key_part = "psicash=";
    URL url_in, url_out;

    //
    // No metadata set
    //

    auto encoded_data = base64::TrimPadding(base64::B64Encode(utils::Stringer(R"({"metadata":{"user_agent":")", user_agent_, R"("},"tokens":null,"v":1})")));

    url_in = {"https://asdf.sadf.gf", "", ""};
    auto res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.query_, url_in.query_);
    ASSERT_EQ(url_out.fragment_,
              "!"s + key_part + encoded_data);

    url_in = {"https://asdf.sadf.gf", "gfaf=asdf", ""};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.query_, url_in.query_);
    ASSERT_EQ(url_out.fragment_,
              "!"s + key_part + encoded_data);

    url_in = {"https://asdf.sadf.gf/asdfilj/adf", "gfaf=asdf", ""};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.query_, url_in.query_);
    ASSERT_EQ(url_out.fragment_,
              "!"s + key_part + encoded_data);

    url_in = {"https://asdf.sadf.gf/asdfilj/adf.html", "gfaf=asdf", ""};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.query_, url_in.query_);
    ASSERT_EQ(url_out.fragment_,
              "!"s + key_part + encoded_data);

    url_in = {"https://asdf.sadf.gf/asdfilj/adf.html", "", "regffd"};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.query_,
              key_part + encoded_data);
    ASSERT_EQ(url_out.fragment_, url_in.fragment_);

    url_in = {"https://asdf.sadf.gf/asdfilj/adf.html", "adfg=asdf&vfjnk=fadjn", "regffd"};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.query_,
              url_in.query_ + "&" + key_part + encoded_data);
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
    ASSERT_EQ(url_out.query_, url_in.query_);
    encoded_data = base64::TrimPadding(base64::B64Encode(utils::Stringer(R"({"metadata":{"k":"v","user_agent":")", user_agent_, R"("},"tokens":null,"v":1})")));
    ASSERT_EQ(url_out.fragment_, "!"s + key_part + encoded_data);

    // With tokens

    AuthTokens auth_tokens = {{kSpenderTokenType, "kSpenderTokenType"},
                              {kEarnerTokenType, "kEarnerTokenType"},
                              {kIndicatorTokenType, "kIndicatorTokenType"}};
    err = pc.user_data().SetAuthTokens(auth_tokens, false);
    ASSERT_FALSE(err);
    url_in = {"https://asdf.sadf.gf", "", ""};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.query_, url_in.query_);
    encoded_data = base64::TrimPadding(base64::B64Encode(utils::Stringer(R"({"metadata":{"k":"v","user_agent":")", user_agent_, R"("},"tokens":"kEarnerTokenType","v":1})")));
    ASSERT_EQ(url_out.fragment_, "!"s + key_part + encoded_data);

    // Some tokens, but no earner token (different code path)
    auth_tokens = {{kSpenderTokenType, "kSpenderTokenType"},
                   {kIndicatorTokenType, "kIndicatorTokenType"}};
    err = pc.user_data().SetAuthTokens(auth_tokens, false);
    ASSERT_FALSE(err);
    url_in = {"https://asdf.sadf.gf", "", ""};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.query_, url_in.query_);
    encoded_data = base64::TrimPadding(base64::B64Encode(utils::Stringer(R"({"metadata":{"k":"v","user_agent":")", user_agent_, R"("},"tokens":null,"v":1})")));
    ASSERT_EQ(url_out.fragment_, "!"s + key_part + encoded_data);

    //
    // Errors
    //

    res = pc.ModifyLandingPage("#$%^&");
    ASSERT_FALSE(res);
}

TEST_F(TestPsiCash, GetRewardedActivityData) {
    PsiCashTester pc;
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
    ASSERT_FALSE(err);

    // Error with no tokens
    auto res = pc.GetRewardedActivityData();
    ASSERT_FALSE(res);

    AuthTokens auth_tokens = {{kSpenderTokenType, "kSpenderTokenType"},
                              {kEarnerTokenType, "kEarnerTokenType"},
                              {kIndicatorTokenType, "kIndicatorTokenType"}};
    err = pc.user_data().SetAuthTokens(auth_tokens, false);
    ASSERT_FALSE(err);

    res = pc.GetRewardedActivityData();
    ASSERT_TRUE(res);
    ASSERT_EQ(*res, base64::B64Encode("{\"metadata\":{},\"tokens\":"
                                      "\"kEarnerTokenType\",\"v\":1}"));

    err = pc.SetRequestMetadataItem("k", "v");
    ASSERT_FALSE(err);
    res = pc.GetRewardedActivityData();
    ASSERT_TRUE(res);
    ASSERT_EQ(*res, base64::B64Encode("{\"metadata\":{\"k\":\"v\"},\"tokens\":"
                                      "\"kEarnerTokenType\",\"v\":1}"));
}

TEST_F(TestPsiCash, GetDiagnosticInfo) {
    PsiCashTester pc;
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
    ASSERT_FALSE(err);

    auto want = R"|({
    "balance":0,
    "isAccount":false,
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
            {{"id2", "tc2", "d2", nonstd::nullopt, nonstd::nullopt, nonstd::nullopt}});
    pc.user_data().SetAuthTokens({{"a", "a"}, {"b", "b"}, {"c", "c"}}, true);
    // pc.user_data().SetServerTimeDiff() // too hard to do reliably
    want = R"|({
    "balance":12345,
    "isAccount":true,
    "purchasePrices":[{"distinguisher":"d1","price":123,"class":"tc1"},{"distinguisher":"d2","price":321,"class":"tc2"}],
    "purchases":[{"class":"tc2","distinguisher":"d2"}],
    "serverTimeDiff":0,
    "test":true,
    "validTokenTypes":["a","b","c"]
    })|"_json;
    j = pc.GetDiagnosticInfo();
    ASSERT_EQ(j, want);
}

TEST_F(TestPsiCash, RefreshState) {
    PsiCashTester pc;
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), HTTPRequester, true);
    ASSERT_FALSE(err);

    pc.user_data().Clear();
    ASSERT_TRUE(pc.ValidTokenTypes().empty());

    // Basic NewTracker success
    auto res = pc.RefreshState({"speed-boost"});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_GE(pc.ValidTokenTypes().size(), 3);
    ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));
    ASSERT_GE(pc.GetPurchasePrices().size(), 2);

    // Test with existing tracker
    auto want_tokens = pc.user_data().GetAuthTokens();
    res = pc.RefreshState({"speed-boost"});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_GE(pc.ValidTokenTypes().size(), 3);
    ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));
    auto speed_boost_purchase_prices = pc.GetPurchasePrices();
    ASSERT_GE(pc.GetPurchasePrices().size(), 2);
    ASSERT_EQ(want_tokens, pc.user_data().GetAuthTokens());

    // Multiple purchase classes
    pc.user_data().Clear();
    res = pc.RefreshState({"speed-boost", TEST_DEBIT_TRANSACTION_CLASS});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_GE(pc.ValidTokenTypes().size(), 3);
    ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));
    ASSERT_GT(pc.GetPurchasePrices().size(), speed_boost_purchase_prices.size());

    // No purchase classes
    pc.user_data().Clear();
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_GE(pc.ValidTokenTypes().size(), 3);
    ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));
    ASSERT_EQ(pc.GetPurchasePrices().size(), 0); // we didn't ask for any

    // Purchase classes, then none; verify that previous aren't lost
    pc.user_data().Clear();
    res = pc.RefreshState({"speed-boost"}); // with class
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    speed_boost_purchase_prices = pc.GetPurchasePrices();
    ASSERT_GT(speed_boost_purchase_prices.size(), 3);
    res = pc.RefreshState({}); // without class
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    ASSERT_EQ(pc.GetPurchasePrices().size(), speed_boost_purchase_prices.size());

    // Balance increase
    pc.user_data().Clear();
    res = pc.RefreshState({"speed-boost"}); // with class
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    auto starting_balance = pc.Balance();
    err = MAKE_1T_REWARD(pc, 1);
    ASSERT_FALSE(err) << err;
    res = pc.RefreshState({"speed-boost"}); // with class
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    ASSERT_EQ(pc.Balance(), starting_balance + ONE_TRILLION);

    // Test bad is-account state. This is a local sanity check failure that will occur
    // after the request to the server sees an illegal is-account state.
    pc.user_data().Clear();
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    // We're setting "isAccount" with tracker tokens. This is not okay and shouldn't happen.
    pc.user_data().SetIsAccount(true);
    res = pc.RefreshState({});
    ASSERT_FALSE(res) << res.error();

    // Test is-account with no tokens
    pc.user_data().Clear();                 // blow away existing tokens
    pc.user_data().SetIsAccount(true);      // force is-account
    res = pc.RefreshState({"speed-boost"}); // ask for purchase prices
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    ASSERT_TRUE(pc.IsAccount());               // should still be is-account
    ASSERT_EQ(pc.ValidTokenTypes().size(), 0); // but no tokens
    ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));
    ASSERT_EQ(pc.GetPurchasePrices().size(),
              0); // shouldn't get any, because no valid indicator token
}

TEST_F(TestPsiCash, RefreshStateMutators) {
    PsiCashTester pc;
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), HTTPRequester, true);
    ASSERT_FALSE(err);

    if (!pc.MutatorsEnabled()) {
      // Can't proceed with these tests
      return;
    }

    // Tracker with invalid tokens
    pc.user_data().Clear();
    auto res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    auto prev_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(prev_tokens.size(), 3);
    err = pc.user_data().SetBalance(12345); // force a nonzero balance
    ASSERT_FALSE(err);
    ASSERT_GT(pc.Balance(), 0);
    // We have tokens; force the server to consider them invalid
    pc.SetRequestMutators({"InvalidTokens"});
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    // We should have brand new tokens now.
    auto next_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(next_tokens.size(), 3);
    ASSERT_NE(prev_tokens, next_tokens);
    // And a reset balance
    ASSERT_THAT(pc.Balance(), AllOf(Ge(0), Le(MAX_STARTING_BALANCE)));

    // Account with invalid tokens
    pc.user_data().Clear();
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    ASSERT_GE(pc.user_data().GetAuthTokens().size(), 3);
    // We're setting "isAccount" with tracker tokens. This is not okay, but the server is
    // going to blindly consider them invalid anyway.
    pc.user_data().SetIsAccount(true);
    // We have tokens; force the server to consider them invalid
    pc.SetRequestMutators({"InvalidTokens"});
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    // Accounts won't get new tokens by refreshing, so now we should have none
    ASSERT_GE(pc.user_data().GetAuthTokens().size(), 0);

    // Tracker with always-invalid tokens (impossible to get valid ones)
    pc.user_data().Clear();
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    ASSERT_GE(pc.user_data().GetAuthTokens().size(), 3);
    // We have tokens; force the server to consider them invalid
    pc.SetRequestMutators({
            "InvalidTokens", // RefreshState
            "",              // NewTracker
            "InvalidTokens", // RefreshState
    });
    res = pc.RefreshState({});
    // Should have failed utterly
    ASSERT_FALSE(res) << static_cast<int>(*res);
    // Should be no tokens
    ASSERT_EQ(pc.user_data().GetAuthTokens().size(), 0);
    ASSERT_EQ(pc.Balance(), 0);

    // No server response for NewTracker
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // First request is NewTracker, sleep for 11 secs
    pc.SetRequestMutators({"Timeout:11"});
    res = pc.RefreshState({});
    ASSERT_FALSE(res) << static_cast<int>(*res);
    // Should be no tokens
    ASSERT_EQ(pc.user_data().GetAuthTokens().size(), 0);
    ASSERT_EQ(pc.Balance(), 0);

    // No server response for RefreshState
    pc.user_data().Clear();
    // Do an initial request to get Tracker tokens
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    auto auth_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(auth_tokens.size(), 3);
    // Because we have tokens the first request will be RefreshState; sleep for 11 secs
    pc.SetRequestMutators({"Timeout:11"});
    res = pc.RefreshState({});
    ASSERT_FALSE(res) << static_cast<int>(*res);
    // Tokens should be unchanged
    ASSERT_EQ(auth_tokens, pc.user_data().GetAuthTokens());

    // NewTracker response with no data
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // First request is NewTracker; force an empty response
    pc.SetRequestMutators({"Response:code=200,body=none"});
    res = pc.RefreshState({});
    ASSERT_FALSE(res) << static_cast<int>(*res);
    // Should be no tokens
    ASSERT_EQ(pc.user_data().GetAuthTokens().size(), 0);
    ASSERT_EQ(pc.Balance(), 0);

    // Empty server response for RefreshState
    pc.user_data().Clear();
    // Do an initial request to get Tracker tokens
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    auth_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(auth_tokens.size(), 3);
    // Because we have tokens the first request will be RefreshState; force an empty response
    pc.SetRequestMutators({"Response:code=200,body=none"});
    res = pc.RefreshState({});
    ASSERT_FALSE(res) << static_cast<int>(*res);
    // Tokens should be unchanged
    ASSERT_EQ(auth_tokens, pc.user_data().GetAuthTokens());

    // NewTracker response with bad JSON
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // First request is NewTracker; force a response with bad JSON
    pc.SetRequestMutators({"BadJSON:200"});
    res = pc.RefreshState({});
    ASSERT_FALSE(res) << static_cast<int>(*res);
    // Should be no tokens
    ASSERT_EQ(pc.user_data().GetAuthTokens().size(), 0);
    ASSERT_EQ(pc.Balance(), 0);

    // RefreshState response with bad JSON
    pc.user_data().Clear();
    // Do an initial request to get Tracker tokens
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    auth_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(auth_tokens.size(), 3);
    // Because we have tokens the first request will be RefreshState; force a response with bad JSON
    pc.SetRequestMutators({"BadJSON:200"});
    res = pc.RefreshState({});
    ASSERT_FALSE(res) << static_cast<int>(*res);
    // Tokens should be unchanged
    ASSERT_EQ(auth_tokens, pc.user_data().GetAuthTokens());

    // 1 NewTracker response is 500 (retry succeeds)
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // First request is NewTracker;
    pc.SetRequestMutators({"Response:code=500"});
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success) << static_cast<int>(*res);
    ASSERT_GE(pc.user_data().GetAuthTokens().size(), 3);

    // 2 NewTracker responses are 500 (retry succeeds)
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // First request is NewTracker
    pc.SetRequestMutators({"Response:code=500", "Response:code=500"});
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success) << static_cast<int>(*res);
    ASSERT_GE(pc.user_data().GetAuthTokens().size(), 3);

    // 3 NewTracker responses are 500 (retry fails)
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // First request is NewTracker
    pc.SetRequestMutators({"Response:code=500", "Response:code=500", "Response:code=500"});
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::ServerError) << static_cast<int>(*res);
    // Should be no tokens
    ASSERT_EQ(pc.user_data().GetAuthTokens().size(), 0);
    ASSERT_EQ(pc.Balance(), 0);

    // 1 RefreshState response is 500 (retry succeeds)
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // Do an initial request to get Tracker tokens
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    auth_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(auth_tokens.size(), 3);
    // Because we have tokens the first request will be RefreshState
    pc.SetRequestMutators({"Response:code=500"});
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success) << static_cast<int>(*res);
    ASSERT_GE(pc.user_data().GetAuthTokens().size(), 3);

    // 2 RefreshState responses are 500 (retry succeeds)
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // Do an initial request to get Tracker tokens
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    auth_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(auth_tokens.size(), 3);
    // Because we have tokens the first request will be RefreshState
    pc.SetRequestMutators({"Response:code=500", "Response:code=500"});
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success) << static_cast<int>(*res);
    ASSERT_GE(pc.user_data().GetAuthTokens().size(), 3);

    // 3 RefreshState responses are 500 (retry fails)
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // Do an initial request to get Tracker tokens
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    auth_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(auth_tokens.size(), 3);
    // Because we have tokens the first request will be RefreshState
    pc.SetRequestMutators({"Response:code=500", "Response:code=500", "Response:code=500"});
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::ServerError) << static_cast<int>(*res);
    // Tokens should be unchanged
    ASSERT_EQ(auth_tokens, pc.user_data().GetAuthTokens());

    // RefreshState response with status code indicating invalid tokens
    pc.user_data().Clear();
    // Do an initial request to get Tracker tokens
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    auth_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(auth_tokens.size(), 3);
    // Because we have tokens the first request will be RefreshState
    pc.SetRequestMutators({"Response:code=401"});
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::InvalidTokens) << static_cast<int>(*res);
    // UserData should be cleared
    ASSERT_EQ(pc.user_data().GetAuthTokens().size(), 0);

    // NewTracker response with unknown status code
    // Blow away any existing tokens to force internal NewTracker.
    pc.user_data().Clear();
    // First request is NewTracker
    pc.SetRequestMutators({"Response:code=666"});
    res = pc.RefreshState({});
    ASSERT_FALSE(res) << static_cast<int>(*res);
    // Should be no tokens
    ASSERT_EQ(pc.user_data().GetAuthTokens().size(), 0);
    ASSERT_EQ(pc.Balance(), 0);

    // RefreshState response with unknown status code
    pc.user_data().Clear();
    // Do an initial request to get Tracker tokens
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    auth_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(auth_tokens.size(), 3);
    // Because we have tokens the first request will be RefreshState
    pc.SetRequestMutators({"Response:code=666"});
    res = pc.RefreshState({});
    ASSERT_FALSE(res) << static_cast<int>(*res);
    // Tokens should be unchanged
    ASSERT_EQ(auth_tokens, pc.user_data().GetAuthTokens());
}

TEST_F(TestPsiCash, NewExpiringPurchase) {
    PsiCashTester pc;
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), HTTPRequester, true);
    ASSERT_FALSE(err);

    // Simple success
    pc.user_data().Clear();
    auto refresh_result = pc.RefreshState({});
    ASSERT_TRUE(refresh_result) << refresh_result.error();
    ASSERT_EQ(*refresh_result, Status::Success);
    auto initial_balance = pc.Balance();
    err = MAKE_1T_REWARD(pc, 1);
    ASSERT_FALSE(err) << err;
    refresh_result = pc.RefreshState({});
    ASSERT_TRUE(refresh_result) << refresh_result.error();
    ASSERT_EQ(*refresh_result, Status::Success);
    ASSERT_EQ(pc.Balance(), initial_balance + ONE_TRILLION);
    // Note: this puchase will be valid for 1 second
    auto purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_SECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
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
    refresh_result = pc.RefreshState({});
    ASSERT_TRUE(refresh_result) << refresh_result.error();
    ASSERT_EQ(*refresh_result, Status::Success);
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
    refresh_result = pc.RefreshState({});
    ASSERT_TRUE(refresh_result) << refresh_result.error();
    ASSERT_EQ(*refresh_result, Status::Success);
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
}

TEST_F(TestPsiCash, NewExpiringPurchaseMutators) {
    PsiCashTester pc;
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), HTTPRequester, true);
    ASSERT_FALSE(err);

    // Failure: invalid tokens
    auto refresh_result = pc.RefreshState({});
    ASSERT_TRUE(refresh_result) << refresh_result.error();
    ASSERT_EQ(*refresh_result, Status::Success);
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
    ASSERT_TRUE(purchase_result);
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
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
    ASSERT_FALSE(err);

    // This isn't a "bad" result, exactly, but we'll force an error code and message.
    auto want_error_message = "my error message"s;
    HTTPResult errResult;
    errResult.code = HTTPResult::RECOVERABLE_ERROR;
    errResult.error = want_error_message;
    pc.SetHTTPRequestFn(FakeHTTPRequester(errResult));
    auto refresh_result = pc.RefreshState({});
    ASSERT_FALSE(refresh_result);
    ASSERT_NE(refresh_result.error().ToString().find(want_error_message), string::npos);
}
