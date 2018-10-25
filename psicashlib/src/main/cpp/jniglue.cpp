#include <jni.h>
#include <string>
#include <stdio.h>
#include "jnihelpers.h"
#include "psicashlib/error.h"
#include "psicashlib/psicash.h"
#include "vendor/nlohmann/json.hpp"

using json = nlohmann::json;

#define HTTP_REQUEST_FN_NAME    "makeHTTPRequest"
#define HTTP_REQUEST_FN_SIG     "(Ljava/lang/String;)Ljava/lang/String;"

static constexpr const char* kPsiCashUserAgent = "Psiphon-PsiCash-iOS"; // TODO: UPDATE FOR ANDROID

using namespace std;
using namespace psicash;

static jclass g_jClass;
static jmethodID g_makeHTTPRequestMID;
static PsiCash g_psi_cash;


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
    if (!j_file_store_root) {
        return JNI_(ERROR("j_file_store_root is null"));
    }

    auto file_store_root = JStringToString(env, j_file_store_root);
    if (!file_store_root) {
        return JNI_(ERROR("file_store_root is invalid"));
    }

    // We can't set the HTTP requester function yet, as we can't cache `this_obj`.
    auto err = g_psi_cash.Init(kPsiCashUserAgent, file_store_root->c_str(), nullptr, test);
    if (err) {
        return JNI_(WRAP_ERROR1(err, "g_psi_cash.Init failed"));
    }

    return nullptr;
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

    return JNI_(WRAP_ERROR(g_psi_cash.SetRequestMetadataItem(*key, *value)));
}

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

extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeIsAccount(
        JNIEnv* env,
        jobject /*this_obj*/) {
    return JNI_s(SuccessResponse(g_psi_cash.IsAccount()));
}

extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeValidTokenTypes(
        JNIEnv* env,
        jobject /*this_obj*/) {
    auto vtt = g_psi_cash.ValidTokenTypes();
    return JNI_s(SuccessResponse(vtt));
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
        jstring j_params_json) {
    if (!j_params_json) {
        return JNI_(ERROR("j_params_json is null"));
    }

    auto params_json = JStringToString(env, j_params_json);
    if (!params_json) {
        return JNI_(ERROR("j_params_json is invalid"));
    }

    string transaction_class, distinguisher;
    int64_t expected_price;
    try {
        auto j = json::parse(*params_json);
        transaction_class = j["class"].get<string>();
        distinguisher = j["distinguisher"].get<string>();
        expected_price = j["expectedPrice"].get<int64_t>();
    }
    catch (json::exception& e) {
        return JNI_(ERROR(utils::Stringer("params json parse failed: ", e.what(), "; id:", e.id)));
    }

    g_psi_cash.SetHTTPRequestFn(GetHTTPReqFn(env, this_obj));

    auto result = g_psi_cash.NewExpiringPurchase(transaction_class, distinguisher, expected_price);

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

