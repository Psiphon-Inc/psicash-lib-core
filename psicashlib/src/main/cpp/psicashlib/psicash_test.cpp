#include "SecretTestValues.h" // This file is in CipherShare
#include "base64.h"
#include "http_status_codes.h"
#include "nlohmann/json.hpp"
#include "psicash.h"
#include "test_helpers.h"
#include "url.h"
#include "userdata.h"
#include "gtest/gtest.h"
#include <regex>
#include <thread>
using json = nlohmann::json;

using namespace std;
using namespace psicash;

// Making this a global rather than PsiCashTester member, because it needs to be modified
// inside a const member. (Tests are not multithreaded, so this is okay.)
static std::vector<std::string> g_request_mutators;

class TestPsiCash : public ::testing::Test, public TempDir {
  public:
    TestPsiCash() : user_agent_("Psiphon-PsiCash-iOS") {
      g_request_mutators.clear();
    }

    static string HTTPRequester(const string& params) {
        auto p = json::parse(params);

        stringstream curl;
        curl << "curl -s -i";
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
        auto res = exec(command.c_str());

        std::stringstream ss(res);
        std::string line;

        json result = {{"staus", -1}};
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
                    result["status"] = stoi(match_pieces[1].str());
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

    const char* user_agent_;
};

// Subclass psicash::PsiCash to get access to private members for testing.
// This would probably be done more cleanly with dependency injection, but that
// adds a bunch of overhead for little gain.
class PsiCashTester : public psicash::PsiCash {
  public:
    virtual ~PsiCashTester() {}

    UserData& user_data() { return *user_data_; }

    error::Error MakeRewardRequests(int trillions) {
        for (int i = 0; i < trillions; ++i) {
            if (i != 0) {
                // Sleep a bit to avoid server DB transaction conflicts
                this_thread::sleep_for(chrono::milliseconds(100));
            }

            auto result = MakeHTTPRequestWithRetry(
                    "POST", "/transaction", true,
                    {{"class", TEST_CREDIT_TRANSACTION_CLASS},
                     {"distinguisher", TEST_ONE_TRILLION_ONE_MICROSECOND_DISTINGUISHER}});
            if (!result) {
                return WrapError(result.error(), "MakeHTTPRequestWithRetry failed");
            } else if (result->status != kHTTPStatusOK) {
                return MakeError(utils::Stringer("1T reward request failed: ", result->status, "; ",
                                                 result->error, "; ", result->body));
            }
        }
        return error::nullerr;
    }

    virtual error::Result<string>
    BuildRequestParams(const std::string& method, const std::string& path, bool include_auth_tokens,
                       const std::vector<std::pair<std::string, std::string>>& query_params,
                       int attempt,
                       const std::map<std::string, std::string>& additional_headers) const {
        auto bonus_headers = additional_headers;
        if (!g_request_mutators.empty()) {
            bonus_headers[TEST_HEADER] = g_request_mutators.back();
            g_request_mutators.pop_back();
        }

        return PsiCash::BuildRequestParams(method, path, include_auth_tokens,
                                           query_params, attempt, bonus_headers);
    }

    void SetRequestMutators(const std::vector<std::string>& mutators) {
      // We're going to store it reversed so we can pop off the end.
      g_request_mutators.assign(mutators.crbegin(), mutators.crend());
    }
};

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
    {
        // Datastore directory that will not work
        auto bad_dir = GetTempDir() + "/a/b/c/d/f/g";
        PsiCashTester pc;
        auto err = pc.Init(user_agent_, bad_dir.c_str(), nullptr, true);
        // This occasionally fails to fail, and I don't know why
        ASSERT_TRUE(err) << bad_dir;
    }
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
    err = pc.MakeRewardRequests(1);
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
    ASSERT_EQ(pc.GetPurchasePrices().size(), 0); // shouldn't get any, because no valid indicator token

    // Tracker with invalid tokens
    pc.user_data().Clear();
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    ASSERT_EQ(*res, Status::Success);
    auto prev_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(prev_tokens.size(), 3);
    // We have tokens; force the server to consider them invalid
    pc.SetRequestMutators({"InvalidTokens"});
    res = pc.RefreshState({});
    ASSERT_TRUE(res) << res.error();
    // We should have brand new tokens now.
    auto next_tokens = pc.user_data().GetAuthTokens();
    ASSERT_GE(next_tokens.size(), 3);
    ASSERT_NE(prev_tokens, next_tokens);

    // Tracker with invalid tokens
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
}

TEST_F(TestPsiCash, NewExpiringPurchase) {
    PsiCashTester pc;
    auto err = pc.Init(user_agent_, GetTempDir().c_str(), HTTPRequester, true);
    ASSERT_FALSE(err);

    auto res = pc.NewExpiringPurchase("asdf", "adf", 100);
    ASSERT_TRUE(res);
    ASSERT_EQ(res->status, Status::TransactionTypeNotFound);
}