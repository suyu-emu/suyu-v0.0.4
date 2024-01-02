// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

struct Applet;

class IApplicationProxy final : public ServiceFramework<IApplicationProxy> {
public:
    explicit IApplicationProxy(Nvnflinger::Nvnflinger& nvnflinger_,
                               std::shared_ptr<Applet> msg_queue_, Core::System& system_);
    ~IApplicationProxy();

private:
    void GetAudioController(HLERequestContext& ctx);
    void GetDisplayController(HLERequestContext& ctx);
    void GetProcessWindingController(HLERequestContext& ctx);
    void GetDebugFunctions(HLERequestContext& ctx);
    void GetWindowController(HLERequestContext& ctx);
    void GetSelfController(HLERequestContext& ctx);
    void GetCommonStateGetter(HLERequestContext& ctx);
    void GetLibraryAppletCreator(HLERequestContext& ctx);
    void GetApplicationFunctions(HLERequestContext& ctx);

    Nvnflinger::Nvnflinger& nvnflinger;
    std::shared_ptr<Applet> applet;
};

} // namespace Service::AM
