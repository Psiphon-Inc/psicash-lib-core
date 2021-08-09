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

#ifndef PSICASHLIB_PSICASH_TESTER_H
#define PSICASHLIB_PSICASH_TESTER_H

#include <string>
#include <vector>
#include <map>
#include "psicash.hpp"
#include "error.hpp"

namespace testing {

// If true, tests will use `dev-api.psi.cash`; if false `api.psi.cash`.
constexpr bool DEV_ENV = true;

// Subclass psicash::PsiCash to get access to private members for testing.
// This would probably be done more cleanly with dependency injection, but that
// adds a bunch of overhead for little gain.
class PsiCashTester : public psicash::PsiCash {
  public:
    PsiCashTester();
    virtual ~PsiCashTester();

    // Most tests should use this form of Init. It will use the global flag for `init`.
    psicash::error::Error Init(const std::string& user_agent, const std::string& file_store_root,
                      psicash::MakeHTTPRequestFn make_http_request_fn, bool force_reset);

    // If the `test` flag really must be set explicitly, use this method.
    psicash::error::Error Init(const std::string& user_agent, const std::string& file_store_root,
                      psicash::MakeHTTPRequestFn make_http_request_fn, bool force_reset, bool test);

    psicash::UserData& user_data();

    psicash::error::Error MakeRewardRequests(const std::string& transaction_class,
                                             const std::string& distinguisher,
                                             int repeat=1);

    virtual psicash::error::Result<psicash::HTTPParams>
    BuildRequestParams(const std::string& method, const std::string& path, bool include_auth_tokens,
                       const std::vector<std::pair<std::string, std::string>>& query_params,
                       int attempt,
                       const std::map<std::string, std::string>& additional_headers,
                       const std::string& body) const;

    bool MutatorsEnabled();

    void SetRequestMutators(const std::vector<std::string>& mutators);

    psicash::error::Result<psicash::Purchase> PurchaseFromJSON(const nlohmann::json& j, const std::string& expected_type="") const;
};

} // namespace testing

#endif // PSICASHLIB_PSICASH_TESTER_H
