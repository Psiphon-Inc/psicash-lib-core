#include "userdata.h"
#include "datastore.h"
#include "psicash.h"
#include "vendor/nlohmann/json.hpp"
using json = nlohmann::json;

using namespace std;
using namespace error;
using namespace psicash;

namespace psicash {
// Datastore keys
static constexpr const char *VERSION = "v"; // Preliminary version key; not yet used for anything
static constexpr const char *SERVER_TIME_DIFF = "serverTimeDiff";
static constexpr const char *AUTH_TOKENS = "authTokens";
static constexpr const char *BALANCE = "balance";
static constexpr const char *IS_ACCOUNT = "IsAccount";
static constexpr const char *PURCHASE_PRICES = "purchasePrices";
static constexpr const char *PURCHASES = "purchases";
static constexpr const char *LAST_TRANSACTION_ID = "lastTransactionID";
const char *REQUEST_METADATA = "requestMetadata"; // used in header
} // namespace psicash

UserData::UserData() {
}

UserData::~UserData() {
}

Error UserData::Init(const char *file_store_root) {
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
  return PassError(datastore_.Set({{AUTH_TOKENS, v}, {IS_ACCOUNT, is_account}}));
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
  return PassError(SetPurchases(purchases));
}

void UserData::UpdatePurchasesLocalTimeExpiry(Purchases& purchases) const {
  auto server_time_diff = GetServerTimeDiff();
  for (auto& p : purchases) {
    if (!p.server_time_expiry) {
      continue;
    }

    // server_time_diff is server-minus-local. So it's positive if server is ahead, negative if behind.
    // So we have to subtract the diff from the server time to get the local time.
    // Δ = s - l
    // l = s - Δ
    p.local_time_expiry = p.server_time_expiry->Sub(server_time_diff);
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
