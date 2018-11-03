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

#include <memory>
#include <functional>
#include "vendor/nlohmann/json.hpp"
#include "jnihelpers.h"

using namespace std;
using json = nlohmann::json;


bool g_testing = false;
jclass g_jGlueClass;
jmethodID g_makeHTTPRequestMID;


psicash::PsiCash& GetPsiCash() {
    static psicash::PsiCash psi_cash;
    static testing::PsiCashTester psi_cash_test;
    if (g_testing) {
        return psi_cash_test;
    }
    return psi_cash;
}

testing::PsiCashTester& GetPsiCashTester() {
    return static_cast<testing::PsiCashTester&>(GetPsiCash());
}


bool CheckJNIException(JNIEnv* env) {
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe(); // writes to logcat
        env->ExceptionClear();
        return true;
    }
    return false;
}

std::function<void(const char*)> StringUTFDeleter(JNIEnv* env, jstring j_param) {
    return [=](const char* s) { env->ReleaseStringUTFChars(j_param, s); };
}

template<typename T>
using deleted_unique_ptr = std::unique_ptr<T, std::function<void(T*)>>;

nonstd::optional<std::string> JStringToString(JNIEnv* env, jstring j_s) {
    if (!j_s) {
        return nonstd::nullopt;
    }

    deleted_unique_ptr<const char> s(env->GetStringUTFChars(j_s, NULL), StringUTFDeleter(env, j_s));
    return std::string(s.get());
}

string ErrorResponseFallback(const string& message) {
    return "{\"error\":{\"message\":\""s + message + "\", \"internal\":true}}";
}

string ErrorResponse(const string& message,
                     const string& filename, const string& function, int line,
                     bool internal/*=false*/) {
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

string ErrorResponse(const error::Error& error, const string& message,
                     const string& filename, const string& function, int line,
                     bool internal/*=false*/) {
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

string SuccessResponse() {
    return SuccessResponse(nullptr);
}

psicash::MakeHTTPRequestFn GetHTTPReqFn(JNIEnv* env, jobject& this_obj) {
    psicash::MakeHTTPRequestFn http_req_fn = [env, &this_obj = this_obj](const string& params) -> string {
        json stub_result = {{"status", -1},
                            {"error",  nullptr},
                            {"body",   nullptr},
                            {"date",   nullptr}};

        auto jParams = env->NewStringUTF(params.c_str());
        if (!jParams) {
            CheckJNIException(env);
            stub_result["error"] = MakeError("NewStringUTF failed").ToString();
            return stub_result.dump(-1, ' ', true);
        }

        auto jResult = (jstring)env->CallObjectMethod(this_obj, g_makeHTTPRequestMID, jParams);
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
