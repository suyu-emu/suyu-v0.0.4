// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/am/applet_message_queue.h"
#include "core/hle/service/service.h"

namespace Service::AM {

class ISystemAppletProxy final : public ServiceFramework<ISystemAppletProxy> {
public:
    explicit ISystemAppletProxy(Nvnflinger::Nvnflinger& nvnflinger_,
                                std::shared_ptr<AppletMessageQueue> msg_queue_,
                                Core::System& system_);

private:
    void GetCommonStateGetter(HLERequestContext& ctx);
    void GetSelfController(HLERequestContext& ctx);
    void GetWindowController(HLERequestContext& ctx);
    void GetAudioController(HLERequestContext& ctx);
    void GetDisplayController(HLERequestContext& ctx);
    void GetLibraryAppletCreator(HLERequestContext& ctx);
    void GetHomeMenuFunctions(HLERequestContext& ctx);
    void GetGlobalStateController(HLERequestContext& ctx);
    void GetApplicationCreator(HLERequestContext& ctx);
    void GetAppletCommonFunctions(HLERequestContext& ctx);
    void GetDebugFunctions(HLERequestContext& ctx);

    Nvnflinger::Nvnflinger& nvnflinger;
    std::shared_ptr<AppletMessageQueue> msg_queue;
};

} // namespace Service::AM
