// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::HID {
class ResourceManager;
class HidFirmwareSettings;

class IHidDebugServer final : public ServiceFramework<IHidDebugServer> {
public:
    explicit IHidDebugServer(Core::System& system_, std::shared_ptr<ResourceManager> resource,
                             std::shared_ptr<HidFirmwareSettings> settings);
    ~IHidDebugServer() override;

private:
    void DeactivateTouchScreen(HLERequestContext& ctx);
    void SetTouchScreenAutoPilotState(HLERequestContext& ctx);
    void UnsetTouchScreenAutoPilotState(HLERequestContext& ctx);
    void GetTouchScreenConfiguration(HLERequestContext& ctx);
    void ProcessTouchScreenAutoTune(HLERequestContext& ctx);
    void ForceStopTouchScreenManagement(HLERequestContext& ctx);
    void ForceRestartTouchScreenManagement(HLERequestContext& ctx);
    void IsTouchScreenManaged(HLERequestContext& ctx);
    void DeactivateGesture(HLERequestContext& ctx);

    std::shared_ptr<ResourceManager> GetResourceManager();

    std::shared_ptr<ResourceManager> resource_manager;
    std::shared_ptr<HidFirmwareSettings> firmware_settings;
};

} // namespace Service::HID
