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

#ifndef PSICASHLIB_JNIHELPERS_H
#define PSICASHLIB_JNIHELPERS_H

#include <string>
#include <jni.h>
#include <vendor/nlohmann/json.hpp>
#include <psicash_tester.h>
#include "vendor/nonstd/optional.hpp"
#include "error.h"
#include "psicash.h"


/// Indicates if we're running in testing mode.
extern bool g_testing;
extern jclass g_jGlueClass;
extern jmethodID g_makeHTTPRequestMID;

/// Get PsiCash instance to use (might actually be PsiCashTester).
psicash::PsiCash& GetPsiCash();
testing::PsiCashTester& GetPsiCashTester();

/// CheckJNIException returns false if there was no outstanding JNI exception, or returns true if
/// there was, in addition to clearing it (allowing for further JNI operations).
bool CheckJNIException(JNIEnv* env);

nonstd::optional<std::string> JStringToString(JNIEnv* env, jstring j_s);

/// Creates a JSON error string appropriate for a JNI response.
/// If `message` is empty, the result will be a non-error.
std::string ErrorResponse(const std::string& message,
                     const std::string& filename, const std::string& function, int line,
                     bool internal = false);

/// Used to return a JSON error without any potential marshaling exceptions.
std::string ErrorResponseFallback(const std::string& message);

/// Creates a JSON error string appropriate for a JNI response. `error` is wrapped.
/// If `error` is a non-error, the result will be a non-error.
std::string ErrorResponse(const error::Error& error, const std::string& message,
                     const std::string& filename, const std::string& function, int line,
                     bool internal = false);

#define ERROR(msg)                      (ErrorResponse(msg, __FILE__, __PRETTY_FUNCTION__, __LINE__).c_str())
#define ERROR_INTERNAL(msg)             (ErrorResponse(msg, __FILE__, __PRETTY_FUNCTION__, __LINE__, true).c_str())
#define WRAP_ERROR1(err, msg)           (ErrorResponse(err, msg, __FILE__, __PRETTY_FUNCTION__, __LINE__).c_str())
#define WRAP_ERROR(err)                 WRAP_ERROR1(err, "")
#define WRAP_ERROR1_INTERNAL(err, msg)  (ErrorResponse(err, msg, __FILE__, __PRETTY_FUNCTION__, __LINE__, true).c_str())
#define WRAP_ERROR_INTERNAL(err)        WRAP_ERROR1_INTERNAL(err, "")
#define JNI_(str)                       (str ? env->NewStringUTF(str) : nullptr)
#define JNI_s(str)                      (!str.empty() ? env->NewStringUTF(str.c_str()) : nullptr)

/// Create a JNI success response.
template<typename T>
std::string SuccessResponse(T res) {
    try {
        nlohmann::json j({{"result", res}});
        return j.dump(-1, ' ', true);
    }
    catch (nlohmann::json::exception& e) {
        return ERROR(
                utils::Stringer("SuccessResponse json dump failed: ", e.what(), "; id:", e.id).c_str());
    }
}

/// To be used for successful responses that don't have a result payload.
std::string SuccessResponse();

// Note that the function returned by this is only valid as long as these arguments are valid.
// So, generally, it should only be used for the duration of a single JNI call.
psicash::MakeHTTPRequestFn GetHTTPReqFn(JNIEnv* env, jobject& this_obj);

#endif //PSICASHLIB_JNIHELPERS_H
