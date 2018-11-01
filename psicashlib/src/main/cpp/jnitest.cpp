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

#include <string>
#include "jnitest.h"
#include "http_status_codes.h"

using namespace std;

error::Error PsiCashTest::TestReward(const string& transaction_class, const string& distinguisher) {
    auto result = MakeHTTPRequestWithRetry(
            "POST", "/transaction", true,
            {{"class",         transaction_class},
             {"distinguisher", distinguisher}});
    if (!result) {
        return WrapError(result.error(), "MakeHTTPRequestWithRetry failed");
    } else if (result->code != kHTTPStatusOK) {
        return MakeError(
                utils::Stringer("reward request failed: ", result->code, "; ", result->error, "; ", result->body));
    }

    return error::nullerr;
}
