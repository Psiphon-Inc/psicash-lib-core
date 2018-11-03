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
#include <regex>
#include <thread>
using json = nlohmann::json;

using namespace std;
using namespace psicash;
using namespace testing;

class TestPsiCash : public ::testing::Test, public TempDir {
  public:
    TestPsiCash() : user_agent_("Psiphon-PsiCash-iOS") {
    }

    static string HTTPRequester(const string& params) {
        auto p = json::parse(params);

        stringstream curl;
        curl << "curl -s -i --max-time 5";
        curl << " -X " << p["method"].get<string>();

        auto headers = p["headers"];
        for (json::iterator it = headers.begin(); it != headers.end(); ++it) {
            curl << " -H \"" << it.key() << ":" << it.value().get<string>() << "\"";
        }

        curl << ' ';
        curl << '"' << p["scheme"].get<string>() << "://";
        curl << p["hostname"].get<string>() << ":" << p["port"].get<int>();
        curl << p["path"].get<string>();

        // query is an array of 2-tuple name-value arrays
        auto query = p["query"];
        for (int i = 0; i < query.size(); ++i) {
            if (i == 0) {
                curl << "?";
            } else {
                curl << "&";
            }

            curl << query[i][0].get<string>() << "=";
            if (query[i][1].is_string())
                curl << query[i][1].get<string>();
            else if (query[i][1].is_number_integer())
                curl << query[i][1].get<int64_t>();
            else if (query[i][1].is_boolean())
                curl << query[i][1].get<bool>();
        }
        curl << '"';

        auto command = curl.str();
        string output;
        auto code = exec(command.c_str(), output);
        if (code != 0) {
            return json({
              {"code", -1},
              {"error", output}
            }).dump();
        }

        std::stringstream ss(output);
        std::string line;

        json result = {{"code", -1}};
        string body;
        bool done_headers = false;
        while (std::getline(ss, line, '\n')) {
            line = trim(line);
            if (line.empty()) {
                done_headers = true;
            }

            if (!done_headers) {
                smatch match_pieces;

                // Look for HTTP status code value (200, etc.)
                regex status_regex("^HTTP\\/\\d\\S* (\\d\\d\\d).*$",
                                   regex_constants::ECMAScript | regex_constants::icase);
                if (regex_match(line, match_pieces, status_regex)) {
                    result["code"] = stoi(match_pieces[1].str());
                }

                // Look for the Date header
                regex date_regex("^Date: (.+)$",
                                 regex_constants::ECMAScript | regex_constants::icase);
                if (regex_match(line, match_pieces, date_regex)) {
                    result["date"] = match_pieces[1].str();
                }
            }

            if (done_headers) {
                body += line;
            }
        }

        result["body"] = body;

        auto result_string = result.dump();
        return result_string;
    }

    // Return a specific result for a HTTP request
    static psicash::MakeHTTPRequestFn FakeHTTPRequester(const string& result_json) {
        return [=](const string& params) -> string {
            return result_json;
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
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
    ASSERT_FALSE(err);

    auto v = pc.GetPurchases();
    ASSERT_EQ(v.size(), 0);

    v = pc.ValidPurchases();
    ASSERT_EQ(v.size(), 0);

    auto before_now = datetime::DateTime::Now().Sub(datetime::Duration(54321));
    auto after_now = datetime::DateTime::Now().Add(datetime::Duration(54321));

    Purchases ps = {{"id1", "tc1", "d1", before_now, nonstd::nullopt, "a1"},
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
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
    ASSERT_FALSE(err);

    auto v = pc.GetPurchases();
    ASSERT_EQ(v.size(), 0);

    auto p = pc.NextExpiringPurchase();
    ASSERT_FALSE(p);

    auto first = datetime::DateTime::Now().Sub(datetime::Duration(333));
    auto second = datetime::DateTime::Now().Sub(datetime::Duration(222));
    auto third = datetime::DateTime::Now().Sub(datetime::Duration(111));

    Purchases ps = {{"id1", "tc1", "d1", second, nonstd::nullopt, "a1"},
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
    ps = {{"id1", "tc1", "d1", nonstd::nullopt, nonstd::nullopt, "a1"},
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
    ps = {{"id1", "tc1", "d1", nonstd::nullopt, nonstd::nullopt, "a1"},
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
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
    ASSERT_FALSE(err);

    auto v = pc.GetPurchases();
    ASSERT_EQ(v.size(), 0);

    auto res = pc.ExpirePurchases();
    ASSERT_TRUE(res);
    ASSERT_EQ(res->size(), 0);

    auto before_now = datetime::DateTime::Now().Sub(datetime::Duration(54321));
    auto after_now = datetime::DateTime::Now().Add(datetime::Duration(54321));

    Purchases ps = {{"id1", "tc1", "d1", after_now, nonstd::nullopt, "a1"},
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
    vector<TransactionID> removeIDs = {ps[1].id, ps[3].id};
    Purchases remaining = {ps[0], ps[2]};

    err = pc.user_data().SetPurchases(ps);
    ASSERT_FALSE(err);

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), ps.size());
    ASSERT_EQ(v, ps);

    err = pc.RemovePurchases(removeIDs);
    ASSERT_FALSE(err);

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), remaining.size());
    ASSERT_EQ(v, remaining);

    // removeIDs are not present now
    err = pc.RemovePurchases(removeIDs);
    ASSERT_FALSE(err);

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), remaining.size());
    ASSERT_EQ(v, remaining);

    // empty array
    err = pc.RemovePurchases({});
    ASSERT_FALSE(err);

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), remaining.size());
    ASSERT_EQ(v, remaining);

    // totally fake IDs
    err = pc.RemovePurchases({"invalid1", "invalid2"});
    ASSERT_FALSE(err);

    v = pc.GetPurchases();
    ASSERT_EQ(v.size(), remaining.size());
    ASSERT_EQ(v, remaining);
}

TEST_F(TestPsiCash, ModifyLandingPage) {
    PsiCashTester pc;
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), nullptr, true);
    ASSERT_FALSE(err);

    const string key_part = "psicash=";
    URL url_in, url_out;

    //
    // No metadata set
    //

    url_in = {"https://asdf.sadf.gf", "", ""};
    auto res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.query_, url_in.query_);
    ASSERT_EQ(url_out.fragment_,
              key_part + URL::Encode("{\"metadata\":{},\"tokens\":null,\"v\":1}", true));

    url_in = {"https://asdf.sadf.gf", "gfaf=asdf", ""};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.query_, url_in.query_);
    ASSERT_EQ(url_out.fragment_,
              key_part + URL::Encode("{\"metadata\":{},\"tokens\":null,\"v\":1}", true));

    url_in = {"https://asdf.sadf.gf/asdfilj/adf", "gfaf=asdf", ""};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.query_, url_in.query_);
    ASSERT_EQ(url_out.fragment_,
              key_part + URL::Encode("{\"metadata\":{},\"tokens\":null,\"v\":1}", true));

    url_in = {"https://asdf.sadf.gf/asdfilj/adf.html", "gfaf=asdf", ""};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.query_, url_in.query_);
    ASSERT_EQ(url_out.fragment_,
              key_part + URL::Encode("{\"metadata\":{},\"tokens\":null,\"v\":1}", true));

    url_in = {"https://asdf.sadf.gf/asdfilj/adf.html", "", "regffd"};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.query_,
              key_part + URL::Encode("{\"metadata\":{},\"tokens\":null,\"v\":1}", true));
    ASSERT_EQ(url_out.fragment_, url_in.fragment_);

    url_in = {"https://asdf.sadf.gf/asdfilj/adf.html", "adfg=asdf&vfjnk=fadjn", "regffd"};
    res = pc.ModifyLandingPage(url_in.ToString());
    ASSERT_TRUE(res);
    url_out.Parse(*res);
    ASSERT_EQ(url_out.scheme_host_path_, url_in.scheme_host_path_);
    ASSERT_EQ(url_out.query_,
              url_in.query_ + "&" + key_part +
                      URL::Encode("{\"metadata\":{},\"tokens\":null,\"v\":1}", true));
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
    ASSERT_EQ(url_out.fragment_, key_part + URL::Encode("{\"metadata\":{\"k\":\"v\"},\"tokens\":"
                                                        "null,\"v\":1}",
                                                        true));

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
    ASSERT_EQ(url_out.fragment_, key_part + URL::Encode("{\"metadata\":{\"k\":\"v\"},\"tokens\":"
                                                        "\"kEarnerTokenType\",\"v\":1}",
                                                        true));

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
    ASSERT_EQ(url_out.fragment_, key_part + URL::Encode("{\"metadata\":{\"k\":\"v\"},\"tokens\":"
                                                        "null,\"v\":1}",
                                                        true));

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
    ASSERT_EQ(pc.Balance(), 0);
    ASSERT_GE(pc.GetPurchasePrices().size(), 2);

    // Test with existing tracker
    auto want_tokens = pc.user_data().GetAuthTokens();
    res = pc.RefreshState({"speed-boost"});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_GE(pc.ValidTokenTypes().size(), 3);
    ASSERT_EQ(pc.Balance(), 0);
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
    ASSERT_EQ(pc.Balance(), 0);
    ASSERT_GT(pc.GetPurchasePrices().size(), speed_boost_purchase_prices.size());

    // No purchase classes
    pc.user_data().Clear();
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    ASSERT_FALSE(pc.IsAccount());
    ASSERT_GE(pc.ValidTokenTypes().size(), 3);
    ASSERT_EQ(pc.Balance(), 0);
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
    ASSERT_EQ(pc.Balance(), 0);
    err = MAKE_1T_REWARD(pc, 1);
    ASSERT_FALSE(err) << err;
    res = pc.RefreshState({"speed-boost"}); // with class
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    ASSERT_EQ(pc.Balance(), ONE_TRILLION);

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
    ASSERT_EQ(pc.Balance(), 0);
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
    ASSERT_EQ(pc.Balance(), 0);

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
    err = MAKE_1T_REWARD(pc, 1);
    ASSERT_FALSE(err) << err;
    refresh_result = pc.RefreshState({});
    ASSERT_TRUE(refresh_result) << refresh_result.error();
    ASSERT_EQ(*refresh_result, Status::Success);
    ASSERT_EQ(pc.Balance(), ONE_TRILLION);
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
    ASSERT_NEAR(purchase_result->purchase->server_time_expiry->MillisSinceEpoch(), local_now.MillisSinceEpoch(), 5000);
    ASSERT_NEAR(purchase_result->purchase->local_time_expiry->MillisSinceEpoch(), local_now.MillisSinceEpoch(), 5000);
    // Check UserData -- purchase should still be valid
    ASSERT_EQ(pc.user_data().GetLastTransactionID(), purchase_result->purchase->id);
    auto purchases = pc.GetPurchases();
    ASSERT_EQ(purchases.size(), 1);
    ASSERT_EQ(purchases[0], purchase_result->purchase);
    purchases = pc.ValidPurchases();
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
    purchases = pc.ValidPurchases();
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
    err = MAKE_1T_REWARD(pc, 3);
    ASSERT_FALSE(err) << err;
    refresh_result = pc.RefreshState({});
    ASSERT_TRUE(refresh_result) << refresh_result.error();
    ASSERT_EQ(*refresh_result, Status::Success);
    ASSERT_EQ(pc.Balance(), 3*ONE_TRILLION);
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::Success) << static_cast<int>(purchase_result->status);
    ASSERT_EQ(pc.Balance(), 2*ONE_TRILLION);
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_MICROSECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::Success) << static_cast<int>(purchase_result->status);
    ASSERT_EQ(pc.Balance(), 1*ONE_TRILLION);
    purchase_result = pc.NewExpiringPurchase(TEST_DEBIT_TRANSACTION_CLASS, TEST_ONE_TRILLION_TEN_SECOND_DISTINGUISHER, ONE_TRILLION);
    ASSERT_TRUE(purchase_result);
    ASSERT_EQ(purchase_result->status, Status::Success);
    ASSERT_EQ(pc.Balance(), 0);
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
    purchases = pc.ValidPurchases();
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
    purchases = pc.ValidPurchases();
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
    ASSERT_EQ(pc.Balance(), 0);
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

    // Test the code path where the HTTP request helper returns an empty string (rather
    // than a proper error structure). Hopefully this never happens, but...
    pc.SetHTTPRequestFn(FakeHTTPRequester(""));
    auto refresh_result = pc.RefreshState({});
    ASSERT_FALSE(refresh_result);

    // The helper should never return bad JSON, but we'll test that code path with a
    // special helper.
    pc.SetHTTPRequestFn(FakeHTTPRequester("bad json"));
    refresh_result = pc.RefreshState({});
    ASSERT_FALSE(refresh_result);

    // This isn't a "bad" result, exactly, but we'll force an error code and message.
    auto want_error_message = "my error message"s;
    pc.SetHTTPRequestFn(FakeHTTPRequester(json({
        {"code", -1},
        {"error", want_error_message},
        {"body", nullptr},
        {"date", nullptr}
    }).dump()));
    refresh_result = pc.RefreshState({});
    ASSERT_FALSE(refresh_result);
    ASSERT_NE(refresh_result.error().ToString().find(want_error_message), string::npos);
}
