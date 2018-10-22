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
using deleted_unique_ptr = std::unique_ptr<T,std::function<void(T*)>>;

nonstd::optional<std::string> JStringToString(JNIEnv* env, jstring j_s) {
    if (!j_s) {
        return nonstd::nullopt;
    }

    deleted_unique_ptr<const char> s(env->GetStringUTFChars(j_s, NULL), StringUTFDeleter(env, j_s));
    return std::string(s.get());
}

