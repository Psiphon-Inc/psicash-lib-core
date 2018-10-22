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
