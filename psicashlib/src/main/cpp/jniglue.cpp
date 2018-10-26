#include <jni.h>
#include <string>
#include <stdio.h>
#include "jnihelpers.h"
#include "jnitest.h"
#include "psicashlib/error.h"
#include "psicashlib/psicash.h"
#include "vendor/nlohmann/json.hpp"

using json = nlohmann::json;

#define HTTP_REQUEST_FN_NAME    "makeHTTPRequest"
#define HTTP_REQUEST_FN_SIG     "(Ljava/lang/String;)Ljava/lang/String;"

static constexpr const char* kPsiCashUserAgent = "Psiphon-PsiCash-iOS"; // TODO: UPDATE FOR ANDROID

using namespace std;
using namespace psicash;

static bool g_test = false;
static jclass g_jClass;
static jmethodID g_makeHTTPRequestMID;

static PsiCash& GetPsiCash() {
    static PsiCash psi_cash;
    static PsiCash psi_cash_test;
    return g_test ? psi_cash_test : psi_cash;
}

static PsiCashTest& GetPsiCashTest() {
    return (PsiCashTest&)GetPsiCash();
}


/// Used to return a JSON error without any potential marshaling exceptions.
string ErrorResponseFallback(const string& message) {
    return "{\"error\":{\"message\":"s + message + "\", \"internal\":true}}";
}

/// Creates a JSON error string appropriate for a JNI response.
/// If `message` is empty, the result will be a non-error.
string ErrorResponse(const string& message,
                     const string& filename, const string& function, int line,
                     bool internal=false) {
    try {
        json j({{"error", nullptr}});
        if (!message.empty()) {
            j["error"]["message"] = error::Error(message, filename, function, line).ToString();
            j["error"]["internal"] = internal;
        }
        return j.dump(-1, ' ', true);
    }
    catch (json::exception& e) {
        return ErrorResponseFallback(
                utils::Stringer("ErrorResponse json dump failed: ", e.what(), "; id:", e.id).c_str());
    }
}

/// Creates a JSON error string appropriate for a JNI response. `error` is wrapped.
/// If `error` is a non-error, the result will be a non-error.
string ErrorResponse(const error::Error& error, const string& message,
                     const string& filename, const string& function, int line,
                     bool internal=false) {
    try {
        json j({{"error", nullptr}});
        if (error) {
            j["error"]["message"] = error::Error(error).Wrap(message, filename, function, line).ToString();
            j["error"]["internal"] = internal;
        }
        return j.dump(-1, ' ', true);
    }
    catch (json::exception& e) {
        return ErrorResponseFallback(
                utils::Stringer("ErrorResponse json dump failed: ", e.what(), "; id:", e.id).c_str());
    }
}

#define ERROR(msg)                      (ErrorResponse(msg, __FILE__, __PRETTY_FUNCTION__, __LINE__).c_str())
#define ERROR_INTERNAL(msg)             (ErrorResponse(msg, __FILE__, __PRETTY_FUNCTION__, __LINE__, true).c_str())
#define WRAP_ERROR1(err, msg)           (ErrorResponse(err, msg, __FILE__, __PRETTY_FUNCTION__, __LINE__).c_str())
#define WRAP_ERROR(err)                 WRAP_ERROR1(err, "")
#define WRAP_ERROR1_INTERNAL(err, msg)  (ErrorResponse(err, msg, __FILE__, __PRETTY_FUNCTION__, __LINE__, true).c_str())
#define WRAP_ERROR_INTERNAL(err)        WRAP_ERROR1_INTERNAL(err, "")
#define JNI_(str)                       (str ? env->NewStringUTF(str) : nullptr)
#define JNI_s(str)                      (!str.empty() ? env->NewStringUTF(str.c_str()) : nullptr)

template<typename T>
string SuccessResponse(T res) {
    try {
        json j({{"result", res}});
        return j.dump(-1, ' ', true);
    }
    catch (json::exception& e) {
        return ERROR(
                utils::Stringer("SuccessResponse json dump failed: ", e.what(), "; id:", e.id).c_str());
    }
}

// To be used for successful responses that don't have a result payload.
string SuccessResponse() {
    return SuccessResponse(nullptr);
}


// Note that the function returned by this is only valid as long as these arguments are valid.
// So, generally, it should only be used for the duration of a single JNI call.
MakeHTTPRequestFn GetHTTPReqFn(JNIEnv* env, jobject& this_obj) {
    MakeHTTPRequestFn http_req_fn = [env, &this_obj = this_obj](const string& params) -> string {
        json stub_result = {{"status", -1}, {"error", nullptr}, {"body", nullptr}, {"date", nullptr}};

        auto jParams = env->NewStringUTF(params.c_str());
        if (!jParams) {
            CheckJNIException(env);
            stub_result["error"] = MakeError("NewStringUTF failed").ToString();
            return stub_result.dump(-1, ' ', true);
        }

        auto jResult = (jstring) env->CallObjectMethod(this_obj, g_makeHTTPRequestMID, jParams);
        if (!jResult) {
            CheckJNIException(env);
            stub_result["error"] = MakeError("CallObjectMethod failed").ToString();
            return stub_result.dump(-1, ' ', true);
        }

        auto resultCString = env->GetStringUTFChars(jResult, NULL);
        if (!resultCString) {
            CheckJNIException(env);
            stub_result["error"] = MakeError("GetStringUTFChars failed").ToString();
            return stub_result.dump(-1, ' ', true);
        }

        auto result = string(resultCString);
        env->ReleaseStringUTFChars(jResult, resultCString);

        return result;
    };

    return http_req_fn;
}

extern "C" JNIEXPORT jboolean
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeStaticInit(JNIEnv* env, jclass type) {
    g_jClass = reinterpret_cast<jclass>(env->NewGlobalRef(type));

    g_makeHTTPRequestMID = env->GetMethodID(g_jClass, HTTP_REQUEST_FN_NAME, HTTP_REQUEST_FN_SIG);
    if (!g_makeHTTPRequestMID) {
        CheckJNIException(env);
        return false;
    }

    return true;
}

// Returns null on success or an error message on failure.
extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeObjectInit(
        JNIEnv* env,
        jobject /*this_obj*/,
        jstring j_file_store_root,
        jboolean test) {
    g_test = test;

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
    return JNI_s(SuccessResponse(GetPsiCash().Balance()));
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

    auto err = GetPsiCashTest().TestReward(*transaction_class, *distinguisher);
    if (err) {
        return JNI_s(err.ToString());
    }

    return nullptr;
}