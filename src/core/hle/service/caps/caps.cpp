// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/caps/caps.h"
#include "core/hle/service/caps/caps_a.h"
#include "core/hle/service/caps/caps_c.h"
#include "core/hle/service/caps/caps_manager.h"
#include "core/hle/service/caps/caps_sc.h"
#include "core/hle/service/caps/caps_ss.h"
#include "core/hle/service/caps/caps_su.h"
#include "core/hle/service/caps/caps_u.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"
#include "frontend_common/firmware_manager.h"

namespace Service::Capture {

class IDecoderControlService final : public ServiceFramework<IDecoderControlService> {
public:
    explicit IDecoderControlService(Core::System& system_) : ServiceFramework{system_, "grc:d"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {3001, nullptr, "DecodeJpeg"},
            {4001, nullptr, "ShrinkJpeg"},
            {4002, nullptr, "ShrinkJpegEx"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);
    auto album_manager = std::make_shared<AlbumManager>(system);

    server_manager->RegisterNamedService("caps:a", std::make_shared<IAlbumAccessorService>(system, album_manager));
    server_manager->RegisterNamedService("caps:c", std::make_shared<IAlbumControlService>(system, album_manager));
    server_manager->RegisterNamedService("caps:u", std::make_shared<IAlbumApplicationService>(system, album_manager));
    server_manager->RegisterNamedService("caps:ss", std::make_shared<IScreenShotService>(system, album_manager));
    server_manager->RegisterNamedService("caps:sc", std::make_shared<IScreenShotControlService>(system));
    server_manager->RegisterNamedService("caps:su", std::make_shared<IScreenShotApplicationService>(system, album_manager));
    // +4.0.0
    if (FirmwareManager::GetFirmwareVersion(system).first.major >= 4) {
        server_manager->RegisterNamedService("caps:dc", std::make_shared<IDecoderControlService>(system));
    }
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::Capture
