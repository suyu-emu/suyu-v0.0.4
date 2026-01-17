// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/android/android_common.h"
#include "common/android/id_cache.h"
#include "common/android/applets/web_browser.h"
#include "common/logging/log.h"

static jclass s_native_library_class = nullptr;
static jmethodID s_open_external_url = nullptr;

namespace Common::Android::WebBrowser {

void InitJNI(JNIEnv* env) {
    const jclass local = env->FindClass("org/yuzu/yuzu_emu/NativeLibrary");
    s_native_library_class = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    s_open_external_url = env->GetStaticMethodID(s_native_library_class, "openExternalUrl", "(Ljava/lang/String;)V");
}

void CleanupJNI(JNIEnv* env) {
    if (s_native_library_class != nullptr) {
        env->DeleteGlobalRef(s_native_library_class);
        s_native_library_class = nullptr;
    }
    s_open_external_url = nullptr;
}

void AndroidWebBrowser::OpenLocalWebPage(const std::string& local_url, ExtractROMFSCallback extract_romfs_callback, OpenWebPageCallback callback) const {
    LOG_WARNING(Frontend, "(STUBBED)");
    callback(Service::AM::Frontend::WebExitReason::WindowClosed, "");
}

void AndroidWebBrowser::OpenExternalWebPage(const std::string& external_url, OpenWebPageCallback callback) const {
    // do a dedicated thread, calling from the this thread crashed CPU fiber.
    Common::Android::RunJNIOnFiber<void>([&](JNIEnv* env) {
        if (env != nullptr && s_native_library_class != nullptr && s_open_external_url != nullptr) {
            const jstring j_url = Common::Android::ToJString(env, external_url);
            env->CallStaticVoidMethod(s_native_library_class, s_open_external_url, j_url);
            env->DeleteLocalRef(j_url);
        } else {
            LOG_ERROR(Frontend, "JNI not initialized, cannot open {}", external_url);
        }
        return;
    });

    callback(Service::AM::Frontend::WebExitReason::WindowClosed, external_url);
}

} // namespace Common::Android::WebBrowser
