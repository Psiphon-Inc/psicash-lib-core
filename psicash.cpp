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
#include <algorithm>
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

const char* const kTransactionIDZero = "";

namespace prod {
static constexpr const char* kAPIServerScheme = "https";
static constexpr const char* kAPIServerHostname = "api.psi.cash";
static constexpr int kAPIServerPort = 443;
}
namespace dev {
#define LOCAL_TEST 0
#if LOCAL_TEST
static constexpr const char* kAPIServerScheme = "http";
static constexpr const char* kAPIServerHostname = "localhost";
static constexpr int kAPIServerPort = 51337;
#else
static constexpr const char* kAPIServerScheme = "https";
static constexpr const char* kAPIServerHostname = "api.dev.psi.cash";
static constexpr int kAPIServerPort = 443;
#endif
}

static constexpr const char* kAPIServerVersion = "v1";
static constexpr const char* kLandingPageParamKey = "psicash";
static constexpr const char* kMethodGET = "GET";
static constexpr const char* kMethodPOST = "POST";

static constexpr const char* kDateHeaderKey = "Date";

//
// PsiCash class implementation
//

// Definitions of static member variables
constexpr int HTTPResult::CRITICAL_ERROR;
constexpr int HTTPResult::RECOVERABLE_ERROR;

PsiCash::PsiCash()
        : test_(false),
          initialized_(false),
          server_port_(0),
          user_data_(std::make_unique<UserData>()),
          make_http_request_fn_(nullptr) {
}

PsiCash::~PsiCash() {
}

Error PsiCash::Init(const string& user_agent, const string& file_store_root,
                    MakeHTTPRequestFn make_http_request_fn, bool force_reset,
                    bool test) {
    test_ = test;
    if (test) {
        server_scheme_ = dev::kAPIServerScheme;
        server_hostname_ = dev::kAPIServerHostname;
        server_port_ = dev::kAPIServerPort;
    } else {
        server_scheme_ = prod::kAPIServerScheme;
        server_hostname_ = prod::kAPIServerHostname;
        server_port_ = prod::kAPIServerPort;
    }

    if (user_agent.empty()) {
        return MakeCriticalError("user_agent is required");
    }
    user_agent_ = user_agent;

    if (file_store_root.empty()) {
        return MakeCriticalError("file_store_root is required");
    }

    if (force_reset) {
        user_data_->Clear(file_store_root, test);
    }

    // May still be null.
    make_http_request_fn_ = std::move(make_http_request_fn);

    if (auto err = user_data_->Init(file_store_root, test)) {
        return PassError(err);
    }

    initialized_ = true;
    return error::nullerr;
}

#define MUST_BE_INITIALIZED     if (!Initialized()) { return MakeCriticalError("PsiCash is uninitialized"); }

bool PsiCash::Initialized() const {
    return initialized_;
}

Error PsiCash::ResetUser() {
    return PassError(user_data_->DeleteUserData(/*is_logged_out_account=*/false));
}

Error PsiCash::MigrateTrackerTokens(const map<string, string>& tokens) {
    MUST_BE_INITIALIZED;

    AuthTokens auth_tokens;
    for (const auto& it : tokens) {
        auth_tokens[it.first].id = it.second;
        // leave expiry null
    }

    UserData::Transaction transaction(*user_data_);
    // Ignoring return values while writing is paused.
    // Blow away any user state, as the newly migrated tokens are overwriting it.
    (void)ResetUser();
    (void)user_data_->SetAuthTokens(auth_tokens, /*is_account=*/false, /*account_username=*/"");
    if (auto err = transaction.Commit()) {
        return WrapError(err, "user data write failed");
    }
    return nullerr;
}

void PsiCash::SetHTTPRequestFn(MakeHTTPRequestFn make_http_request_fn) {
    make_http_request_fn_ = std::move(make_http_request_fn);
}

Error PsiCash::SetRequestMetadataItem(const string& key, const string& value) {
    MUST_BE_INITIALIZED;
    return PassError(user_data_->SetRequestMetadataItem(key, value));
}

Error PsiCash::SetLocale(const string& locale) {
    MUST_BE_INITIALIZED;
    return PassError(user_data_->SetLocale(locale));
}

//
// Stored info accessors
//

bool PsiCash::HasTokens() const {
    MUST_BE_INITIALIZED;

    // Trackers and Accounts both require the same token types (for now).
    // (Accounts will also have the "logout" type, but it isn't strictly needed for sane operation.)
    vector<string> required_token_types = {kEarnerTokenType, kSpenderTokenType, kIndicatorTokenType};
    auto auth_tokens = user_data_->GetAuthTokens();
    for (const auto& it : auth_tokens) {
        auto found = std::find(required_token_types.begin(), required_token_types.end(), it.first);
        if (found != required_token_types.end()) {
            required_token_types.erase(found);
        }
    }

    return required_token_types.empty();
}

/// If the user has no tokens, most actions are disallowed. (This can include being in
/// the is-logged-out-account state.)
#define TOKENS_REQUIRED     if (!HasTokens()) { return MakeCriticalError("user has insufficient tokens"); }

bool PsiCash::IsAccount() const {
    if (user_data_->GetIsLoggedOutAccount()) {
        return true;
    }
    return user_data_->GetIsAccount();
}

nonstd::optional<std::string> PsiCash::AccountUsername() const {
    if (user_data_->GetIsLoggedOutAccount() || !user_data_->GetIsAccount()) {
        return nullopt;
    }
    return user_data_->GetAccountUsername();
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

Purchases PsiCash::ActivePurchases() const {
    Purchases res;
    for (const auto& p : user_data_->GetPurchases()) {
        if (!IsExpired(p)) {
            res.push_back(p);
        }
    }
    return res;
}

Authorizations PsiCash::GetAuthorizations(bool activeOnly/*=false*/) const {
    Authorizations res;
    for (const auto& p : user_data_->GetPurchases()) {
        if (p.authorization && (!activeOnly || !IsExpired(p))) {
            res.push_back(*p.authorization);
        }
    }
    return res;
}

Purchases PsiCash::GetPurchasesByAuthorizationID(std::vector<std::string> authorization_ids) const {
    auto purchases = user_data_->GetPurchases();

    auto new_end = std::remove_if(purchases.begin(), purchases.end(), [&authorization_ids](const Purchase& p){
        return !p.authorization
            || std::find(authorization_ids.begin(), authorization_ids.end(), p.authorization->id) == authorization_ids.end();
    });

    purchases.erase(new_end, purchases.end());

    return purchases;
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

error::Result<Purchases> PsiCash::RemovePurchases(const vector<TransactionID>& ids) {
    auto all_purchases = GetPurchases();
    Purchases remaining_purchases, removed_purchases;
    for (const auto& p : all_purchases) {
        bool match = false;
        for (const auto& id : ids) {
            if (p.id == id) {
                match = true;
                break;
            }
        }

        if (match) {
            removed_purchases.push_back(p);
        }
        else {
            remaining_purchases.push_back(p);
        }
    }

    auto err = user_data_->SetPurchases(remaining_purchases);
    if (err) {
        return WrapError(err, "SetPurchases failed");
    }

    return removed_purchases;
}

/// Adds a params package to the URL which includes the user's earner token (if there is one).
/// @param query_param_only If true, the params will only be added to the query parameters
///     part of the URL, rather than first attempting to add it to the hash/fragment.
Result<string> PsiCash::AddEarnerTokenToURL(const string& url_string, bool query_param_only) const {
    URL url;
    auto err = url.Parse(url_string);
    if (err) {
        return WrapError(err, "url.Parse failed");
    }

    json psicash_data;
    psicash_data["v"] = 1;
    psicash_data["timestamp"] = datetime::DateTime::Now().ToISO8601();

    auto auth_tokens = user_data_->GetAuthTokens();
    if (auth_tokens.count(kEarnerTokenType) == 0) {
        psicash_data["tokens"] = nullptr;
    } else {
        psicash_data["tokens"] = CommaDelimitTokens({kEarnerTokenType});
    }

    if (test_) {
        psicash_data["dev"] = 1;
        psicash_data["debug"] = 1;
    }

    // Get the metadata (sponsor ID, etc.)
    psicash_data["metadata"] = GetRequestMetadata(0);

    string json_data;
    try {
        json_data = psicash_data.dump(-1, ' ',  // disable indent
                                      true);    // ensure ASCII
    }
    catch (json::exception& e) {
        return MakeCriticalError(
                utils::Stringer("json dump failed: ", e.what(), "; id:", e.id));
    }

    // Base64-encode the JSON
    auto encoded_json = URL::Encode(base64::TrimPadding(base64::B64Encode(json_data)), false);

    // Our preference is to put the our data into the URL's fragment/hash/anchor,
    // because we'd prefer the data not be sent to the server nor included in the referrer
    // header to third-party page resources.
    // But if there already is a fragment value then we'll put our data into the query parameters.
    // (Because altering the fragment is more likely to have negative consequences
    // for the page than adding a query parameter that will be ignored.)

    if (!query_param_only && url.fragment_.empty()) {
        // When setting in the fragment, we use "#!psicash=etc". The ! prevents the
        // fragment from accidentally functioning as a jump-to anchor on a landing page
        // (where we don't control element IDs, etc.).
        url.fragment_ = "!"s + kLandingPageParamKey + "=" + encoded_json;
    } else {
        if (!url.query_.empty()) {
            url.query_ += "&";
        }
        url.query_ += kLandingPageParamKey + "="s + encoded_json;
    }

    return url.ToString();
}

Result<string> PsiCash::ModifyLandingPage(const string& url_string) const {
    // All of our landing pages are arrived at via the redirector service we run. We want
    // to send our token package to the redirector, so that it can decide if and how to
    // include it in the final site URL. So we have to send it via a query parameter.
    return AddEarnerTokenToURL(url_string, true);
}

Result<string> PsiCash::GetBuyPsiURL() const {
    TOKENS_REQUIRED;
    return AddEarnerTokenToURL(test_ ? "https://dev-psicash.myshopify.com/" : "https://buy.psi.cash/", false);
}

std::string PsiCash::GetUserSiteURL(UserSiteURLType url_type, bool webview) const {
    URL url;
    url.scheme_host_path_ = test_ ? "https://dev-my.psi.cash" : "https://my.psi.cash";

    switch (url_type) {
    case UserSiteURLType::AccountSignup:
        url.scheme_host_path_ += "/signup";
        break;

    case UserSiteURLType::ForgotAccount:
        url.scheme_host_path_ += "/forgot";
        break;

    case UserSiteURLType::AccountManagement:
    default:
        // Just the root domain
        break;
    }

    url.query_ = "utm_source=" + URL::Encode(user_agent_, false);
    url.query_ += "&locale=" + URL::Encode(user_data_->GetLocale(), false);

    if (!user_data_->GetAccountUsername().empty()) {
        auto encoded_username = URL::Encode(user_data_->GetAccountUsername(), false);
        // IE has a URL limit of 2083 characters, so if the username is too long (or encodes
        // to too long), then we're going to omit this parameter). It is better to omit the
        // username than to pre-fill an incorrect username or have broken UTF-8 characters.
        if (encoded_username.length() < 2000) {
            url.query_ += "&username=" + encoded_username;
        }
    }

    if (webview) {
        url.query_ += "&webview=true";
    }

    return url.ToString();
}

Result<string> PsiCash::GetRewardedActivityData() const {
    TOKENS_REQUIRED;

    json psicash_data;
    psicash_data["v"] = 1;

    // Get the earner token. If we don't have one, the webhook can't succeed.
    auto auth_tokens = user_data_->GetAuthTokens();
    if (auth_tokens.empty()) {
        return MakeCriticalError("earner token missing; can't create webhoook data");
    } else {
        psicash_data["tokens"] = auth_tokens[kEarnerTokenType].id;
    }

    // Get the metadata (sponsor ID, etc.)
    psicash_data["metadata"] = GetRequestMetadata(0);

    string json_data;
    try {
        json_data = psicash_data.dump(-1, ' ', true);
    }
    catch (json::exception& e) {
        return MakeCriticalError(
                utils::Stringer("json dump failed: ", e.what(), "; id:", e.id));
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

    j["test"] = test_;
    j["isLoggedOutAccount"] = user_data_->GetIsLoggedOutAccount();
    j["validTokenTypes"] = user_data_->ValidTokenTypes();
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

// Creates the metadata JSON that should be included with requests.
// This method MUST be called rather than calling UserData::GetRequestMetdata directly.
// If `attempt` is 0 it will be omitted from the metadata object.
json PsiCash::GetRequestMetadata(int attempt) const {
    auto req_metadata = user_data_->GetRequestMetadata();
    // UserData stores only the explicitly set metadata. We have more fields to add at this level.
    req_metadata["v"] = 1;
    req_metadata["user_agent"] = user_agent_;
    if (attempt > 0) {
        req_metadata["attempt"] = attempt;
    }
    return req_metadata;
}

// Makes an HTTP request (with possible retries).
// HTTPResult.error will always be empty on a non-error return.
Result<HTTPResult> PsiCash::MakeHTTPRequestWithRetry(
        const std::string& method, const std::string& path, bool include_auth_tokens,
        const std::vector<std::pair<std::string, std::string>>& query_params,
        const optional<json>& body)
{
    MUST_BE_INITIALIZED;

    if (!make_http_request_fn_) {
        throw std::runtime_error("make_http_request_fn_ must be set before requests are attempted");
    }

    string body_string;
    if (body) {
        try {
            body_string = body->dump(-1, ' ', true);
        }
        catch (json::exception& e) {
            return MakeCriticalError(
                    utils::Stringer("body json dump failed: ", e.what(), "; id:", e.id));
        }
    }

    const int max_attempts = 3;
    HTTPResult http_result;

    for (int i = 0; i < max_attempts; i++) {
        if (i > 0) {
            // Not the first attempt; wait before retrying
            this_thread::sleep_for(chrono::seconds(i));
        }

        auto req_params = BuildRequestParams(
            method, path, include_auth_tokens, query_params, i + 1, {}, body_string);
        if (!req_params) {
            return WrapError(req_params.error(), "BuildRequestParams failed");
        }


        http_result = make_http_request_fn_(*req_params);

        // Error state sanity check
        if (http_result.code < 0 && http_result.error.empty()) {
            return MakeCriticalError("HTTP result code is negative but no error message provided");
        }

        // We just got a fresh server timestamp, so set the server time diff
        auto date_header = utils::FindHeaderValue(http_result.headers, kDateHeaderKey);
        if (!date_header.empty()) {
            datetime::DateTime server_datetime;
            if (server_datetime.FromRFC7231(date_header)) {
                // We don't care about the return value at this point.
                (void)user_data_->SetServerTimeDiff(server_datetime);
            }
            // else: we're not going to raise the error
        }

        if (http_result.code < 0) {
            // Something happened that prevented the request from nominally succeeding.
            // If the native code indicates that this is a "recoverable error" (such as
            // the network interruption error we see on iOS sometimes), then we will retry.
            if (http_result.code == HTTPResult::RECOVERABLE_ERROR) {
                continue;
            }

            // Unrecoverable error; don't retry.
            return MakeCriticalError("Request resulted in critical error: "s + http_result.error);
        }

        if (IsServerError(http_result.code)) {
            // Server error; retry
            continue;
        }

        // We got a response of less than 500. We'll consider that success at this point.
        return http_result;
    }

    // We exceeded our retry limit.

    if (http_result.code < 0) {
        // A critical error would have returned above, so this is a non-critical error
        return MakeNoncriticalError("Request resulted in noncritical error: "s + http_result.error);
    }

    // Return the last result (which is a 5xx server error)
    return http_result;
}

// Build the request parameters JSON appropriate for passing to make_http_request_fn_.
Result<HTTPParams> PsiCash::BuildRequestParams(
        const std::string& method, const std::string& path, bool include_auth_tokens,
        const std::vector<std::pair<std::string, std::string>>& query_params, int attempt,
        const std::map<std::string, std::string>& additional_headers,
        const std::string& body) const {

    HTTPParams params;

    params.scheme = server_scheme_;
    params.hostname = server_hostname_;
    params.port = server_port_;
    params.method = method;
    params.path = "/"s + kAPIServerVersion + path;
    params.query = query_params;

    params.headers = additional_headers;
    params.headers["Accept"] = "application/json";
    params.headers["User-Agent"] = user_agent_;

    if (include_auth_tokens) {
        params.headers["X-PsiCash-Auth"] = CommaDelimitTokens({});
    }

    auto metadata = GetRequestMetadata(attempt);

    try {
        params.headers["X-PsiCash-Metadata"] = metadata.dump(-1, ' ', true);
    }
    catch (json::exception& e) {
        return MakeCriticalError(
                utils::Stringer("metadata json dump failed: ", e.what(), "; id:", e.id));
    }

    params.body = body;
    if (!body.empty()) {
        params.headers["Content-Type"] = "application/json; charset=utf-8";
    }

    return params;
}

/// Returns our auth tokens in comma-delimited format. If types is `{}`, all tokens will
/// be included; otherwise only tokens of the types specified will be included.
std::string PsiCash::CommaDelimitTokens(const std::vector<std::string>& types) const {
    vector<string> tokens;
    for (const auto& at : user_data_->GetAuthTokens()) {
        if (types.empty() || std::find(types.begin(), types.end(), at.first) != types.end()) {
            tokens.push_back(at.second.id);
        }
    }
    return utils::Join(tokens, ",");
}

// Get new tracker tokens from the server. This effectively gives us a new identity.
Result<Status> PsiCash::NewTracker() {
    MUST_BE_INITIALIZED;

    auto result = MakeHTTPRequestWithRetry(
            kMethodPOST,
            "/tracker",
            false,
            {{"instanceID", user_data_->GetInstanceID()}},
            nullopt // body
    );
    if (!result) {
        return WrapError(result.error(), "MakeHTTPRequestWithRetry failed");
    }

    if (result->code == kHTTPStatusOK) {
        if (result->body.empty()) {
            return MakeCriticalError(
                    utils::Stringer("result has no body; code: ", result->code));
        }

        AuthTokens auth_tokens;
        try {
            auto j = json::parse(result->body);

            auth_tokens = j.get<AuthTokens>();
        }
        catch (json::exception& e) {
            return MakeCriticalError(
                    utils::Stringer("json parse failed: ", e.what(), "; id:", e.id));
        }

        // Sanity check
        if (auth_tokens.size() < 3) {
            return MakeCriticalError(
                    utils::Stringer("bad number of tokens received: ", auth_tokens.size()));
        }

        // Set our new data in a single write.
        UserData::Transaction transaction(*user_data_);
        (void)user_data_->SetIsLoggedOutAccount(false);
        (void)user_data_->SetAuthTokens(auth_tokens, /*is_account=*/false, /*account_username=*/"");
        (void)user_data_->SetBalance(0);
        if (auto err = transaction.Commit()) {
            return WrapError(err, "user data write failed");
        }

        return Status::Success;
    } else if (IsServerError(result->code)) {
        return Status::ServerError;
    }

    return MakeCriticalError(utils::Stringer(
            "request returned unexpected result code: ", result->code, "; ",
            result->body, "; ", json(result->headers).dump()));
}

Result<PsiCash::RefreshStateResponse> PsiCash::RefreshState(bool local_only, const std::vector<std::string>& purchase_classes) {
    if (local_only) {
        // Our "local only" refresh involves checking tokens for expiry and potentially
        // shifting into a logged-out state.

        // This call is offline, but we might be currently connected, so the reconnect_required
        // considerations still apply.
        bool reconnect_required = false;

        auto local_now = datetime::DateTime::Now();
        for (const auto& it : user_data_->GetAuthTokens()) {
            if (it.second.server_time_expiry
                && user_data_->ServerTimeToLocal(*it.second.server_time_expiry) < local_now) {
                    // If any tokens are expired, we consider ourselves to not have a proper set

                    // If we're transitioning to a logged out state and there are active
                    // authorizations (applied to the current tunnel), then we need to
                    // reconnect to remove them.
                    // TODO: this line/logic is duplicated below; consider a helper to encapsulate
                    reconnect_required = !GetAuthorizations(true).empty();

                    if (auto err = user_data_->DeleteUserData(IsAccount())) {
                        return WrapError(err, "DeleteUserData failed");
                    }

                    break;
            }
        }

        return PsiCash::RefreshStateResponse{ Status::Success, reconnect_required };
    }

    return RefreshState(purchase_classes, true);
}

// RefreshState helper that makes recursive calls (to allow for NewTracker and then
// RefreshState requests).
Result<PsiCash::RefreshStateResponse> PsiCash::RefreshState(
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

    MUST_BE_INITIALIZED;

    auto auth_tokens = user_data_->GetAuthTokens();
    if (auth_tokens.empty()) {
        // No tokens.
        if (IsAccount()) {
            // This is a logged-in or logged-out account. We can't just get a new tracker.
            // The app will have to force a login for the user to do anything.
            return PsiCash::RefreshStateResponse{ Status::Success, false };
        }

        if (!allow_recursion) {
            // We have already recursed and can't do it again. This is an error condition.
            // This is impossible-ish. It requires us to start out with no tokens, make a NewTracker
            // call that appears to succeed, but then _still_ have no tokens.
            return MakeCriticalError("failed to obtain valid tracker tokens (a)");
        }

        // Get new tracker tokens. (Which is effectively getting a new identity.)
        auto new_tracker_result = NewTracker();
        if (!new_tracker_result) {
            return WrapError(new_tracker_result.error(), "NewTracker failed");
        }

        if (*new_tracker_result != Status::Success) {
            return PsiCash::RefreshStateResponse{ *new_tracker_result, false };
        }

        // Note: NewTracker calls SetAuthTokens and SetBalance.

        // Recursive RefreshState call now that we have tokens.
        return RefreshState(purchase_classes, false);
    }

    // We have tokens. Make the RefreshClientState request.

    vector<pair<string, string>> query_items;
    for (const auto& purchase_class : purchase_classes) {
        query_items.emplace_back("class", purchase_class);
    }

    // If LastTransactionID is empty, we'll get all transactions.
    query_items.emplace_back("lastTransactionID", user_data_->GetLastTransactionID());

    auto result = MakeHTTPRequestWithRetry(
            kMethodGET,
            "/refresh-state",
            true,
            query_items,
            nullopt // body
    );
    if (!result) {
        return WrapError(result.error(), "MakeHTTPRequestWithRetry failed");
    }

    if (result->code == kHTTPStatusOK) {
        if (result->body.empty()) {
            return MakeCriticalError(
                    utils::Stringer("result has no body; code: ", result->code));
        }

        bool reconnect_required = false;

        try {
            // We're going to be setting a bunch of UserData values, so let's wait until we're done
            // to write them all to disk.
            UserData::Transaction transaction(*user_data_);

            auto j = json::parse(result->body);

            auto valid_token_types = j["TokensValid"].get<map<string, bool>>();
            (void)user_data_->CullAuthTokens(valid_token_types);

            // If any of our tokens were valid, then the IsAccount value from the
            // server is authoritative. Otherwise we'll respect our existing value.
            bool any_valid_token = false;
            for (const auto& vtt : valid_token_types) {
                if (vtt.second) {
                    any_valid_token = true;
                    break;
                }
            }
            if (any_valid_token && j["IsAccount"].is_boolean()) {
                // If we have moved from being an account to not being an account,
                // something is very wrong.
                auto prev_is_account = IsAccount();
                auto is_account = j["IsAccount"].get<bool>();
                if (prev_is_account && !is_account) {
                    return MakeCriticalError("invalid is-account state");
                }

                (void)user_data_->SetIsAccount(is_account);
            }

            if (j["AccountUsername"].is_string()) {
                (void)user_data_->SetAccountUsername(j["AccountUsername"].get<string>());
            }

            if (j["Balance"].is_number_integer()) {
                (void)user_data_->SetBalance(j["Balance"].get<int64_t>());
            }

            // We only try to use the PurchasePrices if we supplied purchase classes to the request
            if (!purchase_classes.empty() && j["PurchasePrices"].is_array()) {
                PurchasePrices purchase_prices;

                // The from_json for the PurchasePrice struct is for our internal (datastore and library API)
                // representation of PurchasePrice. We won't assume that the representation used by the
                // server is the same (nor that it won't change independent of our representation).
                for (const auto& pp : j["PurchasePrices"]) {
                    auto transaction_class = pp["Class"].get<string>();

                    purchase_prices.push_back(PurchasePrice{
                            transaction_class,
                            pp["Distinguisher"].get<string>(),
                            pp["Price"].get<int64_t>()
                    });
                }

                (void)user_data_->SetPurchasePrices(purchase_prices);
            }

            if (j["Purchases"].is_array()) {
                for (const auto& p : j["Purchases"]) {
                    auto purchase_res = PurchaseFromJSON(p);
                    if (!purchase_res) {
                        return WrapError(purchase_res.error(), "failed to deserialize purchases");
                    }

                    // Authorizations are applied to tunnel connections, which requires a reconnect
                    reconnect_required = reconnect_required || purchase_res->authorization;

                    (void)user_data_->AddPurchase(*purchase_res);
                }
            }

            // If the account tokens just expired, then we need to go into a logged-out state.
            if (IsAccount() && !HasTokens()) {
                // If we're transitioning to a logged out state and there are active
                // authorizations (applied to the current tunnel), then we need to
                // reconnect to remove them.
                reconnect_required = reconnect_required || !GetAuthorizations(true).empty();

                (void)user_data_->DeleteUserData(true);
            }

            if (auto err = transaction.Commit()) {
                return WrapError(err, "UserData write failed");
            }
        }
        catch (json::exception& e) {
            return MakeCriticalError(
                    utils::Stringer("json parse failed: ", e.what(), "; id:", e.id));
        }

        if (IsAccount()) {
            // For accounts there's nothing else we can do, regardless of the state of token validity.
            return PsiCash::RefreshStateResponse{ Status::Success, reconnect_required };
        }

        if (HasTokens()) {
            // We have a good tracker state.
            return PsiCash::RefreshStateResponse{ Status::Success, reconnect_required };
        }

        // We started out with tracker tokens, but they're all invalid.
        // Note that this shouldn't happen -- we "know" that Tracker tokens don't
        // expire -- but we'll still try to recover if we haven't already recursed.

        if (!allow_recursion) {
            return MakeCriticalError("failed to obtain valid tracker tokens (b)");
        }

        return RefreshState(purchase_classes, true);
    }
    else if (result->code == kHTTPStatusUnauthorized) {
        // This can only happen if the tokens we sent didn't all belong to same user.
        // This really should never happen. We're not checking the return value, as there
        // isn't a sane response to a failure at this point.
        (void)user_data_->Clear();
        return PsiCash::RefreshStateResponse{ Status::InvalidTokens, false };
    }
    else if (IsServerError(result->code)) {
        return PsiCash::RefreshStateResponse{ Status::ServerError, false };
    }

    return MakeCriticalError(utils::Stringer(
            "request returned unexpected result code: ", result->code, "; ",
            result->body, "; ", json(result->headers).dump()));
}

Result<PsiCash::NewExpiringPurchaseResponse> PsiCash::NewExpiringPurchase(
        const string& transaction_class,
        const string& distinguisher,
        const int64_t expected_price) {
    TOKENS_REQUIRED;

    auto result = MakeHTTPRequestWithRetry(
            kMethodPOST,
            "/transaction",
            true,
            {
                    {"class",          transaction_class},
                    {"distinguisher",  distinguisher},
                    // Note the conversion from positive to negative: price to amount.
                    {"expectedAmount", to_string(-expected_price)}
            },
            nullopt // body
    );
    if (!result) {
        return WrapError(result.error(), "MakeHTTPRequestWithRetry failed");
    }

    optional<Purchase> purchase;

    // These statuses require the response body to be parsed
    if (result->code == kHTTPStatusOK ||
        result->code == kHTTPStatusTooManyRequests ||
        result->code == kHTTPStatusPaymentRequired ||
        result->code == kHTTPStatusConflict) {
        if (result->body.empty()) {
            return MakeCriticalError(
                    utils::Stringer("result has no body; code: ", result->code));
        }

        try {
            auto j = json::parse(result->body);

            // Set our new data in a single write.
            // Note that any early return will cause updates to roll back.
            UserData::Transaction transaction(*user_data_);

            // Balance is present for all non-error responses
            if (j.at("Balance").is_number_integer()) {
                // We don't care about the return value of this right now
                (void)user_data_->SetBalance(j.at("Balance").get<int64_t>());
            }

            if (result->code == kHTTPStatusOK) {
                auto parse_res = PurchaseFromJSON(j, "expiring-purchase");
                if (!parse_res) {
                    return WrapError(parse_res.error(), "failed to parse purchase from response JSON");
                }

                purchase = *parse_res;

                if (!purchase->server_time_expiry) {
                    // Purchase expiry is optional, but we're specifically making a New**Expiring**Purchase
                    return MakeCriticalError("response did not provide valid expiry");
                }

                // Not checking authorization, as it doesn't apply to all expiring purchases

                if (auto err = user_data_->AddPurchase(*purchase)) {
                    return WrapError(err, "AddPurchase failed");
                }

            }

            if (auto err = transaction.Commit()) {
                return WrapError(err, "UserData write failed");
            }
        }
        catch (json::exception& e) {
            return MakeCriticalError(
                    utils::Stringer("json parse failed: ", e.what(), "; id:", e.id));
        }
    }

    optional<PsiCash::NewExpiringPurchaseResponse> response;

    if (result->code == kHTTPStatusOK) {
        response = PsiCash::NewExpiringPurchaseResponse{
                Status::Success,
                purchase
        };
    } else if (result->code == kHTTPStatusTooManyRequests) {
        response = PsiCash::NewExpiringPurchaseResponse{
                Status::ExistingTransaction
        };
    } else if (result->code == kHTTPStatusPaymentRequired) {
        response = PsiCash::NewExpiringPurchaseResponse{
                Status::InsufficientBalance
        };
    } else if (result->code == kHTTPStatusConflict) {
        response = PsiCash::NewExpiringPurchaseResponse{
                Status::TransactionAmountMismatch
        };
    } else if (result->code == kHTTPStatusNotFound) {
        response = PsiCash::NewExpiringPurchaseResponse{
                Status::TransactionTypeNotFound
        };
    } else if (result->code == kHTTPStatusUnauthorized) {
        response = PsiCash::NewExpiringPurchaseResponse{
                Status::InvalidTokens
        };
    } else if (IsServerError(result->code)) {
        response = PsiCash::NewExpiringPurchaseResponse{
                Status::ServerError
        };
    }
    else {
        return MakeCriticalError(utils::Stringer(
                "request returned unexpected result code: ", result->code, "; ",
                result->body, "; ", json(result->headers).dump()));
    }

    assert(response);
    return *response;
}

Result<PsiCash::AccountLogoutResponse> PsiCash::AccountLogout() {
    TOKENS_REQUIRED;

    if (!IsAccount()) {
        return MakeNoncriticalError("user is not account");
    }

    // Authorizations are applied to psiphond connections, so the presence of an active
    // one means we will need to reconnect after logging out.
    bool reconnect_required = !GetAuthorizations(true).empty();

    Error httpErr;
    auto result = MakeHTTPRequestWithRetry(
            kMethodPOST,
            "/logout",
            true,  // include auth tokens
            {},
            nullopt // body
    );
    if (!result) {
        httpErr = result.error();
    }
    else if (result->code != kHTTPStatusOK) {
        httpErr = MakeNoncriticalError(utils::Stringer("logout request failed; code:", result->code, "; body:", result->body));
    }
    // Even if an error occurred, we still want to do the local logout, so carry on.

    auto localErr = user_data_->DeleteUserData(true);

    // The localErr is a more significant failure, so check it first.
    if (localErr) {
        return WrapError(localErr, "local AccountLogout failed");
    }
    /*
    // We are not returning an error if the remote request failed. We have already
    // affected the local logout, and we'll have to rely on the next login from this
    // device to invalidate the tokens on the server.
    else if (httpErr) {
        return WrapError(httpErr, "MakeHTTPRequestWithRetry failed");
    }
    */

    return PsiCash::AccountLogoutResponse{ reconnect_required };
}

error::Result<PsiCash::AccountLoginResponse> PsiCash::AccountLogin(
        const std::string& utf8_username,
        const std::string& utf8_password) {
    MUST_BE_INITIALIZED;

    static const vector<string> token_types = {kEarnerTokenType, kSpenderTokenType, kIndicatorTokenType, kLogoutTokenType};
    static const string token_types_str = utils::Join(token_types, ",");

    // If we have tracker tokens, include them to (attempt to) merge the balance.
    string old_tokens;
    if (!IsAccount() && HasTokens()) {
        old_tokens = CommaDelimitTokens({});
    }

    json body =
        {
            {"username", utf8_username},
            {"password", utf8_password},
            {"instanceID", user_data_->GetInstanceID()},
            {"tokenTypes", token_types_str},
            {"oldTokens", old_tokens}
        };

    auto result = MakeHTTPRequestWithRetry(
            kMethodPOST,
            "/login",
            false,  // tokens for tracker merge are provided via the request body
            {},    // query params
            body
    );
    if (!result) {
        return WrapError(result.error(), "MakeHTTPRequestWithRetry failed");
    }

    if (result->code == kHTTPStatusOK) {
        // Delete whatever local user data may be present. If it was a tracker, it has
        // been merged now (or can't be); if it was an account, we should interpret the
        // login as a desire to no longer be logged in with the previous account.
        if (auto err = ResetUser()) {
            return PassError(err);
        }

        if (result->body.empty()) {
            return MakeCriticalError(
                    utils::Stringer("result has no body; code: ", result->code));
        }

        AuthTokens auth_tokens;
        optional<bool> last_tracker_merge;
        try {
            auto j = json::parse(result->body);
            auth_tokens = j["Tokens"].get<AuthTokens>();

            if (!j.at("TrackerMerged").is_null()) {
                auto tracker_merges_remaining = j["TrackerMergesRemaining"].get<int>();
                auto tracker_merged = j["TrackerMerged"].get<bool>();
                last_tracker_merge = tracker_merged && tracker_merges_remaining == 0;
            }
        }
        catch (json::exception& e) {
            return MakeCriticalError(
                    utils::Stringer("json parse failed: ", e.what(), "; id:", e.id));
        }

        // Sanity check
        if (auth_tokens.size() < token_types.size()) {
            return MakeCriticalError(
                    utils::Stringer("bad number of tokens received: ", auth_tokens.size()));
        }

        // Set our new data in a single write.
        UserData::Transaction transaction(*user_data_);
        (void)user_data_->SetIsLoggedOutAccount(false);
        (void)user_data_->SetAuthTokens(auth_tokens, /*is_account=*/true, /*utf8_username=*/utf8_username);
        if (auto err = transaction.Commit()) {
            return WrapError(err, "user data write failed");
        }

        return PsiCash::AccountLoginResponse{
            Status::Success,
            last_tracker_merge
        };
    }
    else if (result->code == kHTTPStatusUnauthorized) {
        return PsiCash::AccountLoginResponse{
                Status::InvalidCredentials
        };
    }
    else if (result->code == kHTTPStatusBadRequest) {
        return PsiCash::AccountLoginResponse{
                Status::BadRequest
        };
    }
    else if (IsServerError(result->code)) {
        return PsiCash::AccountLoginResponse{
                Status::ServerError
        };
    }

    return MakeCriticalError(utils::Stringer(
            "request returned unexpected result code: ", result->code, "; ",
            result->body, "; ", json(result->headers).dump()));
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
// NOTE: This is only for datastore purposes, and not for server responses.
bool operator==(const Purchase& lhs, const Purchase& rhs) {
    return lhs.transaction_class == rhs.transaction_class &&
           lhs.distinguisher == rhs.distinguisher &&
           lhs.server_time_expiry == rhs.server_time_expiry &&
           //lhs.local_time_expiry == rhs.local_time_expiry && // Don't include the derived local time in the comparison
           lhs.authorization == rhs.authorization &&
           lhs.server_time_created == rhs.server_time_created;
}

void to_json(json& j, const Purchase& p) {
    j = json{
            {"id",                p.id},
            {"class",             p.transaction_class},
            {"distinguisher",     p.distinguisher},
            {"serverTimeCreated", p.server_time_created}};

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
        p.authorization = j.at("authorization").get<Authorization>();
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

    // This field was not added until later versions of the datastore, so may not be present.
    if (j.contains("serverTimeCreated")) {
        p.server_time_created = j.at("serverTimeCreated").get<datetime::DateTime>();
    } else {
        // Default it to a very long time ago.
        p.server_time_created = datetime::DateTime(datetime::TimePoint(datetime::DurationFromInt64(1)));
    }
}

/// Builds a purchase from server response JSON.
error::Result<psicash::Purchase> PsiCash::PurchaseFromJSON(const json& j, const string& expected_type/*=""*/) const {
    string transaction_id, transaction_class, transaction_distinguisher, authorization_encoded, transaction_type;
    datetime::DateTime server_expiry, server_created;
    try {
        if (!expected_type.empty() && expected_type != j.at("/TransactionResponse/Type"_json_pointer).get<string>()) {
            return MakeCriticalError("expected type mismatch; want '"s + expected_type + "'; got '" + j.at("/TransactionResponse/Type"_json_pointer).get<string>() + "'");
        }

        transaction_id = j.at("TransactionID").get<string>();
        transaction_class = j.at("Class").get<string>();
        transaction_distinguisher = j.at("Distinguisher").get<string>();

        if (!server_created.FromISO8601(j.at("Created").get<string>())) {
            return MakeCriticalError("failed to parse Created; got "s + j.at("Created").get<string>());
        }

        if (j.at("Authorization").is_string()) {
            authorization_encoded = j["Authorization"].get<string>();
        }

        // NOTE: The presence of this field depends on the type. Right now we only have
        // expiring purchases, but that may change in the future.
        if (j.at("/TransactionResponse/Values/Expires"_json_pointer).is_string()) {
            auto expiry_string = j["/TransactionResponse/Values/Expires"_json_pointer].get<string>();
            if (!server_expiry.FromISO8601(expiry_string)) {
                return MakeCriticalError("failed to parse TransactionResponse.Values.Expires; got "s + expiry_string);
            }
        }

        // Unused fields
        //auto transaction_amount = j.at("TransactionAmount").get<int64_t>();
    }
    catch (json::exception& e) {
        return MakeCriticalError(
                utils::Stringer("json parse failed: ", e.what(), "; id:", e.id));
    }

    optional<Authorization> authOptional = nullopt;
    if (!authorization_encoded.empty()) {
        auto decodeAuthResult = DecodeAuthorization(authorization_encoded);
        if (!decodeAuthResult) {
            // Authorization can be optional, but inability to decode suggests
            // something is very wrong.
            return WrapError(decodeAuthResult.error(), "failed to decode Purchase Authorization");
        }
        authOptional = *decodeAuthResult;
    }

    Purchase purchase = {
        transaction_id,
        server_created,
        transaction_class,
        transaction_distinguisher,
        server_expiry.IsZero() ? nullopt : make_optional(
                server_expiry),
        server_expiry.IsZero() ? nullopt : make_optional(
                server_expiry),
        authOptional
    };

    user_data_->UpdatePurchaseLocalTimeExpiry(purchase);

    return purchase;
}

// Enable JSON de/serializing of Authorization.
// See https://github.com/nlohmann/json#basic-usage
bool operator==(const Authorization& lhs, const Authorization& rhs) {
    return lhs.encoded == rhs.encoded;
}

void to_json(json& j, const Authorization& v) {
    j = json{
            {"ID",         v.id},
            {"AccessType", v.access_type},
            {"Expires",    v.expires},
            {"Encoded",    v.encoded}};
}

void from_json(const json& j, Authorization& v) {
    v.id = j.at("ID").get<string>();
    v.access_type = j.at("AccessType").get<string>();
    v.expires = j.at("Expires").get<datetime::DateTime>();

    // When an Authorization comes from the server, it is itself encoded, but doesn't have
    // and "Encoded" field. When we store the Authorization in the local datastore, the
    // encoded value is present, and will therefore be present when we deserialize.
    v.encoded = j.value("Encoded", ""s);
}

Result<Authorization> DecodeAuthorization(const string& encoded) {
    try {
        auto decoded = base64::B64Decode(encoded);
        auto json = json::parse(decoded);
        auto auth = json.at("Authorization").get<Authorization>();
        auth.encoded = encoded;
        return auth;
    }
    catch (json::exception& e) {
        return MakeCriticalError(
                utils::Stringer("json parse failed: ", e.what(), "; id:", e.id));
    }
}

} // namespace psicash
