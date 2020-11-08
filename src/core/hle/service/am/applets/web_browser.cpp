// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/frontend/applets/web_browser.h"
#include "core/hle/result.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/applets/web_browser.h"

namespace Service::AM::Applets {

WebBrowser::WebBrowser(Core::System& system_, const Core::Frontend::WebBrowserApplet& frontend_)
    : Applet{system_.Kernel()}, frontend(frontend_), system{system_} {}

WebBrowser::~WebBrowser() = default;

void WebBrowser::Initialize() {
    Applet::Initialize();
}

bool WebBrowser::TransactionComplete() const {
    return complete;
}

ResultCode WebBrowser::GetStatus() const {
    return status;
}

void WebBrowser::ExecuteInteractive() {
    UNIMPLEMENTED_MSG("Unexpected interactive data recieved!");
}

void WebBrowser::Execute() {}

} // namespace Service::AM::Applets
