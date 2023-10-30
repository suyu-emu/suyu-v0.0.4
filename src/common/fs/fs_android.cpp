// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/fs/fs_android.h"
#include "common/string_util.h"

namespace Common::FS::Android {

JNIEnv* GetEnvForThread() {
    thread_local static struct OwnedEnv {
        OwnedEnv() {
            status = g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
            if (status == JNI_EDETACHED)
                g_jvm->AttachCurrentThread(&env, nullptr);
        }

        ~OwnedEnv() {
            if (status == JNI_EDETACHED)
                g_jvm->DetachCurrentThread();
        }

        int status;
        JNIEnv* env = nullptr;
    } owned;
    return owned.env;
}

void RegisterCallbacks(JNIEnv* env, jclass clazz) {
    env->GetJavaVM(&g_jvm);
    native_library = clazz;

#define FH(FunctionName, JMethodID, Caller, JMethodName, Signature)                                \
    F(JMethodID, JMethodName, Signature)
#define FR(FunctionName, ReturnValue, JMethodID, Caller, JMethodName, Signature)                   \
    F(JMethodID, JMethodName, Signature)
#define FS(FunctionName, ReturnValue, Parameters, JMethodID, JMethodName, Signature)               \
    F(JMethodID, JMethodName, Signature)
#define F(JMethodID, JMethodName, Signature)                                                       \
    JMethodID = env->GetStaticMethodID(native_library, JMethodName, Signature);
    ANDROID_SINGLE_PATH_HELPER_FUNCTIONS(FH)
    ANDROID_SINGLE_PATH_DETERMINE_FUNCTIONS(FR)
    ANDROID_STORAGE_FUNCTIONS(FS)
#undef F
#undef FS
#undef FR
#undef FH
}

void UnRegisterCallbacks() {
#define FH(FunctionName, JMethodID, Caller, JMethodName, Signature) F(JMethodID)
#define FR(FunctionName, ReturnValue, JMethodID, Caller, JMethodName, Signature) F(JMethodID)
#define FS(FunctionName, ReturnValue, Parameters, JMethodID, JMethodName, Signature) F(JMethodID)
#define F(JMethodID) JMethodID = nullptr;
    ANDROID_SINGLE_PATH_HELPER_FUNCTIONS(FH)
    ANDROID_SINGLE_PATH_DETERMINE_FUNCTIONS(FR)
    ANDROID_STORAGE_FUNCTIONS(FS)
#undef F
#undef FS
#undef FR
#undef FH
}

bool IsContentUri(const std::string& path) {
    constexpr std::string_view prefix = "content://";
    if (path.size() < prefix.size()) [[unlikely]] {
        return false;
    }

    return path.find(prefix) == 0;
}

int OpenContentUri(const std::string& filepath, OpenMode openmode) {
    if (open_content_uri == nullptr)
        return -1;

    const char* mode = "";
    switch (openmode) {
    case OpenMode::Read:
        mode = "r";
        break;
    default:
        UNIMPLEMENTED();
        return -1;
    }
    auto env = GetEnvForThread();
    jstring j_filepath = env->NewStringUTF(filepath.c_str());
    jstring j_mode = env->NewStringUTF(mode);
    return env->CallStaticIntMethod(native_library, open_content_uri, j_filepath, j_mode);
}

#define FR(FunctionName, ReturnValue, JMethodID, Caller, JMethodName, Signature)                   \
    F(FunctionName, ReturnValue, JMethodID, Caller)
#define F(FunctionName, ReturnValue, JMethodID, Caller)                                            \
    ReturnValue FunctionName(const std::string& filepath) {                                        \
        if (JMethodID == nullptr) {                                                                \
            return 0;                                                                              \
        }                                                                                          \
        auto env = GetEnvForThread();                                                              \
        jstring j_filepath = env->NewStringUTF(filepath.c_str());                                  \
        return env->Caller(native_library, JMethodID, j_filepath);                                 \
    }
ANDROID_SINGLE_PATH_DETERMINE_FUNCTIONS(FR)
#undef F
#undef FR

#define FH(FunctionName, JMethodID, Caller, JMethodName, Signature)                                \
    F(FunctionName, JMethodID, Caller)
#define F(FunctionName, JMethodID, Caller)                                                         \
    std::string FunctionName(const std::string& filepath) {                                        \
        if (JMethodID == nullptr) {                                                                \
            return 0;                                                                              \
        }                                                                                          \
        auto env = GetEnvForThread();                                                              \
        jstring j_filepath = env->NewStringUTF(filepath.c_str());                                  \
        jstring j_return =                                                                         \
            static_cast<jstring>(env->Caller(native_library, JMethodID, j_filepath));              \
        if (!j_return) {                                                                           \
            return {};                                                                             \
        }                                                                                          \
        const jchar* jchars = env->GetStringChars(j_return, nullptr);                              \
        const jsize length = env->GetStringLength(j_return);                                       \
        const std::u16string_view string_view(reinterpret_cast<const char16_t*>(jchars), length);  \
        const std::string converted_string = Common::UTF16ToUTF8(string_view);                     \
        env->ReleaseStringChars(j_return, jchars);                                                 \
        return converted_string;                                                                   \
    }
ANDROID_SINGLE_PATH_HELPER_FUNCTIONS(FH)
#undef F
#undef FH

} // namespace Common::FS::Android
