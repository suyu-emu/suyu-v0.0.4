// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

struct Applet;

class ILibraryAppletProxy final : public ServiceFramework<ILibraryAppletProxy> {
public:
    explicit ILibraryAppletProxy(Nvnflinger::Nvnflinger& nvnflinger_,
                                 std::shared_ptr<Applet> applet_, Core::System& system_);
    ~ILibraryAppletProxy();

private:
    void GetCommonStateGetter(HLERequestContext& ctx);
    void GetSelfController(HLERequestContext& ctx);
    void GetWindowController(HLERequestContext& ctx);
    void GetAudioController(HLERequestContext& ctx);
    void GetDisplayController(HLERequestContext& ctx);
    void GetProcessWindingController(HLERequestContext& ctx);
    void GetLibraryAppletCreator(HLERequestContext& ctx);
    void OpenLibraryAppletSelfAccessor(HLERequestContext& ctx);
    void GetAppletCommonFunctions(HLERequestContext& ctx);
    void GetHomeMenuFunctions(HLERequestContext& ctx);
    void GetGlobalStateController(HLERequestContext& ctx);
    void GetDebugFunctions(HLERequestContext& ctx);

    Nvnflinger::Nvnflinger& nvnflinger;
    std::shared_ptr<Applet> applet;
};

} // namespace Service::AM
