// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/frontend/applets/web_browser.h"

namespace Core::Frontend {

WebBrowserApplet::~WebBrowserApplet() = default;

DefaultWebBrowserApplet::~DefaultWebBrowserApplet() = default;

void DefaultWebBrowserApplet::OpenLocalWebPage(
    std::string_view local_url, std::function<void(WebExitReason, std::string)> callback) const {
    LOG_WARNING(Service_AM, "(STUBBED) called, backend requested to open local web page at {}",
                local_url);

    callback(WebExitReason::WindowClosed, "http://localhost/");
}

} // namespace Core::Frontend
