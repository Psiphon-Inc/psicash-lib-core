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

namespace testing {

// Subclass psicash::PsiCash to get access to private members for testing.
// This would probably be done more cleanly with dependency injection, but that
// adds a bunch of overhead for little gain.
class PsiCashTester : public psicash::PsiCash {
  public:
    PsiCashTester();
    virtual ~PsiCashTester();

    psicash::UserData& user_data();

    error::Error MakeRewardRequests(const std::string& transaction_class,
                                    const std::string& distinguisher,
                                    int repeat=1);

    virtual error::Result<std::string>
    BuildRequestParams(const std::string& method, const std::string& path, bool include_auth_tokens,
                       const std::vector<std::pair<std::string, std::string>>& query_params,
                       int attempt,
                       const std::map<std::string, std::string>& additional_headers) const;

    bool MutatorsEnabled();

    void SetRequestMutators(const std::vector<std::string>& mutators);

private:
    bool mutators_enabled_;
};

} // namespace testing

#endif // PSICASHLIB_PSICASH_TESTER_H
