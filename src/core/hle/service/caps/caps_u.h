// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Capture {
class AlbumManager;

class IAlbumApplicationService final : public ServiceFramework<IAlbumApplicationService> {
public:
    explicit IAlbumApplicationService(Core::System& system_,
                                      std::shared_ptr<AlbumManager> album_manager);
    ~IAlbumApplicationService() override;

private:
    void SetShimLibraryVersion(HLERequestContext& ctx);
    void GetAlbumFileList0AafeAruidDeprecated(HLERequestContext& ctx);
    void GetAlbumFileList3AaeAruid(HLERequestContext& ctx);

    std::shared_ptr<AlbumManager> manager = nullptr;
};

} // namespace Service::Capture
