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

#include "userdata.hpp"
#include "datastore.hpp"
#include "psicash.hpp"
#include "vendor/nlohmann/json.hpp"

using json = nlohmann::json;

using namespace std;
using namespace error;

namespace psicash {
// Datastore keys
static constexpr const char* VERSION = "v"; // Preliminary version key; not yet used for anything
static constexpr const char* SERVER_TIME_DIFF = "serverTimeDiff";
static constexpr const char* AUTH_TOKENS = "authTokens";
static constexpr const char* BALANCE = "balance";
static constexpr const char* IS_ACCOUNT = "IsAccount";
static constexpr const char* PURCHASE_PRICES = "purchasePrices";
static constexpr const char* PURCHASES = "purchases";
static constexpr const char* LAST_TRANSACTION_ID = "lastTransactionID";
const char* REQUEST_METADATA = "requestMetadata"; // used in header

UserData::UserData() {
}

UserData::~UserData() {
}

Error UserData::Init(const char* file_store_root) {
    auto err = datastore_.Init(file_store_root);
    if (err) {
        return PassError(err);
    }

    err = datastore_.Set({{VERSION, 1}});
    if (err) {
        return PassError(err);
    }

    return nullerr;
}

void UserData::Clear() {
    datastore_.Clear();
}

datetime::Duration UserData::GetServerTimeDiff() const {
    auto v = datastore_.Get<int64_t>(SERVER_TIME_DIFF);
    if (!v) {
        return datetime::DurationFromInt64(0);
    }
    return datetime::DurationFromInt64(*v);
}

Error UserData::SetServerTimeDiff(const datetime::DateTime& serverTimeNow) {
    auto localTimeNow = datetime::DateTime::Now();
    auto diff = serverTimeNow.Diff(localTimeNow);
    return PassError(datastore_.Set({{SERVER_TIME_DIFF, datetime::DurationToInt64(diff)}}));
}

AuthTokens UserData::GetAuthTokens() const {
    auto v = datastore_.Get<AuthTokens>(AUTH_TOKENS);
    if (!v) {
        return AuthTokens();
    }
    return *v;
}

Error UserData::SetAuthTokens(const AuthTokens& v, bool is_account) {
    return PassError(datastore_.Set({{AUTH_TOKENS, v},
                                     {IS_ACCOUNT,  is_account}}));
}

error::Error UserData::CullAuthTokens(const std::map<std::string, bool>& valid_tokens) {
    auto all_auth_tokens = GetAuthTokens();
    AuthTokens good_auth_tokens;

    // all_auth_tokens is { "earner": "ABCD0123" } and valid_tokens is { "ABCD0123": true }
    for (const auto& t : all_auth_tokens) {
        for (const auto& vtt : valid_tokens) {
            if (vtt.first == t.second && vtt.second) {
                good_auth_tokens[t.first] = t.second;
                break;
            }
        }
    }

    return PassError(datastore_.Set({{AUTH_TOKENS, good_auth_tokens}}));
}

bool UserData::GetIsAccount() const {
    auto v = datastore_.Get<bool>(IS_ACCOUNT);
    if (!v) {
        return false;
    }
    return *v;
}

Error UserData::SetIsAccount(bool v) {
    return PassError(datastore_.Set({{IS_ACCOUNT, v}}));
}

int64_t UserData::GetBalance() const {
    auto v = datastore_.Get<int64_t>(BALANCE);
    if (!v) {
        return 0;
    }
    return *v;
}

Error UserData::SetBalance(int64_t v) {
    return PassError(datastore_.Set({{BALANCE, v}}));
}

PurchasePrices UserData::GetPurchasePrices() const {
    auto v = datastore_.Get<PurchasePrices>(PURCHASE_PRICES);
    if (!v) {
        return PurchasePrices();
    }
    return *v;
}

Error UserData::SetPurchasePrices(const PurchasePrices& v) {
    return PassError(datastore_.Set({{PURCHASE_PRICES, v}}));
}

Purchases UserData::GetPurchases() const {
    auto v = datastore_.Get<Purchases>(PURCHASES);
    if (!v) {
        v = Purchases();
    }

    UpdatePurchasesLocalTimeExpiry(*v);
    return *v;
}

Error UserData::SetPurchases(const Purchases& v) {
    return PassError(datastore_.Set({{PURCHASES, v}}));
}

Error UserData::AddPurchase(const Purchase& v) {
    auto purchases = GetPurchases();
    // Prevent duplicate insertion
    for (const auto& p : purchases) {
        if (p.id == v.id) {
            return nullerr;
        }
    }

    purchases.push_back(v);

    // Pause to set Purchases and LastTransactionID in one write
    WritePauser pauser(*this);
    // These don't write, so have no meaningful return
    (void)SetPurchases(purchases);
    (void)SetLastTransactionID(v.id);
    return PassError(pauser.Unpause()); // write
}

void UserData::UpdatePurchaseLocalTimeExpiry(Purchase& purchase) const {
    if (!purchase.server_time_expiry) {
        return;
    }

    // server_time_diff is server-minus-local. So it's positive if server is ahead, negative if behind.
    // So we have to subtract the diff from the server time to get the local time.
    // Δ = s - l
    // l = s - Δ
    purchase.local_time_expiry = purchase.server_time_expiry->Sub(GetServerTimeDiff());
}

void UserData::UpdatePurchasesLocalTimeExpiry(Purchases& purchases) const {
    for (auto& p : purchases) {
        UpdatePurchaseLocalTimeExpiry(p);
    }
}

TransactionID UserData::GetLastTransactionID() const {
    auto v = datastore_.Get<TransactionID>(LAST_TRANSACTION_ID);
    if (!v) {
        return TransactionID();
    }
    return *v;
}

Error UserData::SetLastTransactionID(const TransactionID& v) {
    return PassError(datastore_.Set({{LAST_TRANSACTION_ID, v}}));
}

json UserData::GetRequestMetadata() const {
    auto j = datastore_.Get<json>(REQUEST_METADATA);
    if (!j) {
        return json::object();
    }

    return *j;
}

} // namespace psicash
