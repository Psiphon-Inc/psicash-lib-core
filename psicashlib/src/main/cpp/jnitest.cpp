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
#include "jnihelpers.h"
#include "http_status_codes.h"

using namespace std;
using namespace psicash;

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

// Returns null on success, error message otherwise.
extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeTestReward(
        JNIEnv* env,
        jobject this_obj,
        jstring j_transaction_class,
        jstring j_distinguisher) {
    auto transaction_class = JStringToString(env, j_transaction_class);
    auto distinguisher = JStringToString(env, j_distinguisher);

    if (!transaction_class || !distinguisher) {
        return JNI_("transaction and distinguisher are required");
    }

    GetPsiCash().SetHTTPRequestFn(GetHTTPReqFn(env, this_obj));

    auto err = GetPsiCashTester().MakeRewardRequests(*transaction_class, *distinguisher);
    if (err) {
        return JNI_s(err.ToString());
    }

    return nullptr;
}

extern "C" JNIEXPORT jboolean
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeTestSetRequestMutators(
        JNIEnv* env,
        jobject this_obj,
        jobjectArray j_mutators) {
    GetPsiCash().SetHTTPRequestFn(GetHTTPReqFn(env, this_obj));
    if (!GetPsiCashTester().MutatorsEnabled()) {
        return false;
    }

    int mutator_count = j_mutators ? env->GetArrayLength(j_mutators) : 0;
    if (mutator_count == 0) {
        return true;
    }

    vector<string> mutators;
    for (int i = 0; i < mutator_count; ++i) {
        auto m = JStringToString(env, (jstring)(env->GetObjectArrayElement(j_mutators, i)));
        if (m) {
            mutators.push_back(*m);
        }
    }

    GetPsiCashTester().SetRequestMutators(mutators);

    return true;
}
