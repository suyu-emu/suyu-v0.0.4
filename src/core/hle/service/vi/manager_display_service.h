// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/service.h"

namespace Service::VI {

class IManagerDisplayService final : public ServiceFramework<IManagerDisplayService> {
public:
    explicit IManagerDisplayService(Core::System& system_, Nvnflinger::Nvnflinger& nvnflinger_);
    ~IManagerDisplayService() override;

private:
    void CloseDisplay(HLERequestContext& ctx);
    void CreateManagedLayer(HLERequestContext& ctx);
    void AddToLayerStack(HLERequestContext& ctx);
    void SetLayerVisibility(HLERequestContext& ctx);

private:
    Nvnflinger::Nvnflinger& nvnflinger;
};

} // namespace Service::VI
