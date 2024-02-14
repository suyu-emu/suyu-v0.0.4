// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/math_util.h"
#include "core/hle/service/cmif_types.h"
#include "core/hle/service/nvnflinger/ui/fence.h"
#include "core/hle/service/service.h"

namespace Service::Nvnflinger {
class Nvnflinger;
struct SharedMemoryPoolLayout;
} // namespace Service::Nvnflinger

namespace Service::VI {

class ISystemDisplayService final : public ServiceFramework<ISystemDisplayService> {
public:
    explicit ISystemDisplayService(Core::System& system_,
                                   std::shared_ptr<Nvnflinger::Nvnflinger> surface_flinger);
    ~ISystemDisplayService() override;

private:
    Result SetLayerZ(u32 z_value, u64 layer_id);
    Result SetLayerVisibility(bool visible, u64 layer_id);
    Result GetDisplayMode(Out<u32> out_width, Out<u32> out_height, Out<f32> out_refresh_rate,
                          Out<u32> out_unknown);

    Result GetSharedBufferMemoryHandleId(
        Out<s32> out_nvmap_handle, Out<u64> out_size,
        OutLargeData<Nvnflinger::SharedMemoryPoolLayout, BufferAttr_HipcMapAlias> out_pool_layout,
        u64 buffer_id, ClientAppletResourceUserId aruid);
    Result OpenSharedLayer(u64 layer_id);
    Result ConnectSharedLayer(u64 layer_id);
    Result GetSharedFrameBufferAcquirableEvent(OutCopyHandle<Kernel::KReadableEvent> out_event,
                                               u64 layer_id);
    Result AcquireSharedFrameBuffer(Out<android::Fence> out_fence,
                                    Out<std::array<s32, 4>> out_slots, Out<s64> out_target_slot,
                                    u64 layer_id);
    Result PresentSharedFrameBuffer(android::Fence fence, Common::Rectangle<s32> crop_region,
                                    u32 window_transform, s32 swap_interval, u64 layer_id,
                                    s64 surface_id);

private:
    const std::shared_ptr<Nvnflinger::Nvnflinger> m_surface_flinger;
};

} // namespace Service::VI
