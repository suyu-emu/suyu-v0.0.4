// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include "common/assert.h"
#include "common/fs/fs_android.h"
#include "jni/applets/software_keyboard.h"
#include "jni/id_cache.h"
#include "video_core/rasterizer_interface.h"

static JavaVM* s_java_vm;
static jclass s_native_library_class;
static jclass s_disk_cache_progress_class;
static jclass s_load_callback_stage_class;
static jclass s_game_dir_class;
static jmethodID s_game_dir_constructor;
static jmethodID s_exit_emulation_activity;
static jmethodID s_disk_cache_load_progress;
static jmethodID s_on_emulation_started;
static jmethodID s_on_emulation_stopped;

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

jclass GetDiskCacheProgressClass() {
    return s_disk_cache_progress_class;
}

jclass GetDiskCacheLoadCallbackStageClass() {
    return s_load_callback_stage_class;
}

jclass GetGameDirClass() {
    return s_game_dir_class;
}

jmethodID GetGameDirConstructor() {
    return s_game_dir_constructor;
}

jmethodID GetExitEmulationActivity() {
    return s_exit_emulation_activity;
}

jmethodID GetDiskCacheLoadProgress() {
    return s_disk_cache_load_progress;
}

jmethodID GetOnEmulationStarted() {
    return s_on_emulation_started;
}

jmethodID GetOnEmulationStopped() {
    return s_on_emulation_stopped;
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
    s_disk_cache_progress_class = reinterpret_cast<jclass>(env->NewGlobalRef(
        env->FindClass("org/yuzu/yuzu_emu/disk_shader_cache/DiskShaderCacheProgress")));
    s_load_callback_stage_class = reinterpret_cast<jclass>(env->NewGlobalRef(env->FindClass(
        "org/yuzu/yuzu_emu/disk_shader_cache/DiskShaderCacheProgress$LoadCallbackStage")));

    const jclass game_dir_class = env->FindClass("org/yuzu/yuzu_emu/model/GameDir");
    s_game_dir_class = reinterpret_cast<jclass>(env->NewGlobalRef(game_dir_class));
    s_game_dir_constructor = env->GetMethodID(game_dir_class, "<init>", "(Ljava/lang/String;Z)V");
    env->DeleteLocalRef(game_dir_class);

    // Initialize methods
    s_exit_emulation_activity =
        env->GetStaticMethodID(s_native_library_class, "exitEmulationActivity", "(I)V");
    s_disk_cache_load_progress =
        env->GetStaticMethodID(s_disk_cache_progress_class, "loadProgress", "(III)V");
    s_on_emulation_started =
        env->GetStaticMethodID(s_native_library_class, "onEmulationStarted", "()V");
    s_on_emulation_stopped =
        env->GetStaticMethodID(s_native_library_class, "onEmulationStopped", "(I)V");

    // Initialize Android Storage
    Common::FS::Android::RegisterCallbacks(env, s_native_library_class);

    // Initialize applets
    SoftwareKeyboard::InitJNI(env);

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
    env->DeleteGlobalRef(s_disk_cache_progress_class);
    env->DeleteGlobalRef(s_load_callback_stage_class);
    env->DeleteGlobalRef(s_game_dir_class);

    // UnInitialize applets
    SoftwareKeyboard::CleanupJNI(env);
}

#ifdef __cplusplus
}
#endif
