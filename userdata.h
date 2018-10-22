#ifndef PSICASHLIB_USERDATA_H
#define PSICASHLIB_USERDATA_H

#include <cstdint>
#include "datastore.h"
#include "psicash.h"
#include "datetime.h"
#include "error.h"
#include "vendor/nlohmann/json.hpp"

namespace psicash {
extern const char* REQUEST_METADATA; // only for use in template method below

using AuthTokens = std::map<std::string, std::string>;

// UserData operations are NOT THREADSAFE.
class UserData {
public:
    UserData();

    virtual ~UserData();

    // Must be called once.
    // Returns false if there's an unrecoverable error (such as an inability to use the filesystem).
    error::Error Init(const char* file_store_root);

    // Clears data and datastore file.
    void Clear();

    class WritePauser {
    public:
        WritePauser(UserData& user_data) : user_data_(
                user_data) { user_data_.datastore_.PauseWrites(); };
        ~WritePauser() { (void)Unpause(); }
        error::Error Unpause() { return user_data_.datastore_.UnpauseWrites(); }
    private:
        UserData& user_data_;
    };

public:
    datetime::Duration GetServerTimeDiff() const;
    error::Error SetServerTimeDiff(const datetime::DateTime& serverTimeNow);
    // Modifies the argument purchase.
    void UpdatePurchaseLocalTimeExpiry(Purchase& purchase) const;

    AuthTokens GetAuthTokens() const;
    error::Error SetAuthTokens(const AuthTokens& v, bool is_account);
    // valid_token_types is of the form {"tokenvalueABCD0123": true, etc.}
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
        auto j = GetRequestMetadata();
        j[key] = val;
        return datastore_.Set({{REQUEST_METADATA, j}});
    }

protected:
    // Modifies the purchases in the argument.
    void UpdatePurchasesLocalTimeExpiry(Purchases& purchases) const;

private:
    Datastore datastore_;
};
} // namespace psicash

#endif //PSICASHLIB_USERDATA_H
