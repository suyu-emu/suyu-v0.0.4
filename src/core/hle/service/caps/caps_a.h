// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Capture {
class AlbumManager;

class IAlbumAccessorService final : public ServiceFramework<IAlbumAccessorService> {
public:
    explicit IAlbumAccessorService(Core::System& system_,
                                   std::shared_ptr<AlbumManager> album_manager);
    ~IAlbumAccessorService() override;

private:
    void DeleteAlbumFile(HLERequestContext& ctx);
    void IsAlbumMounted(HLERequestContext& ctx);
    void Unknown18(HLERequestContext& ctx);
    void GetAlbumFileListEx0(HLERequestContext& ctx);
    void GetAutoSavingStorage(HLERequestContext& ctx);
    void LoadAlbumScreenShotImageEx1(HLERequestContext& ctx);
    void LoadAlbumScreenShotThumbnailImageEx1(HLERequestContext& ctx);

    Result TranslateResult(Result in_result);

    std::shared_ptr<AlbumManager> manager = nullptr;
};

} // namespace Service::Capture
