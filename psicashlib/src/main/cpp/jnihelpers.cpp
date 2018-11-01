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
#include "jnihelpers.h"


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

