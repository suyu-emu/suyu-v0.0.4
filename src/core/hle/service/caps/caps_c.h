// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Capture {
class AlbumManager;

class IAlbumControlService final : public ServiceFramework<IAlbumControlService> {
public:
    explicit IAlbumControlService(Core::System& system_,
                                  std::shared_ptr<AlbumManager> album_manager);
    ~IAlbumControlService() override;

private:
    void SetShimLibraryVersion(HLERequestContext& ctx);

    std::shared_ptr<AlbumManager> manager = nullptr;
};

} // namespace Service::Capture
