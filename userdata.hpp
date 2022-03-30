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

#ifndef PSICASHLIB_USERDATA_H
#define PSICASHLIB_USERDATA_H

#include <cstdint>
#include "datastore.hpp"
#include "psicash.hpp"
#include "datetime.hpp"
#include "error.hpp"
#include "vendor/nlohmann/json.hpp"

namespace psicash {

extern const nlohmann::json::json_pointer kRequestMetadataPtr; // only for use in template method below

struct TokenInfo { std::string id; nonstd::optional<datetime::DateTime> server_time_expiry; };
using AuthTokens = std::map<std::string, TokenInfo>; // type to token info
void to_json(nlohmann::json& j, const AuthTokens& v);
void from_json(const nlohmann::json& j, AuthTokens& v);
using TokenTypes = std::vector<std::string>;

// These are the possible token types.
extern const char* const kEarnerTokenType;
extern const char* const kSpenderTokenType;
extern const char* const kIndicatorTokenType;
extern const char* const kAccountTokenType;
extern const char* const kLogoutTokenType;


/// Storage and retrieval (and some processing) of PsiCash user data/state.
/// UserData operations are threadsafe (via Datastore).
class UserData {
public:
    UserData();

    virtual ~UserData();

    /// Must be called once.
    /// dev should be true if this instance is communicating with the dev server.
    /// Returns false if there's an unrecoverable error (such as an inability to use the filesystem).
    error::Error Init(const std::string& file_store_root, bool dev);

    /// Clears data and datastore file. Calling this does not change the initialized
    /// state. If the datastore was already initialized with a different file_root+suffix,
    /// then the result is undefined.
    error::Error Clear(const std::string& file_store_root, bool dev);

    /// Clears data and datastore file. Calling this does not change the initialized state.
    /// Init() must have already been called, successfully.
    error::Error Clear();

    /// Used to wrap datastore "transactions" (paused writing, mutexed access).
    /// Transaction can be nested -- inner instances will do nothing.
    class Transaction {
    public:
        Transaction(UserData& user_data) : user_data_(user_data), in_transaction_(false)
            { user_data_.datastore_.BeginTransaction(); in_transaction_ = true; };
        ~Transaction() { if (in_transaction_) { (void)Rollback(); } }
        error::Error Commit() { return End(true); }
        error::Error Rollback() { return End(false); }
    private:
        error::Error End(bool commit) {
            if (in_transaction_) {
                in_transaction_ = false; return user_data_.datastore_.EndTransaction(commit);
            }
            return error::nullerr;
        }
        UserData& user_data_;
        bool in_transaction_;
    };

public:
    /// Deletes the stored user data and sets the isLoggedOutAccount flag.
    error::Error DeleteUserData(bool isLoggedOutAccount);

    std::string GetInstanceID() const;
    /// Returns true if there is a non-empty instance ID in the datstore.
    bool HasInstanceID() const;

    bool GetIsLoggedOutAccount() const;
    error::Error SetIsLoggedOutAccount(bool v);

    datetime::Duration GetServerTimeDiff() const;
    error::Error SetServerTimeDiff(const datetime::DateTime& serverTimeNow);
    /// Converts `server_time` to local time using the current diff
    datetime::DateTime ServerTimeToLocal(const datetime::DateTime& server_time) const;
    /// Modifies the argument purchase.
    void UpdatePurchaseLocalTimeExpiry(Purchase& purchase) const;

    AuthTokens GetAuthTokens() const;
    /// `utf8_username` must be set if `is_account` is true.
    error::Error SetAuthTokens(const AuthTokens& v, bool is_account, const std::string& utf8_username);
    /// valid_token_types is of the form {"tokenvalueABCD0123": true, ...}
    error::Error CullAuthTokens(const std::map<std::string, bool>& valid_tokens);
    psicash::TokenTypes ValidTokenTypes() const;

    bool GetIsAccount() const;
    /// Note that setting is-account to true does _not_ populate the account username field.
    error::Error SetIsAccount(bool v);

    std::string GetAccountUsername() const;
    error::Error SetAccountUsername(const std::string& v);

    int64_t GetBalance() const;
    error::Error SetBalance(int64_t v);

    PurchasePrices GetPurchasePrices() const;
    error::Error SetPurchasePrices(const PurchasePrices& v);

    Purchases GetPurchases() const;
    /// Does not update LastTransactionID. This must only be called when storing a subset
    /// of the already-existing purchases. Also, the vector must still be sorted.
    error::Error SetPurchases(const Purchases& v);
    /// Does update LastTransactionID.
    error::Error AddPurchase(const Purchase& v);

    TransactionID GetLastTransactionID() const;
    error::Error SetLastTransactionID(const TransactionID& v);

    nlohmann::json GetRequestMetadata() const;
    template<typename T>
    error::Error SetRequestMetadataItem(const std::string& key, const T& val) {
        if (key.empty()) {
            return error::MakeCriticalError("Metadata key cannot be empty");
        }
        auto ptr = kRequestMetadataPtr / key;
        return datastore_.Set(ptr, val);
    }

    std::string GetLocale() const;
    error::Error SetLocale(const std::string& v);

protected:
    /// Modifies the purchases in the argument.
    void UpdatePurchasesLocalTimeExpiry(Purchases& purchases) const;

    nlohmann::json GetStashedRequestMetadata() const;
    void SetStashedRequestMetadata(const nlohmann::json& j);

private:
    Datastore datastore_;

    /// In-memory stash of request metadata. When DeleteUserData is called, the request
    /// metadata is lost. But we want that data available when making a Login request and
    /// we want to restore it after login (so that the clients don't have to set it again).
    /// This _must_ be accessed through the mutex. Use Get/SetStashedRequestMetadata.
    nlohmann::json stashed_request_metadata_;
    mutable std::recursive_mutex stashed_request_metadata_mutex_;
};

} // namespace psicash

#endif //PSICASHLIB_USERDATA_H
