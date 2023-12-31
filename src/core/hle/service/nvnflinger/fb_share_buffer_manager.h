// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/math_util.h"
#include "core/hle/service/nvdrv/nvdata.h"
#include "core/hle/service/nvnflinger/nvnflinger.h"
#include "core/hle/service/nvnflinger/ui/fence.h"

namespace Kernel {
class KPageGroup;
}

namespace Service::Nvnflinger {

struct SharedMemorySlot {
    u64 buffer_offset;
    u64 size;
    s32 width;
    s32 height;
};
static_assert(sizeof(SharedMemorySlot) == 0x18, "SharedMemorySlot has wrong size");

struct SharedMemoryPoolLayout {
    s32 num_slots;
    std::array<SharedMemorySlot, 0x10> slots;
};
static_assert(sizeof(SharedMemoryPoolLayout) == 0x188, "SharedMemoryPoolLayout has wrong size");

class FbShareBufferManager final {
public:
    explicit FbShareBufferManager(Core::System& system, Nvnflinger& flinger,
                                  std::shared_ptr<Nvidia::Module> nvdrv);
    ~FbShareBufferManager();

    Result Initialize(u64* out_buffer_id, u64* out_layer_handle, u64 display_id);
    Result GetSharedBufferMemoryHandleId(u64* out_buffer_size, s32* out_nvmap_handle,
                                         SharedMemoryPoolLayout* out_pool_layout, u64 buffer_id,
                                         u64 applet_resource_user_id);
    Result AcquireSharedFrameBuffer(android::Fence* out_fence, std::array<s32, 4>& out_slots,
                                    s64* out_target_slot, u64 layer_id);
    Result PresentSharedFrameBuffer(android::Fence fence, Common::Rectangle<s32> crop_region,
                                    u32 transform, s32 swap_interval, u64 layer_id, s64 slot);
    Result GetSharedFrameBufferAcquirableEvent(Kernel::KReadableEvent** out_event, u64 layer_id);

private:
    Result GetLayerFromId(VI::Layer** out_layer, u64 layer_id);

private:
    u64 m_next_buffer_id = 1;
    u64 m_display_id = 0;
    u64 m_buffer_id = 0;
    u64 m_layer_id = 0;
    u32 m_buffer_nvmap_handle = 0;
    SharedMemoryPoolLayout m_pool_layout = {};
    Nvidia::DeviceFD m_nvmap_fd = {};
    size_t m_session_id = {};
    std::unique_ptr<Kernel::KPageGroup> m_buffer_page_group;

    std::mutex m_guard;
    Core::System& m_system;
    Nvnflinger& m_flinger;
    std::shared_ptr<Nvidia::Module> m_nvdrv;
};

} // namespace Service::Nvnflinger
