// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/service.h"
#include "core/hle/service/vi/vi_types.h"

namespace Service::VI {

class IApplicationDisplayService final : public ServiceFramework<IApplicationDisplayService> {
public:
    IApplicationDisplayService(Core::System& system_, Nvnflinger::Nvnflinger& nvnflinger_,
                               Nvnflinger::HosBinderDriverServer& hos_binder_driver_server_);
    ~IApplicationDisplayService() override;

private:
    void GetRelayService(HLERequestContext& ctx);
    void GetSystemDisplayService(HLERequestContext& ctx);
    void GetManagerDisplayService(HLERequestContext& ctx);
    void GetIndirectDisplayTransactionService(HLERequestContext& ctx);
    void OpenDisplay(HLERequestContext& ctx);
    void OpenDefaultDisplay(HLERequestContext& ctx);
    void OpenDisplayImpl(HLERequestContext& ctx, std::string_view name);
    void CloseDisplay(HLERequestContext& ctx);
    void SetDisplayEnabled(HLERequestContext& ctx);
    void GetDisplayResolution(HLERequestContext& ctx);
    void SetLayerScalingMode(HLERequestContext& ctx);
    void ListDisplays(HLERequestContext& ctx);
    void OpenLayer(HLERequestContext& ctx);
    void CloseLayer(HLERequestContext& ctx);
    void CreateStrayLayer(HLERequestContext& ctx);
    void DestroyStrayLayer(HLERequestContext& ctx);
    void GetDisplayVsyncEvent(HLERequestContext& ctx);
    void ConvertScalingMode(HLERequestContext& ctx);
    void GetIndirectLayerImageMap(HLERequestContext& ctx);
    void GetIndirectLayerImageRequiredMemoryInfo(HLERequestContext& ctx);

private:
    static Result ConvertScalingModeImpl(ConvertedScaleMode* out_scaling_mode,
                                         NintendoScaleMode mode);

private:
    Nvnflinger::Nvnflinger& nvnflinger;
    Nvnflinger::HosBinderDriverServer& hos_binder_driver_server;
    std::vector<u64> stray_layer_ids;
    bool vsync_event_fetched{false};
};

} // namespace Service::VI
