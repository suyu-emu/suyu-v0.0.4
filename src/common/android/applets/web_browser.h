// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <jni.h>
#include <string>

#include "core/frontend/applets/web_browser.h"

namespace Common::Android::WebBrowser {

class AndroidWebBrowser final : public Core::Frontend::WebBrowserApplet {
public:
    ~AndroidWebBrowser() override = default;

    void Close() const override {}

    void OpenLocalWebPage(const std::string& local_url,
                          ExtractROMFSCallback extract_romfs_callback,
                          OpenWebPageCallback callback) const override;

    void OpenExternalWebPage(const std::string& external_url,
                             OpenWebPageCallback callback) const override;
};

void InitJNI(JNIEnv* env);
void CleanupJNI(JNIEnv* env);

} // namespace Common::Android::WebBrowser
