// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Capture {

class IScreenShotService final : public ServiceFramework<IScreenShotService> {
public:
    explicit IScreenShotService(Core::System& system_, std::shared_ptr<AlbumManager> album_manager);
    ~IScreenShotService() override;

private:
    void SaveScreenShotEx0(HLERequestContext& ctx);
    void SaveEditedScreenShotEx1(HLERequestContext& ctx);

    std::shared_ptr<AlbumManager> manager;
};

} // namespace Service::Capture
