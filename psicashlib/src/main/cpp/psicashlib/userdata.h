#ifndef PSICASHLIB_USERDATA_H
#define PSICASHLIB_USERDATA_H

#include <cstdint>
#include "datastore.h"
#include "psicash.h"
#include "datetime.h"
#include "error.h"
#include "vendor/nlohmann/json.hpp"

namespace psicash
{
extern const char *REQUEST_METADATA; // only for use in template method below

using AuthTokens = std::map<std::string, std::string>;

// UserData operations are NOT THREADSAFE.
class UserData
{
public:
  UserData();
  ~UserData();

  // Must be called once.
  // Returns false if there's an unrecoverable error (such as an inability to use the filesystem).
  error::Error Init(const char *file_store_root);

  // Clears data and datastore file.
  void Clear();

public:
  datetime::Duration GetServerTimeDiff() const;
  error::Error SetServerTimeDiff(const datetime::DateTime& serverTimeNow);

  AuthTokens GetAuthTokens() const;
  error::Error SetAuthTokens(const AuthTokens& v);

  bool GetIsAccount() const;
  error::Error SetIsAccount(bool v);

  int64_t GetBalance() const;
  error::Error SetBalance(int64_t v);

  PurchasePrices GetPurchasePrices() const;
  error::Error SetPurchasePrices(const PurchasePrices& v);

  Purchases GetPurchases() const;
  error::Error SetPurchases(const Purchases& v);

  TransactionID GetLastTransactionID() const;
  error::Error SetLastTransactionID(const TransactionID& v);

  std::string GetRequestMetadataJSON() const;
  template <typename T>
  error::Error SetRequestMetadataItem(const char *key, const T& val)
  {
    return datastore_.Set({{REQUEST_METADATA, {{key, val}}}});
  }

protected:
  // Modifies the purchases in the argument.
  void UpdatePurchasesLocalTimeExpiry(Purchases& purchases) const;

private:
  Datastore datastore_;
};
} // namespace psicash

#endif //PSICASHLIB_USERDATA_H
