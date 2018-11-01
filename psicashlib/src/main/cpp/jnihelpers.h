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
#include "vendor/nonstd/optional.hpp"

/// CheckJNIException returns false if there was no outstanding JNI exception, or returns true if
/// there was, in addition to clearing it (allowing for further JNI operations).
bool CheckJNIException(JNIEnv* env);

nonstd::optional<std::string> JStringToString(JNIEnv* env, jstring j_s);

#endif //PSICASHLIB_JNIHELPERS_H
