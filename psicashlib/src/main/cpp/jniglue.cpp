#include <jni.h>
#include <string>
#include <stdio.h>
#include "error.h"
#include "psicashlib/psicash.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

#define HTTP_REQUEST_FN_NAME    "makeHTTPRequest"
#define HTTP_REQUEST_FN_SIG     "(Ljava/lang/String;)Ljava/lang/String;"

using namespace std;
using namespace psicash;

static jclass g_jClass;
static jmethodID g_makeHTTPRequestMID;
static PsiCash g_psiCash;

jstring jErrorMsg(JNIEnv *env, const char *msg, const char *func, int line) {
  auto em = ErrorMsg(msg, func, line);
  return env->NewStringUTF(em.c_str());
}

// CheckJNIException returns false if there was no outstanding JNI exception, or returns true if
// there was, in addition to clearing it (allowing for further JNI operations).
bool CheckJNIException(JNIEnv *env) {
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe(); // writes to logcat
    env->ExceptionClear();
    return true;
  }
  return false;
}

// Note that the function returned by this is only valid as long as these arguments are valid.
// So, generally, it should only be used for the duration of a single JNI call.
MakeHTTPRequestFn GetHTTPReqFn(JNIEnv *env, jobject& this_obj) {
  MakeHTTPRequestFn http_req_fn = [env, &this_obj = this_obj](const string& params) -> string {
    auto jParams = env->NewStringUTF(params.c_str());
    if (!jParams) {
      CheckJNIException(env);
      return ErrorMsg("NewStringUTF failed", __PRETTY_FUNCTION__, __LINE__);
    }

    auto jResult = (jstring) env->CallObjectMethod(this_obj, g_makeHTTPRequestMID, jParams);
    if (!jResult) {
      CheckJNIException(env);
      return ErrorMsg("CallObjectMethod failed", __PRETTY_FUNCTION__, __LINE__);
    }

    auto resultCString = env->GetStringUTFChars(jResult, NULL);
    if (!resultCString) {
      CheckJNIException(env);
      return ErrorMsg("GetStringUTFChars failed", __PRETTY_FUNCTION__, __LINE__);
    }

    auto result = string(resultCString);
    env->ReleaseStringUTFChars(jResult, resultCString);

    return result;
  };

  return http_req_fn;
}

extern "C" JNIEXPORT jboolean
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NativeStaticInit(JNIEnv *env, jclass type) {
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
    JNIEnv *env,
    jobject /*this_obj*/,
    jstring file_store_root) {
  if (file_store_root == nullptr) {
    return env->NewStringUTF("file_store_root is null");
  }

  auto file_store_root_str = env->GetStringUTFChars(file_store_root, NULL);

  if (file_store_root_str == nullptr) {
    return env->NewStringUTF("file_store_root_str is null");
  }

  // We can't set the HTTP requester function yet, as we can't cache `this_obj`.
  auto err = g_psiCash.Init(file_store_root_str, nullptr);

  env->ReleaseStringUTFChars(file_store_root, file_store_root_str);

  if (err) {
    err = WrapError(err, "g_psiCash.Init failed");
    return env->NewStringUTF(err.ToString().c_str());
  }

  return nullptr;
}

extern "C" JNIEXPORT jstring
JNICALL
Java_ca_psiphon_psicashlib_PsiCashLib_NewExpiringPurchase(
    JNIEnv *env,
    jobject this_obj,
    jstring j_params_json) {
  if (!j_params_json) {
    return jErrorMsg(env, "j_params_json is null", __PRETTY_FUNCTION__, __LINE__);
  }

  auto c_params_json = env->GetStringUTFChars(j_params_json, NULL);
  if (!c_params_json) {
    return jErrorMsg(env, "GetStringUTFChars failed", __PRETTY_FUNCTION__, __LINE__);
  }

  auto params_json = string(c_params_json);
  env->ReleaseStringUTFChars(j_params_json, c_params_json);

  string transaction_class, distinguisher;
  int64_t expected_price;
  try {
    auto j = json::parse(params_json);
    transaction_class = j["class"].get<string>();
    distinguisher = j["distinguisher"].get<string>();
    expected_price = j["expectedPrice"].get<int64_t>();
  }
  catch (json::exception& e) {
    auto err = MakeError(utils::Stringer("params json parse failed: ", e.what(), "; id:", e.id).c_str());
    // TODO: Error structure format
    return env->NewStringUTF(err.ToString().c_str());
  }

  g_psiCash.SetHTTPRequestFn(GetHTTPReqFn(env, this_obj));

  auto result = g_psiCash.NewExpiringPurchase(transaction_class, distinguisher, expected_price);

  if (!result) {
    // TODO
    return env->NewStringUTF("error");
  }

  // TODO
  return env->NewStringUTF("success");
}

