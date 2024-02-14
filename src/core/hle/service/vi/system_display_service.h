// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/service.h"

namespace Service::VI {

class ISystemDisplayService final : public ServiceFramework<ISystemDisplayService> {
public:
    explicit ISystemDisplayService(Core::System& system_, Nvnflinger::Nvnflinger& nvnflinger_);
    ~ISystemDisplayService() override;

private:
    void GetSharedBufferMemoryHandleId(HLERequestContext& ctx);
    void OpenSharedLayer(HLERequestContext& ctx);
    void ConnectSharedLayer(HLERequestContext& ctx);
    void GetSharedFrameBufferAcquirableEvent(HLERequestContext& ctx);
    void AcquireSharedFrameBuffer(HLERequestContext& ctx);
    void PresentSharedFrameBuffer(HLERequestContext& ctx);
    void SetLayerZ(HLERequestContext& ctx);
    void SetLayerVisibility(HLERequestContext& ctx);
    void GetDisplayMode(HLERequestContext& ctx);

private:
    Nvnflinger::Nvnflinger& nvnflinger;
};

} // namespace Service::VI
