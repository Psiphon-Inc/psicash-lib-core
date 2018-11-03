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

#include <jni.h>
#include <string>
#include <stdio.h>
#include "jnihelpers.hpp"
#include "psicash_tester.hpp"
#include "error.hpp"
#include "psicash.hpp"
#include "vendor/nlohmann/json.hpp"

using json = nlohmann::json;

#define HTTP_REQUEST_FN_NAME    "makeHTTPRequest"
#define HTTP_REQUEST_FN_SIG     "(Ljava/lang/String;)Ljava/lang/String;"

static constexpr const char* kPsiCashUserAgent = "Psiphon-PsiCash-iOS"; // TODO: UPDATE FOR ANDROID

using namespace std;
using namespace psicash;


extern "C" JNIEXPORT jboolean
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeStaticInit(JNIEnv* env, jclass type) {
    g_jGlueClass = reinterpret_cast<jclass>(env->NewGlobalRef(type));

    g_makeHTTPRequestMID = env->GetMethodID(g_jGlueClass, HTTP_REQUEST_FN_NAME, HTTP_REQUEST_FN_SIG);
    if (!g_makeHTTPRequestMID) {
        CheckJNIException(env);
        return static_cast<jboolean>(false);
    }

    return static_cast<jboolean>(true);
}

// Returns null on success or an error message on failure.
extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeObjectInit(
        JNIEnv* env,
        jobject /*this_obj*/,
        jstring j_file_store_root,
        jboolean test) {
    g_testing = test;

    if (!j_file_store_root) {
        return JNI_(ERROR("j_file_store_root is null"));
    }

    auto file_store_root = JStringToString(env, j_file_store_root);
    if (!file_store_root) {
        return JNI_(ERROR("file_store_root is invalid"));
    }

    // We can't set the HTTP requester function yet, as we can't cache `this_obj`.
    auto err = GetPsiCash().Init(kPsiCashUserAgent, file_store_root->c_str(), nullptr, test);
    if (err) {
        return JNI_(WRAP_ERROR1(err, "PsiCash.Init failed"));
    }

    return JNI_s(SuccessResponse());
}

extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_SetRequestMetadataItem(
        JNIEnv* env,
        jobject /*this_obj*/,
        jstring j_key,
        jstring j_value) {
    auto key = JStringToString(env, j_key);
    auto value = JStringToString(env, j_value);
    if (!key || !value) {
        return JNI_(ERROR("key and value must be non-null"));
    }

    return JNI_(WRAP_ERROR(GetPsiCash().SetRequestMetadataItem(*key, *value)));
}

extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeIsAccount(
        JNIEnv* env,
        jobject /*this_obj*/) {
    return JNI_s(SuccessResponse(GetPsiCash().IsAccount()));
}

extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeValidTokenTypes(
        JNIEnv* env,
        jobject /*this_obj*/) {
    auto vtt = GetPsiCash().ValidTokenTypes();
    return JNI_s(SuccessResponse(vtt));
}

extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeBalance(
        JNIEnv* env,
        jobject /*this_obj*/) {
    auto b = GetPsiCash().Balance();
    return JNI_s(SuccessResponse(b));
}

extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeGetPurchasePrices(
        JNIEnv* env,
        jobject /*this_obj*/) {
    auto pp = GetPsiCash().GetPurchasePrices();
    return JNI_s(SuccessResponse(pp));
}

extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeGetPurchases(
        JNIEnv* env,
        jobject /*this_obj*/) {
    auto p = GetPsiCash().GetPurchases();
    return JNI_s(SuccessResponse(p));
}

extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeValidPurchases(
        JNIEnv* env,
        jobject /*this_obj*/) {
    auto p = GetPsiCash().ValidPurchases();
    return JNI_s(SuccessResponse(p));
}

extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeNextExpiringPurchase(
        JNIEnv* env,
        jobject /*this_obj*/) {
    auto p = GetPsiCash().NextExpiringPurchase();
    if (!p) {
        return JNI_s(SuccessResponse(nullptr));
    }
    return JNI_s(SuccessResponse(*p));
}

extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeExpirePurchases(
        JNIEnv* env,
        jobject /*this_obj*/) {
    auto result = GetPsiCash().ExpirePurchases();
    if (!result) {
        return JNI_(WRAP_ERROR(result.error()));
    }
    return JNI_s(SuccessResponse(*result));
}

extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeRemovePurchases(
        JNIEnv* env,
        jobject /*this_obj*/,
        jobjectArray transaction_ids) {
    if (!transaction_ids) {
        return JNI_s(SuccessResponse());
    }

    int id_count = env->GetArrayLength(transaction_ids);
    if (id_count == 0) {
        return JNI_s(SuccessResponse());
    }

    vector<TransactionID> ids;
    for (int i = 0; i < id_count; ++i) {
        auto id = JStringToString(env, (jstring)(env->GetObjectArrayElement(transaction_ids, i)));
        if (id) {
            ids.push_back(*id);
        }
    }

    auto err = GetPsiCash().RemovePurchases(ids);
    if (err) {
        return JNI_(WRAP_ERROR(err));
    }
    return JNI_s(SuccessResponse());
}

extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeModifyLandingPage(
        JNIEnv* env,
        jobject /*this_obj*/,
        jstring j_url) {
    auto url = JStringToString(env, j_url);
    if (!url) {
        return JNI_(ERROR("url is required"));
    }

    auto result = GetPsiCash().ModifyLandingPage(*url);
    if (!result) {
        return JNI_(WRAP_ERROR(result.error()));
    }
    return JNI_s(SuccessResponse(*result));
}

extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeGetRewardedActivityData(
        JNIEnv* env,
        jobject /*this_obj*/) {
    auto result = GetPsiCash().GetRewardedActivityData();
    if (!result) {
        return JNI_(WRAP_ERROR(result.error()));
    }
    return JNI_s(SuccessResponse(*result));
}

extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeGetDiagnosticInfo(
        JNIEnv* env,
        jobject /*this_obj*/) {
    auto json = GetPsiCash().GetDiagnosticInfo();
    return JNI_s(SuccessResponse(json.dump(-1, ' ', true)));
}

/*
 * Response JSON structure is:
 * {
 *      error: { ... },
 *      result: {
 *          status: Status value,
 *          purchase: Purchase; valid iff status == Status::Success
 *      }
 * }
 */
extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeRefreshState(
        JNIEnv* env,
        jobject this_obj,
        jobjectArray j_purchase_classes) {
    vector<string> purchase_classes;

    int purchase_classes_count = env->GetArrayLength(j_purchase_classes);
    for (int i = 0; i < purchase_classes_count; i++) {
        auto purchase_class = JStringToString(env, (jstring)env->GetObjectArrayElement(j_purchase_classes, i));
        if (purchase_class) {
            purchase_classes.push_back(*purchase_class);
        }
    }

    GetPsiCash().SetHTTPRequestFn(GetHTTPReqFn(env, this_obj));

    auto result = GetPsiCash().RefreshState(purchase_classes);
    if (!result) {
        return JNI_(WRAP_ERROR(result.error()));
    }

    return JNI_s(SuccessResponse(*result));
}

/*
 * Response JSON structure is:
 * {
 *      error: { ... },
 *      result: {
 *          status: Status value,
 *          purchase: Purchase; valid iff status == Status::Success
 *      }
 * }
 */
extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeNewExpiringPurchase(
        JNIEnv* env,
        jobject this_obj,
        jstring j_transaction_class,
        jstring j_distinguisher,
        jlong j_expected_price) {
    auto transaction_class = JStringToString(env, j_transaction_class);
    auto distinguisher = JStringToString(env, j_distinguisher);
    int64_t expected_price = j_expected_price;

    if (!transaction_class || !distinguisher) {
        return JNI_(ERROR("transaction and distinguisher are required"));
    }

    GetPsiCash().SetHTTPRequestFn(GetHTTPReqFn(env, this_obj));

    auto result = GetPsiCash().NewExpiringPurchase(*transaction_class, *distinguisher, expected_price);
    if (!result) {
        return JNI_(WRAP_ERROR(result.error()));
    }

    auto output = json::object({{"status",   result->status},
                                {"purchase", nullptr}});
    if (result->purchase) {
        output["purchase"] = *result->purchase;
    }

    return JNI_s(SuccessResponse(output));
}
