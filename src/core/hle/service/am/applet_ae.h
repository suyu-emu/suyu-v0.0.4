// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "core/hle/service/service.h"

namespace Service {
namespace FileSystem {
class FileSystemController;
}

namespace Nvnflinger {
class Nvnflinger;
}

namespace AM {

struct Applet;

class AppletAE final : public ServiceFramework<AppletAE> {
public:
    explicit AppletAE(Nvnflinger::Nvnflinger& nvnflinger_, Core::System& system_);
    ~AppletAE() override;

private:
    void OpenSystemAppletProxy(HLERequestContext& ctx);
    void OpenLibraryAppletProxy(HLERequestContext& ctx);
    void OpenLibraryAppletProxyOld(HLERequestContext& ctx);

    std::shared_ptr<Applet> GetAppletFromContext(HLERequestContext& ctx);

    Nvnflinger::Nvnflinger& nvnflinger;
};

} // namespace AM
} // namespace Service
