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

extern const char* REQUEST_METADATA; // only for use in template method below

using AuthTokens = std::map<std::string, std::string>;

/// Storage and retrieval (and some processing) of PsiCash user data/state.
/// UserData operations are threadsafe (via Datastore).
class UserData {
public:
    UserData();

    virtual ~UserData();

    /// Must be called once.
    /// Returns false if there's an unrecoverable error (such as an inability to use the filesystem).
    error::Error Init(const char* file_store_root);

    /// Clears data and datastore file.
    void Clear();

    /// Used to pause and result datastore file writing.
    class WritePauser {
    public:
        WritePauser(UserData& user_data) : user_data_(
                user_data) { user_data_.datastore_.PauseWrites(); };
        ~WritePauser() { (void)Unpause(); } // TODO: Should dtor nuke changes (implying error)? Maybe param to ctor to indicate?
        error::Error Unpause() { return user_data_.datastore_.UnpauseWrites(); }
    private:
        UserData& user_data_;
    };

public:
    datetime::Duration GetServerTimeDiff() const;
    error::Error SetServerTimeDiff(const datetime::DateTime& serverTimeNow);
    /// Modifies the argument purchase.
    void UpdatePurchaseLocalTimeExpiry(Purchase& purchase) const;

    AuthTokens GetAuthTokens() const;
    error::Error SetAuthTokens(const AuthTokens& v, bool is_account);
    /// valid_token_types is of the form {"tokenvalueABCD0123": true, ...}
    error::Error CullAuthTokens(const std::map<std::string, bool>& valid_tokens);

    bool GetIsAccount() const;
    error::Error SetIsAccount(bool v);

    int64_t GetBalance() const;
    error::Error SetBalance(int64_t v);

    PurchasePrices GetPurchasePrices() const;
    error::Error SetPurchasePrices(const PurchasePrices& v);

    Purchases GetPurchases() const;
    error::Error SetPurchases(const Purchases& v);
    error::Error AddPurchase(const Purchase& v);

    TransactionID GetLastTransactionID() const;
    error::Error SetLastTransactionID(const TransactionID& v);

    nlohmann::json GetRequestMetadata() const;
    template<typename T>
    error::Error SetRequestMetadataItem(const std::string& key, const T& val) {
        if (key.empty()) {
            return MakeCriticalError("Metadata key cannot be empty");
        }
        auto j = GetRequestMetadata();
        j[key] = val;
        return datastore_.Set({{REQUEST_METADATA, j}});
    }

protected:
    /// Modifies the purchases in the argument.
    void UpdatePurchasesLocalTimeExpiry(Purchases& purchases) const;

private:
    Datastore datastore_;
};

} // namespace psicash

#endif //PSICASHLIB_USERDATA_H
