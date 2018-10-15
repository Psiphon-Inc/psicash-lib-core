#ifndef PSICASHLIB_PSICASH_H
#define PSICASHLIB_PSICASH_H

#include <string>
#include <functional>
#include <vector>
#include <memory>
#include "nonstd/optional.hpp"
#include "datetime.h"
#include "error.h"


namespace psicash {

//
// HTTP Request-related types
//
// The string param that MakeHTTPRequestFn takes is a JSON-encoding of this structure:
// {
//    "scheme": "https",
//    "hostname": "api.psi.cash",
//    "port": 443,
//    "method": "POST", // or "GET", etc.
//    "path": "/v1/tracker",
//    "headers": { "User-Agent": "value", ...etc. },
//    "query": { "class": "speed-boost", "expectedAmount": -10000, ...etc. }
// }
// The string param that it returns is an encoding of this structure:
struct HTTPResult
{
  // 200, 404, etc.
  int status;

  // The contents of the response body, if any.
  std::string body;

  // The value of the response Date header.
  std::string date;

  // Any error message relating to an unsuccessful network attempt;
  // must be empty if the request succeeded (regardless of status code).
  std::string error;

  HTTPResult() : status(-1) {}
};
using MakeHTTPRequestFn = std::function<std::string(const std::string &)>;


constexpr const char* kEarnerTokenType = "earner";
constexpr const char* kSpenderTokenType = "spender";
constexpr const char* kIndicatorTokenType = "indicator";
constexpr const char* kAccountTokenType = "account";

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

enum PsiCashStatus {
  PsiCashStatus_Invalid = -1,
  PsiCashStatus_Success = 0,
  PsiCashStatus_ExistingTransaction,
  PsiCashStatus_InsufficientBalance,
  PsiCashStatus_TransactionAmountMismatch,
  PsiCashStatus_TransactionTypeNotFound,
  PsiCashStatus_InvalidTokens,
  PsiCashStatus_ServerError
};

class UserData; // forward declaration

class PsiCash {
public:
  PsiCash();

  ~PsiCash();

  // Must be called once. make_http_request_fn may be null and set later with SetHTTPRequestFn.
  // Returns false if there's an unrecoverable error (such as an inability to use the filesystem).
  error::Error Init(const char *file_store_root, MakeHTTPRequestFn make_http_request_fn);

  // Can be used for updating the HTTP requester function pointer.
  void SetHTTPRequestFn(MakeHTTPRequestFn make_http_request_fn);

  error::Error SetRequestMetadataItem(const std::string& key, const std::string& value);

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

  error::Result<Purchases> ExpirePurchases();

  error::Error RemovePurchases(const std::vector<TransactionID>& ids);

  error::Result<std::string> ModifyLandingPage(const std::string& url) const;

  error::Result<std::string> GetRewardedActivityData() const;

  nlohmann::json GetDiagnosticInfo() const;

  //
  // API Server Requests
  //

  struct NewExpiringPurchaseResponse {
    PsiCashStatus status;
    nonstd::optional<Purchase> purchase;
  };
  error::Result<NewExpiringPurchaseResponse> NewExpiringPurchase(
      const std::string& transaction_class,
      const std::string& distinguisher,
      const int64_t expected_price);

protected:
  // HTTPResult.error will always be empty on a non-error return.
  error::Result<HTTPResult> MakeHTTPRequestWithRetry(
      const std::string& method, const std::string& path,
      bool include_auth_tokens, const nlohmann::json& query_params);
  error::Result<std::string> BuildRequestParams(
      const std::string& method, const std::string& path,
      bool include_auth_tokens, const nlohmann::json& query_params, int attempt) const;

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
