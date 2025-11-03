// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/assert.h"
#include "common/hex_util.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/frontend/applets/net_connect.h"
#include "core/hle/result.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/applet_data_broker.h"
#include "core/hle/service/am/frontend/applet_net_connect.h"
#include "core/hle/service/am/service/storage.h"
#include "core/reporter.h"

namespace Service::AM::Frontend {

NetConnect::NetConnect(Core::System& system_, std::shared_ptr<Applet> applet_, LibraryAppletMode applet_mode_, const Core::Frontend::NetConnectApplet& frontend_)
    : FrontendApplet{system_, applet_, applet_mode_}, frontend{frontend_} {}

NetConnect::~NetConnect() = default;

void NetConnect::Initialize() {
    FrontendApplet::Initialize();
    complete = false;
}

Result NetConnect::GetStatus() const {
    return ResultSuccess;
}

void NetConnect::ExecuteInteractive() {
    ASSERT_MSG(false, "Unexpected interactive applet data.");
}

void NetConnect::Execute() {
    if (complete)
        return;
}

Result NetConnect::RequestExit() {
    frontend.Close();
    R_SUCCEED();
}

} // namespace Service::AM::Frontend
