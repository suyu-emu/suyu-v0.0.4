// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include "common/fs/fs_android.h"
#include "jni/id_cache.h"

static JavaVM* s_java_vm;
static jclass s_native_library_class;
static jmethodID s_exit_emulation_activity;

static constexpr jint JNI_VERSION = JNI_VERSION_1_6;

namespace IDCache {

JNIEnv* GetEnvForThread() {
    thread_local static struct OwnedEnv {
        OwnedEnv() {
            status = s_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
            if (status == JNI_EDETACHED)
                s_java_vm->AttachCurrentThread(&env, nullptr);
        }

        ~OwnedEnv() {
            if (status == JNI_EDETACHED)
                s_java_vm->DetachCurrentThread();
        }

        int status;
        JNIEnv* env = nullptr;
    } owned;
    return owned.env;
}

jclass GetNativeLibraryClass() {
    return s_native_library_class;
}

jmethodID GetExitEmulationActivity() {
    return s_exit_emulation_activity;
}

} // namespace IDCache

#ifdef __cplusplus
extern "C" {
#endif

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    s_java_vm = vm;

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK)
        return JNI_ERR;

    // Initialize Java classes
    const jclass native_library_class = env->FindClass("org/yuzu/yuzu_emu/NativeLibrary");
    s_native_library_class = reinterpret_cast<jclass>(env->NewGlobalRef(native_library_class));
    s_exit_emulation_activity =
        env->GetStaticMethodID(s_native_library_class, "exitEmulationActivity", "(I)V");

    // Initialize Android Storage
    Common::FS::Android::RegisterCallbacks(env, s_native_library_class);

    return JNI_VERSION;
}

void JNI_OnUnload(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK) {
        return;
    }

    // UnInitialize Android Storage
    Common::FS::Android::UnRegisterCallbacks();
    env->DeleteGlobalRef(s_native_library_class);
}

#ifdef __cplusplus
}
#endif
