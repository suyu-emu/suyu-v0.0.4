// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Capture {
class AlbumManager;

class IScreenShotApplicationService final : public ServiceFramework<IScreenShotApplicationService> {
public:
    explicit IScreenShotApplicationService(Core::System& system_,
                                           std::shared_ptr<AlbumManager> album_manager);
    ~IScreenShotApplicationService() override;

private:
    void SetShimLibraryVersion(HLERequestContext& ctx);
    void SaveScreenShotEx0(HLERequestContext& ctx);
    void SaveScreenShotEx1(HLERequestContext& ctx);

    std::shared_ptr<AlbumManager> manager;
};

} // namespace Service::Capture
