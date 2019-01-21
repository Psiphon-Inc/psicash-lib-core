/*
 * Copyright (c) 2018, Psiphon Inc.
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <map>
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include "psicash.hpp"
#include "userdata.hpp"
#include "datetime.hpp"
#include "error.hpp"
#include "url.hpp"
#include "base64.hpp"
#include "utils.hpp"
#include "http_status_codes.h"

#include "vendor/nlohmann/json.hpp"

using json = nlohmann::json;

using namespace std;
using namespace nonstd;
using namespace psicash;
using namespace error;

namespace psicash {

const char* const kEarnerTokenType = "earner";
const char* const kSpenderTokenType = "spender";
const char* const kIndicatorTokenType = "indicator";
const char* const kAccountTokenType = "account";

const char* const kTransactionIDZero = "";

namespace prod {
static constexpr const char* kAPIServerScheme = "https";
static constexpr const char* kAPIServerHostname = "api.psi.cash";
static constexpr int kAPIServerPort = 443;
}
namespace dev {
static constexpr const char* kAPIServerScheme = "https";
static constexpr const char* kAPIServerHostname = "dev-api.psi.cash";
static constexpr int kAPIServerPort = 443;
/*
static constexpr const char* kAPIServerScheme = "http";
static constexpr const char* kAPIServerHostname = "localhost";
static constexpr int kAPIServerPort = 51337;
*/
}

static constexpr const char* kAPIServerVersion = "v1";
static const string kLandingPageParamKey = "psicash";
static constexpr const char* kMethodGET = "GET";
static constexpr const char* kMethodPOST = "POST";

//
// PsiCash class implementation
//

PsiCash::PsiCash()
        : server_port_(0), make_http_request_fn_(nullptr) {
}

PsiCash::~PsiCash() {
}

Error PsiCash::Init(const char* user_agent, const char* file_store_root,
              MakeHTTPRequestFn make_http_request_fn, bool test) {
    if (test) {
        server_scheme_ = dev::kAPIServerScheme;
        server_hostname_ = dev::kAPIServerHostname;
        server_port_ = dev::kAPIServerPort;
    } else {
        server_scheme_ = prod::kAPIServerScheme;
        server_hostname_ = prod::kAPIServerHostname;
        server_port_ = prod::kAPIServerPort;
    }

    if (!user_agent || (user_agent_ = user_agent).empty()) {
        return MakeError("user_agent is required");
    }

    if (!file_store_root) {
        return MakeError("file_store_root is required");
    }

    // May still be null.
    make_http_request_fn_ = make_http_request_fn;

    user_data_ = std::make_unique<UserData>();
    auto err = user_data_->Init(file_store_root);
    if (err) {
        // If UserData.Init fails, the only way to proceed is to try to reset it and create a new one.
        user_data_->Clear();
        err = user_data_->Init(file_store_root);
        if (err) {
            return PassError(err);
        }
    }

    return nullerr;
}

void PsiCash::SetHTTPRequestFn(MakeHTTPRequestFn make_http_request_fn) {
    make_http_request_fn_ = make_http_request_fn;
}

Error PsiCash::SetRequestMetadataItem(const string& key, const string& value) {
    return PassError(user_data_->SetRequestMetadataItem(key, value));
}

//
// Stored info accessors
//

TokenTypes PsiCash::ValidTokenTypes() const {
    TokenTypes tt;

    auto auth_tokens = user_data_->GetAuthTokens();
    for (const auto& it : auth_tokens) {
        tt.push_back(it.first);
    }

    return tt;
}

bool PsiCash::IsAccount() const {
    return user_data_->GetIsAccount();
}

int64_t PsiCash::Balance() const {
    return user_data_->GetBalance();
}

PurchasePrices PsiCash::GetPurchasePrices() const {
    return user_data_->GetPurchasePrices();
}

Purchases PsiCash::GetPurchases() const {
    return user_data_->GetPurchases();
}

static bool IsExpired(const Purchase& p) {
    // Note that "expired" is decided using local time.
    auto local_now = datetime::DateTime::Now();
    return (p.local_time_expiry && *p.local_time_expiry < local_now);
}

Purchases PsiCash::ValidPurchases() const {
    Purchases res;
    for (const auto& p : user_data_->GetPurchases()) {
        if (!IsExpired(p)) {
            res.push_back(p);
        }
    }
    return res;
}

optional<Purchase> PsiCash::NextExpiringPurchase() const {
    optional<Purchase> next;
    for (const auto& p : user_data_->GetPurchases()) {
        // We're using server time, since we're not comparing to local now (because we're
        // not checking to see if the purchase is expired -- just which expires next).
        if (!p.server_time_expiry) {
            continue;
        }

        if (!next) {
            // We haven't yet set a next.
            next = p;
            continue;
        }

        if (p.server_time_expiry < next->server_time_expiry) {
            next = p;
        }
    }

    return next;
}

Result<Purchases> PsiCash::ExpirePurchases() {
    auto all_purchases = GetPurchases();
    Purchases expired_purchases, valid_purchases;
    for (const auto& p : all_purchases) {
        if (IsExpired(p)) {
            expired_purchases.push_back(p);
        } else {
            valid_purchases.push_back(p);
        }
    }

    auto err = user_data_->SetPurchases(valid_purchases);
    if (err) {
        return WrapError(err, "SetPurchases failed");
    }

    return expired_purchases;
}

Error PsiCash::RemovePurchases(const vector<TransactionID>& ids) {
    auto all_purchases = GetPurchases();
    Purchases remaining_purchases;
    for (const auto& p : all_purchases) {
        bool match = false;
        for (const auto& id : ids) {
            if (p.id == id) {
                match = true;
                break;
            }
        }

        if (!match) {
            remaining_purchases.push_back(p);
        }
    }

    auto err = user_data_->SetPurchases(remaining_purchases);
    return WrapError(err, "SetPurchases failed");
}

Result<string> PsiCash::ModifyLandingPage(const string& url_string) const {
    URL url;
    auto err = url.Parse(url_string);
    if (err) {
        return WrapError(err, "url.Parse failed");
    }

    json psicash_data;
    psicash_data["v"] = 1;

    auto auth_tokens = user_data_->GetAuthTokens();
    if (auth_tokens.size() == 0 || auth_tokens.count(kEarnerTokenType) == 0) {
        psicash_data["tokens"] = nullptr;
    } else {
        psicash_data["tokens"] = auth_tokens[kEarnerTokenType];
    }

    // Get the metadata (sponsor ID, etc.)
    psicash_data["metadata"] = user_data_->GetRequestMetadata();

    string json_data;
    try {
        // The third argument is "ensure ASCII"
        json_data = psicash_data.dump(-1, ' ', true);
    }
    catch (json::exception& e) {
        return MakeError(
                utils::Stringer("json dump failed: ", e.what(), "; id:", e.id).c_str());
    }

    // Our preference is to put the our data into the URL's fragment/hash/anchor,
    // because we'd prefer the data not be sent to the server.
    // But if there already is a fragment value then we'll put our data into the query parameters.
    // (Because altering the fragment is more likely to have negative consequences
    // for the page than adding a query parameter that will be ignored.)

    if (url.fragment_.empty()) {
        url.fragment_ = kLandingPageParamKey + "=" + URL::Encode(json_data, true);
    } else {
        if (!url.query_.empty()) {
            url.query_ += "&";
        }
        url.query_ += kLandingPageParamKey + "=" + URL::Encode(json_data, true);
    }

    return url.ToString();
}

Result<string> PsiCash::GetRewardedActivityData() const {
    /*
     The data is base64-encoded JSON-serialized with this structure:
     {
         "v": 1,
         "tokens": "earner token",
         "metadata": {
             "client_region": "CA",
             "client_version": "123",
             "sponsor_id": "ABCDEFGH12345678",
             "propagation_channel_id": "ABCDEFGH12345678"
         },
         "user_agent": "PsiCash-iOS-Client"
     }
    */

    json psicash_data;
    psicash_data["v"] = 1;

    // Get the earner token. If we don't have one, the webhook can't succeed.
    auto auth_tokens = user_data_->GetAuthTokens();
    if (auth_tokens.size() == 0) {
        return MakeError("earner token missing; can't create webhoook data");
    } else {
        psicash_data["tokens"] = auth_tokens[kEarnerTokenType];
    }

    // Get the metadata (sponsor ID, etc.)
    psicash_data["metadata"] = user_data_->GetRequestMetadata();

    string json_data;
    try {
        json_data = psicash_data.dump(-1, ' ', true);
    }
    catch (json::exception& e) {
        return MakeError(
                utils::Stringer("json dump failed: ", e.what(), "; id:", e.id).c_str());
    }

    json_data = base64::B64Encode(json_data);

    return json_data;
}

json PsiCash::GetDiagnosticInfo() const {
    // NOTE: Do not put personal identifiers in this package.
    // TODO: This is still enough info to uniquely identify the user (combined with the
    // PsiCash DB). So maybe avoiding direct PII does not achieve anything, and we should
    // instead either include less data or make sure our retention of this data is
    // aggregated and/or short.

    json j = json::object();

    j["validTokenTypes"] = ValidTokenTypes();
    j["isAccount"] = IsAccount();
    j["balance"] = Balance();
    j["serverTimeDiff"] = user_data_->GetServerTimeDiff().count(); // in milliseconds
    j["purchasePrices"] = GetPurchasePrices();

    // Include a sanitized version of the purchases
    j["purchases"] = json::array();
    for (const auto& p : GetPurchases()) {
        j["purchases"].push_back({{"class",         p.transaction_class},
                                  {"distinguisher", p.distinguisher}});
    }

    return j;
}

//
// API Server Requests
//

// Simple helper to determine if a given HTTP response code should be considered a "server error".
inline bool IsServerError(int code) {
    return code >= 500 && code <= 599;
}

// Makes an HTTP request (with possible retries).
// HTTPResult.error will always be empty on a non-error return.
Result<HTTPResult> PsiCash::MakeHTTPRequestWithRetry(
        const std::string& method, const std::string& path, bool include_auth_tokens,
        const std::vector<std::pair<std::string, std::string>>& query_params) {
    if (!make_http_request_fn_) {
        throw std::runtime_error("make_http_request_fn_ must be set before requests are attempted");
    }

    const int max_attempts = 3;
    HTTPResult http_result;

    for (int i = 0; i < max_attempts; i++) {
        if (i > 0) {
            // Not the first attempt; wait before retrying
            this_thread::sleep_for(chrono::seconds(i));
        }

        auto req_params = BuildRequestParams(
            method, path, include_auth_tokens, query_params, i + 1, {});
        if (!req_params) {
            return WrapError(req_params.error(), "BuildRequestParams failed");
        }

        auto result_string = make_http_request_fn_(*req_params);
        if (result_string.empty()) {
            // An error so catastrophic that we didn't get any error info.
            return MakeError("HTTP request function returned no value");
        }

        try {
            auto j = json::parse(result_string);
            http_result.code = j["code"].get<int>();

            if (j["body"].is_string()) {
                http_result.body = j["body"].get<string>();
            }

            if (j["date"].is_string()) {
                http_result.date = j["date"].get<string>();
            }

            if (j["error"].is_string()) {
                http_result.error = j["error"].get<string>();
            }
        }
        catch (json::exception& e) {
            return MakeError(
                    utils::Stringer("json parse failed: ", e.what(), "; id:", e.id).c_str());
        }

        if (http_result.code == -1 && http_result.error.empty()) {
            return MakeError("HTTP result code is -1 but no error message provided");
        }

        // We just got a fresh server timestamp, so set the server time diff
        if (!http_result.date.empty()) {
            datetime::DateTime server_datetime;
            if (server_datetime.FromRFC7231(http_result.date)) {
                // We don't care about the return value at this point.
                (void)user_data_->SetServerTimeDiff(server_datetime);
            }
            // else: we're not going to raise the error
        }

        if (!http_result.error.empty()) {
            // Something happened that prevented the request from nominally succeeding. Don't retry.
            return MakeError(("Request resulted in error: "s + http_result.error).c_str());
        }

        if (IsServerError(http_result.code)) {
            // Server error; retry
            continue;
        }

        // We got a response of less than 500. We'll consider that success at this point.
        return http_result;
    }

    // We exceeded our retry limit. Return the last result received, which will be 500-ish.
    return http_result;
}

// Build the request paramters JSON appropriate for passing to make_http_request_fn_.
Result<string> PsiCash::BuildRequestParams(
        const std::string& method, const std::string& path, bool include_auth_tokens,
        const std::vector<std::pair<std::string, std::string>>& query_params, int attempt,
        const std::map<std::string, std::string>& additional_headers) const {
    json headers(additional_headers);
    headers["User-Agent"] = user_agent_;

    if (include_auth_tokens) {
        string s;
        for (const auto& at : user_data_->GetAuthTokens()) {
            if (!s.empty()) {
                s += ",";
            }
            s += at.second;
        }
        headers["X-PsiCash-Auth"] = s;
    }

    auto metadata = user_data_->GetRequestMetadata();
    metadata["attempt"] = attempt;

    try {
        headers["X-PsiCash-Metadata"] = metadata.dump(-1, ' ', true);
    }
    catch (json::exception& e) {
        return MakeError(
                utils::Stringer("metadata json dump failed: ", e.what(), "; id:", e.id).c_str());
    }

    json j = {
            {"scheme",   server_scheme_},
            {"hostname", server_hostname_},
            {"port",     server_port_},
            {"method",   method},
            {"path",     "/"s + kAPIServerVersion + path},
            {"query",    query_params},
            {"headers",  headers},
    };

    try {
        return j.dump(-1, ' ', true);
    }
    catch (json::exception& e) {
        return MakeError(
                utils::Stringer("params json dump failed: ", e.what(), "; id:", e.id).c_str());
    }
}

// Get new tracker tokens from the server. This effectively gives us a new identity.
Result<Status> PsiCash::NewTracker() {
    auto result = MakeHTTPRequestWithRetry(
            kMethodPOST,
            "/tracker",
            false,
            {}
    );
    if (!result) {
        return WrapError(result.error(), "MakeHTTPRequestWithRetry failed");
    }

    if (result->code == kHTTPStatusOK) {
        if (result->body.empty()) {
            return MakeError(
                    utils::Stringer("result has no body; code: ", result->code).c_str());
        }

        AuthTokens auth_tokens;
        try {
            auto j = json::parse(result->body);

            auth_tokens = j.get<AuthTokens>();
        }
        catch (json::exception& e) {
            return MakeError(
                    utils::Stringer("json parse failed: ", e.what(), "; id:", e.id).c_str());
        }

        // Sanity check
        if (auth_tokens.size() < 3) {
            return MakeError(
                    utils::Stringer("bad number of tokens received: ", auth_tokens.size()).c_str());
        }

        // Set our new data in a single write.
        UserData::WritePauser pauser(*user_data_);
        (void)user_data_->SetAuthTokens(auth_tokens, false);
        (void)user_data_->SetBalance(0);
        if (auto err = pauser.Unpause()) {
            return WrapError(err, "SetAuthTokens failed");
        }

        return Status::Success;
    } else if (IsServerError(result->code)) {
        return Status::ServerError;
    }

    return MakeError(utils::Stringer(
        "request returned unexpected result code: ", result->code).c_str());
}

Result<Status> PsiCash::RefreshState(const std::vector<std::string>& purchase_classes) {
    return RefreshState(purchase_classes, true);
}

// RefreshState helper that makes recursive calls (to allow for NewTracker and then
// RefreshState requests).
Result<Status> PsiCash::RefreshState(
    const std::vector<std::string>& purchase_classes, bool allow_recursion) {
    /*
     Logic flow overview:

     1. If there are no tokens:
        a. If isAccount then return. The user needs to log in immediately.
        b. If !isAccount then call NewTracker to get new tracker tokens.
     2. Make the RefreshClientState request.
     3. If isAccount then return. (Even if there are no valid tokens.)
     4. If there are valid (tracker) tokens then return.
     5. If there are no valid tokens call NewTracker. Call RefreshClientState again.
     6. If there are still no valid tokens, then things are horribly wrong. Return error.
    */

    auto auth_tokens = user_data_->GetAuthTokens();
    if (auth_tokens.empty()) {
        // No tokens.

        if (user_data_->GetIsAccount()) {
            // This is/was a logged-in account. We can't just get a new tracker.
            // The app will have to force a login for the user to do anything.
            return Status::Success;
        }

        if (!allow_recursion) {
            // We have already recursed and can't do it again. This is an error condition.
            // This is impossible-ish. It requires us to start out with no tokens, make a NewTracker
            // call that appears to succeed, but then _still_ have no tokens.
            return MakeError("failed to obtain valid tracker tokens (a)");
        }

        // Get new tracker tokens. (Which is effectively getting a new identity.)
        auto new_tracker_result = NewTracker();
        if (!new_tracker_result) {
            return WrapError(new_tracker_result.error(), "NewTracker failed");
        }

        if (*new_tracker_result != Status::Success) {
            return *new_tracker_result;
        }

        // Note: NewTracker calls SetAuthTokens and SetBalance.

        // Recursive RefreshState call now that we have tokens.
        return RefreshState(purchase_classes, false);
    }

    // We have tokens. Make the RefreshClientState request.

    vector<pair<string, string>> query_items;
    for (const auto& purchase_class : purchase_classes) {
        query_items.push_back({"class", purchase_class});
    }

    auto result = MakeHTTPRequestWithRetry(
            kMethodGET,
            "/refresh-state",
            true,
            query_items
    );
    if (!result) {
        return WrapError(result.error(), "MakeHTTPRequestWithRetry failed");
    }

    if (result->code == kHTTPStatusOK) {
        if (result->body.empty()) {
            return MakeError(
                    utils::Stringer("result has no body; code: ", result->code).c_str());
        }

        try {
            // We're going to be setting a bunch of UserData values, so let's wait until we're done
            // to write them all to disk.
            UserData::WritePauser pauser(*user_data_);

            auto j = json::parse(result->body);

            auto valid_token_types = j["TokensValid"].get<map<string, bool>>();
            user_data_->CullAuthTokens(valid_token_types);

            // If any of our tokens were valid, then the IsAccount value from the
            // server is authoritative. Otherwise we'll respect our existing value.
            if (!valid_token_types.empty() && j["IsAccount"].is_boolean()) {
                // If we have moved from being an account to not being an account,
                // something is very wrong.
                auto prev_is_account = IsAccount();
                auto is_account = j["IsAccount"].get<bool>();
                if (prev_is_account && !is_account) {
                    return MakeError("invalid is-account state");
                }

                user_data_->SetIsAccount(is_account);
            }

            if (j["Balance"].is_number_integer()) {
                user_data_->SetBalance(j["Balance"].get<int64_t>());
            }

            // We only try to use the PurchasePrices if we supplied purchase classes to the request
            if (!purchase_classes.empty() && j["PurchasePrices"].is_array()) {
                PurchasePrices purchase_prices;

                // The from_json for the PurchasePrice struct is for our internal (datastore and library API)
                // representation of PurchasePrice. We won't assume that the representation used by the
                // server is the same (nor that it won't change independent of our representation).
                for (size_t i = 0; i < j["PurchasePrices"].size(); i++) {
                    const auto& pp = j["PurchasePrices"][i];
                    purchase_prices.push_back(PurchasePrice{
                            pp["Class"].get<string>(),
                            pp["Distinguisher"].get<string>(),
                            pp["Price"].get<int64_t>()
                    });
                }

                user_data_->SetPurchasePrices(purchase_prices);
            }

            if (auto err = pauser.Unpause()) {
                return WrapError(err, "UserData write failed");
            }
        }
        catch (json::exception& e) {
            return MakeError(
                    utils::Stringer("json parse failed: ", e.what(), "; id:", e.id).c_str());
        }

        if (IsAccount()) {
            // For accounts there's nothing else we can do, regardless of the state of token validity.
            return Status::Success;
        }

        if (!ValidTokenTypes().empty()) {
            // We have a good tracker state.
            return Status::Success;
        }

        // We started out with tracker tokens, but they're all invalid.

        if (!allow_recursion) {
            return MakeError("failed to obtain valid tracker tokens (b)");
        }

        return RefreshState(purchase_classes, true);
    } else if (result->code == kHTTPStatusUnauthorized) {
        // This can only happen if the tokens we sent didn't all belong to same user.
        // This really should never happen.
        user_data_->Clear();
        return Status::InvalidTokens;
    } else if (IsServerError(result->code)) {
        return Status::ServerError;
    }

    return MakeError(utils::Stringer(
        "request returned unexpected result code: ", result->code).c_str());
}

Result<PsiCash::NewExpiringPurchaseResponse> PsiCash::NewExpiringPurchase(
        const string& transaction_class,
        const string& distinguisher,
        const int64_t expected_price) {
    auto result = MakeHTTPRequestWithRetry(
            kMethodPOST,
            "/transaction",
            true,
            {
                    {"class",          transaction_class},
                    {"distinguisher",  distinguisher},
                    // Note the conversion from positive to negative: price to amount.
                    {"expectedAmount", to_string(-expected_price)}
            }
    );
    if (!result) {
        return WrapError(result.error(), "MakeHTTPRequestWithRetry failed");
    }

    string transaction_id, authorization, transaction_type;
    datetime::DateTime server_expiry;

    // These statuses require the response body to be parsed
    if (result->code == kHTTPStatusOK ||
        result->code == kHTTPStatusTooManyRequests ||
        result->code == kHTTPStatusPaymentRequired ||
        result->code == kHTTPStatusConflict) {
        if (result->body.empty()) {
            return MakeError(
                    utils::Stringer("result has no body; code: ", result->code).c_str());
        }

        try {
            auto j = json::parse(result->body);

            // Many response fields are optional (depending on the presence of the indicator token)

            if (j["Balance"].is_number_integer()) {
                // We don't care about the return value of this right now
                (void)user_data_->SetBalance(j["Balance"].get<int64_t>());
            }

            if (j["TransactionID"].is_string()) {
                transaction_id = j["TransactionID"].get<string>();
            }

            if (j["Authorization"].is_string()) {
                authorization = j["Authorization"].get<string>();
            }

            if (j["TransactionResponse"]["Type"].is_string()) {
                transaction_type = j["TransactionResponse"]["Type"].get<string>();
            }

            if (j["TransactionResponse"]["Values"]["Expires"].is_string()) {
                string expiry_string = j["TransactionResponse"]["Values"]["Expires"].get<string>();
                if (!server_expiry.FromISO8601(expiry_string)) {
                    return MakeError(
                            ("failed to parse TransactionResponse.Values.Expires; got "s +
                             expiry_string).c_str());
                }
            }

            // Unused fields
            //auto transaction_amount = j.at("TransactionAmount").get<int64_t>();
        }
        catch (json::exception& e) {
            return MakeError(
                    utils::Stringer("json parse failed: ", e.what(), "; id:", e.id).c_str());
        }
    }

    if (result->code == kHTTPStatusOK) {
        if (transaction_type != "expiring-purchase") {
            return MakeError(
                    ("response contained incorrect TransactionResponse.Type; want 'expiring-purchase', got "s +
                     transaction_type).c_str());
        }
        if (transaction_id.empty()) {
            return MakeError("response did not provide valid TransactionID");
        }
        if (server_expiry.IsZero()) {
            // Purchase expiry is optional, but we're specifically making a New**Expiring**Purchase
            return MakeError(
                    "response did not provide valid TransactionResponse.Values.Expires");
        }
        // Not checking authorization, as it doesn't apply to all expiring purchases

        Purchase purchase = {
                transaction_id,
                transaction_class,
                distinguisher,
                server_expiry.IsZero() ? nullopt : make_optional(
                        server_expiry),
                server_expiry.IsZero() ? nullopt : make_optional(
                        server_expiry),
                authorization.empty() ? nullopt : make_optional(authorization)
        };

        user_data_->UpdatePurchaseLocalTimeExpiry(purchase);

        if (auto err = user_data_->AddPurchase(purchase)) {
            return WrapError(err, "AddPurchase failed");
        }

        return PsiCash::NewExpiringPurchaseResponse{
                Status::Success,
                purchase
        };
    } else if (result->code == kHTTPStatusTooManyRequests) {
        return PsiCash::NewExpiringPurchaseResponse{
                Status::ExistingTransaction
        };
    } else if (result->code == kHTTPStatusPaymentRequired) {
        return PsiCash::NewExpiringPurchaseResponse{
                Status::InsufficientBalance
        };
    } else if (result->code == kHTTPStatusConflict) {
        return PsiCash::NewExpiringPurchaseResponse{
                Status::TransactionAmountMismatch
        };
    } else if (result->code == kHTTPStatusNotFound) {
        return PsiCash::NewExpiringPurchaseResponse{
                Status::TransactionTypeNotFound
        };
    } else if (result->code == kHTTPStatusUnauthorized) {
        return PsiCash::NewExpiringPurchaseResponse{
                Status::InvalidTokens
        };
    } else if (IsServerError(result->code)) {
        return PsiCash::NewExpiringPurchaseResponse{
                Status::ServerError
        };
    }

    return MakeError(utils::Stringer(
        "request returned unexpected result code: ", result->code).c_str());
}


// Enable JSON de/serializing of PurchasePrice.
// See https://github.com/nlohmann/json#basic-usage
bool operator==(const PurchasePrice& lhs, const PurchasePrice& rhs) {
    return lhs.transaction_class == rhs.transaction_class &&
           lhs.distinguisher == rhs.distinguisher &&
           lhs.price == rhs.price;
}

void to_json(json& j, const PurchasePrice& pp) {
    j = json{
            {"class",         pp.transaction_class},
            {"distinguisher", pp.distinguisher},
            {"price",         pp.price}};
}

void from_json(const json& j, PurchasePrice& pp) {
    pp.transaction_class = j.at("class").get<string>();
    pp.distinguisher = j.at("distinguisher").get<string>();
    pp.price = j.at("price").get<int64_t>();
}

// Enable JSON de/serializing of Purchase.
// See https://github.com/nlohmann/json#basic-usage
bool operator==(const Purchase& lhs, const Purchase& rhs) {
    return lhs.transaction_class == rhs.transaction_class &&
           lhs.distinguisher == rhs.distinguisher &&
           lhs.server_time_expiry == rhs.server_time_expiry &&
           //lhs.local_time_expiry == rhs.local_time_expiry && // Don't include the derived local time in the comparison
           lhs.authorization == rhs.authorization;
}

void to_json(json& j, const Purchase& p) {
    j = json{
            {"id",            p.id},
            {"class",         p.transaction_class},
            {"distinguisher", p.distinguisher}};

    if (p.authorization) {
        j["authorization"] = *p.authorization;
    } else {
        j["authorization"] = nullptr;
    }

    if (p.server_time_expiry) {
        j["serverTimeExpiry"] = *p.server_time_expiry;
    } else {
        j["serverTimeExpiry"] = nullptr;
    }

    if (p.local_time_expiry) {
        j["localTimeExpiry"] = *p.local_time_expiry;
    } else {
        j["localTimeExpiry"] = nullptr;
    }
}

void from_json(const json& j, Purchase& p) {
    p.id = j.at("id").get<string>();
    p.transaction_class = j.at("class").get<string>();
    p.distinguisher = j.at("distinguisher").get<string>();

    if (j.at("authorization").is_null()) {
        p.authorization = nullopt;
    } else {
        p.authorization = j.at("authorization").get<string>();
    }

    if (j.at("serverTimeExpiry").is_null()) {
        p.server_time_expiry = nullopt;
    } else {
        p.server_time_expiry = j.at("serverTimeExpiry").get<datetime::DateTime>();
    }

    if (j.at("localTimeExpiry").is_null()) {
        p.local_time_expiry = nullopt;
    } else {
        p.local_time_expiry = j.at("localTimeExpiry").get<datetime::DateTime>();
    }
}

} // namespace psicash
