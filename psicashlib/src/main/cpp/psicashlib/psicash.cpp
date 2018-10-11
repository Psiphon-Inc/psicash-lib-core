#include <map>
#include <iostream>
#include <sstream>
#include "psicash.h"
#include "http_request.h"
#include "userdata.h"
#include "datetime.h"
#include "error.h"

#include "vendor/nlohmann/json.hpp"

using json = nlohmann::json;

using namespace std;
using namespace nonstd;
using namespace psicash;
using namespace error;


string psicash::ErrorMsg(const char *msg, const char *func, int line) {
  stringbuf sbuf;
  ostream os(&sbuf);
  os << msg << " @" << func << ":" << line;
  return string("{\"error\":\"") + sbuf.str() + "\"}";
}

bool MakeHTTPRequestWithRetry(MakeHTTPRequestFn make_http_request_fn, string req_params, HTTPResult &result) {
  auto result_string = make_http_request_fn(req_params);
  if (result_string.length() == 0) {
    // An error so catastrophic that we don't get any error info.
    return false;
  }

  auto json_result = json::parse(result_string);

  result.status = json_result["status"].get<int>();
  if (result.status < 0) {
    result.error = json_result["error"].get<string>();
  } else {
    result.body = json_result["body"].get<string>();
  }

  return true;
}

//
// PsiCash class implementation
//

PsiCash::PsiCash()
    : make_http_request_fn_(nullptr) {
}

PsiCash::~PsiCash() {
}

Error PsiCash::Init(const char *file_store_root, MakeHTTPRequestFn make_http_request_fn) {
  if (!file_store_root) {
    return MakeError("file_store_root null");
  }

  make_http_request_fn_ = make_http_request_fn;

  user_data_ = std::make_unique<UserData>();
  auto err = user_data_->Init(file_store_root);
  if (err) {
    // If UserData.Init fails, the only way to proceed to try to reset it and create a new one.
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

//
// Stored info accessors
//

bool PsiCash::IsAccount() const {
  return user_data_->GetIsAccount();
}

TokenTypes PsiCash::ValidTokenTypes() const {
  TokenTypes tt;

  auto auth_tokens = user_data_->GetAuthTokens();
  for (const auto &it : auth_tokens) {
    tt.push_back(it.first);
  }

  return tt;
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
    // We're using server time, since we're not comparing to local now.
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

//
//
//

string PsiCash::NewTracker() {
  AuthTokens at;
  at["xxx"] = "yyy";
  user_data_->SetAuthTokens(at, false);
  auto bt = user_data_->GetAuthTokens();

  auto tt = ValidTokenTypes();

  auto reqParams = buildRequestParams("tracker", HTTP_METHOD_POST, {}, false);

  auto response = make_http_request_fn_(reqParams);

  return response;
}

bool NewExpiringPurchaseTransactionForClass(
    MakeHTTPRequestFn make_http_request_fn,
    const char *transactionClass, const char *distinguisher,
    int64_t expectedPrice) {
  auto queryParams = map<string, string>();
  queryParams["class"] = transactionClass;
  queryParams["distinguisher"] = distinguisher;
  // Note the conversion from positive to negative: price to amount.
  queryParams["expectedAmount"] = -expectedPrice;

  auto reqParams = buildRequestParams("transaction", HTTP_METHOD_POST, queryParams, true);

  HTTPResult result;
  auto reqSuccess = MakeHTTPRequestWithRetry(make_http_request_fn, reqParams, result);

  if (!reqSuccess) {
    // An error so catastrophic that we don't get any error info.
    return false;
  }

  return true;
}

// Enable JSON de/serializing of PurchasePrice.
// See https://github.com/nlohmann/json#basic-usage
namespace psicash {
bool operator==(const PurchasePrice &lhs, const PurchasePrice &rhs) {
  return lhs.transaction_class == rhs.transaction_class &&
         lhs.distinguisher == rhs.distinguisher &&
         lhs.price == rhs.price;
}

void to_json(json& j, const PurchasePrice& pp) {
  j = json{
      {"transactionClass", pp.transaction_class},
      {"distinguisher",    pp.distinguisher},
      {"price",            pp.price}};
}

void from_json(const json& j, PurchasePrice& pp) {
  pp.transaction_class = j.at("transactionClass").get<string>();
  pp.distinguisher = j.at("distinguisher").get<string>();
  pp.price = j.at("price").get<int64_t>();
}
} // namespace psicash

// Enable JSON de/serializing of Purchase.
// See https://github.com/nlohmann/json#basic-usage
namespace psicash {
bool operator==(const Purchase &lhs, const Purchase &rhs) {
  return lhs.transaction_class == rhs.transaction_class &&
         lhs.distinguisher == rhs.distinguisher &&
         lhs.server_time_expiry == rhs.server_time_expiry &&
         //lhs.local_time_expiry == rhs.local_time_expiry && // Don't include the derived local time in the comparison
         lhs.authorization == rhs.authorization;
}

void to_json(json& j, const Purchase& p) {
  j = json{
      {"id",               p.id},
      {"transactionClass", p.transaction_class},
      {"distinguisher",    p.distinguisher}};

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
  p.transaction_class = j.at("transactionClass").get<string>();
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
