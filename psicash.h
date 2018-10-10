#ifndef PSICASHLIB_PSICASH_H
#define PSICASHLIB_PSICASH_H

#include <string>
#include <functional>
#include <vector>
#include <memory>
#include "vendor/nonstd/optional.hpp"
#include "datetime.h"
#include "error.h"


namespace psicash {
class UserData;

using MakeHTTPRequestFn = std::function<std::string(const std::string &)>;

using TokenTypes = std::vector<std::string>;

struct PurchasePrice {
  std::string transaction_class;
  std::string distinguisher;
  int64_t price;

  friend bool operator==(const PurchasePrice &lhs, const PurchasePrice &rhs);
  friend void to_json(nlohmann::json& j, const PurchasePrice& pp);
  friend void from_json(const nlohmann::json& j, PurchasePrice& pp);
};

using PurchasePrices = std::vector<PurchasePrice>;

using TransactionID = std::string;
constexpr const auto kTransactionIDZero = "";

struct Purchase {
  TransactionID id;
  std::string transaction_class;
  std::string distinguisher;
  nonstd::optional<datetime::DateTime> server_time_expiry;
  nonstd::optional<datetime::DateTime> local_time_expiry;
  nonstd::optional<std::string> authorization;

  friend bool operator==(const Purchase &lhs, const Purchase &rhs);
  friend void to_json(nlohmann::json& j, const Purchase& p);
  friend void from_json(const nlohmann::json& j, Purchase& p);
};

using Purchases = std::vector<Purchase>;

class PsiCash {
public:
  PsiCash();

  ~PsiCash();

  // Must be called once. make_http_request_fn may be null and set later with SetHTTPRequestFn.
  // Returns false if there's an unrecoverable error (such as an inability to use the filesystem).
  error::Error Init(const char *file_store_root, MakeHTTPRequestFn make_http_request_fn);

  // Can be used for updating the HTTP requester function pointer.
  void SetHTTPRequestFn(MakeHTTPRequestFn make_http_request_fn);

  //
  // Stored info accessors
  //

  bool IsAccount() const;

  // Returns the stored valid token types. Like ["spender", "indicator"].
  // May be nil or empty.
  TokenTypes ValidTokenTypes() const;

  int64_t Balance() const;

  PurchasePrices GetPurchasePrices() const;

  Purchases GetPurchases() const;

  Purchases ValidPurchases() const;

  // The returned optional will false if there's no next expiring purchase.
  nonstd::optional<Purchase> NextExpiringPurchase() const;

  Purchases ExpirePurchases();

  void RemovePurchases(const std::vector<TransactionID>& ids);

  std::string NewTracker(); // TEMP

private:
#ifdef TESTING
protected:
#endif
  std::unique_ptr<UserData> user_data_;
  MakeHTTPRequestFn make_http_request_fn_;
};

std::string ErrorMsg(const char *msg, const char *func, int line);
} // namespace psicash

#endif //PSICASHLIB_PSICASH_H
