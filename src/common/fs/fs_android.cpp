// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/fs/fs_android.h"

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

#define FR(FunctionName, ReturnValue, JMethodID, Caller, JMethodName, Signature)                   \
    F(JMethodID, JMethodName, Signature)
#define FS(FunctionName, ReturnValue, Parameters, JMethodID, JMethodName, Signature)               \
    F(JMethodID, JMethodName, Signature)
#define F(JMethodID, JMethodName, Signature)                                                       \
    JMethodID = env->GetStaticMethodID(native_library, JMethodName, Signature);
    ANDROID_SINGLE_PATH_DETERMINE_FUNCTIONS(FR)
    ANDROID_STORAGE_FUNCTIONS(FS)
#undef F
#undef FS
#undef FR
}

void UnRegisterCallbacks() {
#define FR(FunctionName, ReturnValue, JMethodID, Caller, JMethodName, Signature) F(JMethodID)
#define FS(FunctionName, ReturnValue, Parameters, JMethodID, JMethodName, Signature) F(JMethodID)
#define F(JMethodID) JMethodID = nullptr;
    ANDROID_SINGLE_PATH_DETERMINE_FUNCTIONS(FR)
    ANDROID_STORAGE_FUNCTIONS(FS)
#undef F
#undef FS
#undef FR
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

} // namespace Common::FS::Android
