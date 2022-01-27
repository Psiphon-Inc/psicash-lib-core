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

#ifndef NDEBUG

#include <string>
#include <thread>
#include <iostream>
#include "psicash_tester.hpp"
#include "utils.hpp"
#include "http_status_codes.h"

using namespace std;
using namespace psicash;


namespace testing {

#define TEST_HEADER "X-PsiCash-Test"

// Making this a global rather than PsiCashTester member, because it needs to be modified
// inside a const method. (Tests are not multithreaded, so this is okay. But still ugly.)
static std::vector<std::string> g_request_mutators;


PsiCashTester::PsiCashTester()
    : PsiCash() {
    g_request_mutators.clear();
}

PsiCashTester::~PsiCashTester() {
}

error::Error PsiCashTester::Init(const string& user_agent, const string& file_store_root,
                                 MakeHTTPRequestFn make_http_request_fn, bool force_reset) {
    return Init(user_agent, file_store_root, make_http_request_fn, force_reset, DEV_ENV);
}

error::Error PsiCashTester::Init(const string& user_agent, const string& file_store_root,
                                 MakeHTTPRequestFn make_http_request_fn, bool force_reset, bool test) {
    return PsiCash::Init(user_agent, file_store_root, make_http_request_fn, force_reset, test);
}

UserData& PsiCashTester::user_data() {
    return *user_data_;
}

error::Error PsiCashTester::MakeRewardRequests(const std::string& transaction_class,
                                               const std::string& distinguisher,
                                               int repeat) {
    for (int i = 0; i < repeat; ++i) {
        if (i != 0) {
            // Sleep a bit to avoid server DB transaction conflicts
            this_thread::sleep_for(chrono::milliseconds(100));
        }

        auto result = MakeHTTPRequestWithRetry(
                "POST", "/transaction", true,
                {{"class",         transaction_class},
                 {"distinguisher", distinguisher}},
                nonstd::nullopt);
        if (!result) {
            return WrapError(result.error(), "MakeHTTPRequestWithRetry failed");
        } else if (result->code != kHTTPStatusOK) {
            return error::MakeNoncriticalError(utils::Stringer(
                "1T reward request failed: ", result->code, "; ", result->error, "; ", result->body));
        }
    }
    return error::nullerr;
}

error::Result<HTTPParams>
PsiCashTester::BuildRequestParams(const std::string& method, const std::string& path,
                                  bool include_auth_tokens,
                                  const std::vector<std::pair<std::string, std::string>>& query_params,
                                  int attempt,
                                  const std::map<std::string, std::string>& additional_headers,
                                  const std::string& body) const {
    auto bonus_headers = additional_headers;
    if (!g_request_mutators.empty()) {
        auto mutator = g_request_mutators.back();
        if (!mutator.empty()) {
            bonus_headers[TEST_HEADER] = mutator;
        }
        g_request_mutators.pop_back();
    }

    return PsiCash::BuildRequestParams(
        method, path, include_auth_tokens, query_params, attempt, bonus_headers, body);
}

bool PsiCashTester::MutatorsEnabled() {
    static bool checked = false, mutators_enabled = false;
    if (checked) {
        return mutators_enabled;
    }
    checked = true;

    SetRequestMutators({"CheckEnabled"});
    auto result = MakeHTTPRequestWithRetry(
            "GET", "/refresh-state", false, {}, nonstd::nullopt);
    if (!result) {
        throw std::runtime_error("MUTATOR CHECK FAILED: "s + result.error().ToString());
    }

    mutators_enabled = (result->code == kHTTPStatusAccepted);

    if (!mutators_enabled) {
        std::cout << "SKIPPING MUTATOR TESTS; code: " << result->code << std::endl;
    }

    return mutators_enabled;
}

void PsiCashTester::SetRequestMutators(const std::vector<std::string>& mutators) {
    // We're going to store it reversed so we can pop off the end.
    g_request_mutators.assign(mutators.crbegin(), mutators.crend());
}

psicash::error::Result<psicash::Purchase> PsiCashTester::PurchaseFromJSON(const nlohmann::json& j, const std::string& expected_type) const {
    return PsiCash::PurchaseFromJSON(j, expected_type);
}

std::string PsiCashTester::CommaDelimitTokens(const std::vector<std::string>& types) const {
    return PsiCash::CommaDelimitTokens(types);
}

} // namespace psicash

#endif // NDEBUG
