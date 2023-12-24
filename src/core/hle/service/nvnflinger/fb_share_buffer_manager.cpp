// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <random>

#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_system_resource.h"
#include "core/hle/service/nvdrv/devices/nvmap.h"
#include "core/hle/service/nvdrv/nvdrv.h"
#include "core/hle/service/nvnflinger/buffer_queue_producer.h"
#include "core/hle/service/nvnflinger/fb_share_buffer_manager.h"
#include "core/hle/service/nvnflinger/pixel_format.h"
#include "core/hle/service/nvnflinger/ui/graphic_buffer.h"
#include "core/hle/service/vi/layer/vi_layer.h"
#include "core/hle/service/vi/vi_results.h"

namespace Service::Nvnflinger {

namespace {

Result AllocateIoForProcessAddressSpace(Common::ProcessAddress* out_map_address,
                                        std::unique_ptr<Kernel::KPageGroup>* out_page_group,
                                        Core::System& system, u32 size) {
    using Core::Memory::YUZU_PAGESIZE;

    // Allocate memory for the system shared buffer.
    // FIXME: Because the gmmu can only point to cpu addresses, we need
    //        to map this in the application space to allow it to be used.
    // FIXME: Add proper smmu emulation.
    // FIXME: This memory belongs to vi's .data section.
    auto& kernel = system.Kernel();
    auto* process = system.ApplicationProcess();
    auto& page_table = process->GetPageTable();

    // Hold a temporary page group reference while we try to map it.
    auto pg = std::make_unique<Kernel::KPageGroup>(
        kernel, std::addressof(kernel.GetSystemSystemResource().GetBlockInfoManager()));

    // Allocate memory from secure pool.
    R_TRY(kernel.MemoryManager().AllocateAndOpen(
        pg.get(), size / YUZU_PAGESIZE,
        Kernel::KMemoryManager::EncodeOption(Kernel::KMemoryManager::Pool::Secure,
                                             Kernel::KMemoryManager::Direction::FromBack)));

    // Get bounds of where mapping is possible.
    const VAddr alias_code_begin = GetInteger(page_table.GetAliasCodeRegionStart());
    const VAddr alias_code_size = page_table.GetAliasCodeRegionSize() / YUZU_PAGESIZE;
    const auto state = Kernel::KMemoryState::IoMemory;
    const auto perm = Kernel::KMemoryPermission::UserReadWrite;
    std::mt19937_64 rng{process->GetRandomEntropy(0)};

    // Retry up to 64 times to map into alias code range.
    Result res = ResultSuccess;
    int i;
    for (i = 0; i < 64; i++) {
        *out_map_address = alias_code_begin + ((rng() % alias_code_size) * YUZU_PAGESIZE);
        res = page_table.MapPageGroup(*out_map_address, *pg, state, perm);
        if (R_SUCCEEDED(res)) {
            break;
        }
    }

    // Return failure, if necessary
    R_UNLESS(i < 64, res);

    // Return the mapped page group.
    *out_page_group = std::move(pg);

    // We succeeded.
    R_SUCCEED();
}

Result CreateNvMapHandle(u32* out_nv_map_handle, Nvidia::Devices::nvmap& nvmap, u32 size) {
    // Create a handle.
    Nvidia::Devices::nvmap::IocCreateParams create_params{
        .size = size,
        .handle = 0,
    };
    R_UNLESS(nvmap.IocCreate(create_params) == Nvidia::NvResult::Success,
             VI::ResultOperationFailed);

    // Assign the output handle.
    *out_nv_map_handle = create_params.handle;

    // We succeeded.
    R_SUCCEED();
}

Result FreeNvMapHandle(Nvidia::Devices::nvmap& nvmap, u32 handle, Nvidia::DeviceFD nvmap_fd) {
    // Free the handle.
    Nvidia::Devices::nvmap::IocFreeParams free_params{
        .handle = handle,
    };
    R_UNLESS(nvmap.IocFree(free_params, nvmap_fd) == Nvidia::NvResult::Success, VI::ResultOperationFailed);

    // We succeeded.
    R_SUCCEED();
}

Result AllocNvMapHandle(Nvidia::Devices::nvmap& nvmap, u32 handle, Common::ProcessAddress buffer,
                        u32 size, Nvidia::DeviceFD nvmap_fd) {
    // Assign the allocated memory to the handle.
    Nvidia::Devices::nvmap::IocAllocParams alloc_params{
        .handle = handle,
        .heap_mask = 0,
        .flags = {},
        .align = 0,
        .kind = 0,
        .address = GetInteger(buffer),
    };
    R_UNLESS(nvmap.IocAlloc(alloc_params, nvmap_fd) == Nvidia::NvResult::Success, VI::ResultOperationFailed);

    // We succeeded.
    R_SUCCEED();
}

Result AllocateHandleForBuffer(u32* out_handle, Nvidia::Module& nvdrv, Nvidia::DeviceFD nvmap_fd,
                               Common::ProcessAddress buffer, u32 size) {
    // Get the nvmap device.
    auto nvmap = nvdrv.GetDevice<Nvidia::Devices::nvmap>(nvmap_fd);
    ASSERT(nvmap != nullptr);

    // Create a handle.
    R_TRY(CreateNvMapHandle(out_handle, *nvmap, size));

    // Ensure we maintain a clean state on failure.
    ON_RESULT_FAILURE {
        ASSERT(R_SUCCEEDED(FreeNvMapHandle(*nvmap, *out_handle, nvmap_fd)));
    };

    // Assign the allocated memory to the handle.
    R_RETURN(AllocNvMapHandle(*nvmap, *out_handle, buffer, size, nvmap_fd));
}

constexpr auto SharedBufferBlockLinearFormat = android::PixelFormat::Rgba8888;
constexpr u32 SharedBufferBlockLinearBpp = 4;

constexpr u32 SharedBufferBlockLinearWidth = 1280;
constexpr u32 SharedBufferBlockLinearHeight = 768;
constexpr u32 SharedBufferBlockLinearStride =
    SharedBufferBlockLinearWidth * SharedBufferBlockLinearBpp;
constexpr u32 SharedBufferNumSlots = 7;

constexpr u32 SharedBufferWidth = 1280;
constexpr u32 SharedBufferHeight = 720;
constexpr u32 SharedBufferAsync = false;

constexpr u32 SharedBufferSlotSize =
    SharedBufferBlockLinearWidth * SharedBufferBlockLinearHeight * SharedBufferBlockLinearBpp;
constexpr u32 SharedBufferSize = SharedBufferSlotSize * SharedBufferNumSlots;

constexpr SharedMemoryPoolLayout SharedBufferPoolLayout = [] {
    SharedMemoryPoolLayout layout{};
    layout.num_slots = SharedBufferNumSlots;

    for (u32 i = 0; i < SharedBufferNumSlots; i++) {
        layout.slots[i].buffer_offset = i * SharedBufferSlotSize;
        layout.slots[i].size = SharedBufferSlotSize;
        layout.slots[i].width = SharedBufferWidth;
        layout.slots[i].height = SharedBufferHeight;
    }

    return layout;
}();

void MakeGraphicBuffer(android::BufferQueueProducer& producer, u32 slot, u32 handle) {
    auto buffer = std::make_shared<android::NvGraphicBuffer>();
    buffer->width = SharedBufferWidth;
    buffer->height = SharedBufferHeight;
    buffer->stride = SharedBufferBlockLinearStride;
    buffer->format = SharedBufferBlockLinearFormat;
    buffer->external_format = SharedBufferBlockLinearFormat;
    buffer->buffer_id = handle;
    buffer->offset = slot * SharedBufferSlotSize;
    ASSERT(producer.SetPreallocatedBuffer(slot, buffer) == android::Status::NoError);
}

} // namespace

FbShareBufferManager::FbShareBufferManager(Core::System& system, Nvnflinger& flinger,
                                           std::shared_ptr<Nvidia::Module> nvdrv)
    : m_system(system), m_flinger(flinger), m_nvdrv(std::move(nvdrv)) {}

FbShareBufferManager::~FbShareBufferManager() = default;

Result FbShareBufferManager::Initialize(u64* out_buffer_id, u64* out_layer_id, u64 display_id) {
    std::scoped_lock lk{m_guard};

    // Ensure we have not already created a buffer.
    R_UNLESS(m_buffer_id == 0, VI::ResultOperationFailed);

    // Allocate memory and space for the shared buffer.
    Common::ProcessAddress map_address;
    R_TRY(AllocateIoForProcessAddressSpace(std::addressof(map_address),
                                           std::addressof(m_buffer_page_group), m_system,
                                           SharedBufferSize));

    auto& container = m_nvdrv->GetContainer();
    m_session_id = container.OpenSession(m_system.ApplicationProcess());
    m_nvmap_fd = m_nvdrv->Open("/dev/nvmap", m_session_id);

    // Create an nvmap handle for the buffer and assign the memory to it.
    R_TRY(AllocateHandleForBuffer(std::addressof(m_buffer_nvmap_handle), *m_nvdrv, m_nvmap_fd, map_address,
                                  SharedBufferSize));

    // Record the display id.
    m_display_id = display_id;

    // Create and open a layer for the display.
    m_layer_id = m_flinger.CreateLayer(m_display_id).value();
    m_flinger.OpenLayer(m_layer_id);

    // Set up the buffer.
    m_buffer_id = m_next_buffer_id++;

    // Get the layer.
    VI::Layer* layer = m_flinger.FindLayer(m_display_id, m_layer_id);
    ASSERT(layer != nullptr);

    // Get the producer and set preallocated buffers.
    auto& producer = layer->GetBufferQueue();
    MakeGraphicBuffer(producer, 0, m_buffer_nvmap_handle);
    MakeGraphicBuffer(producer, 1, m_buffer_nvmap_handle);

    // Assign outputs.
    *out_buffer_id = m_buffer_id;
    *out_layer_id = m_layer_id;

    // We succeeded.
    R_SUCCEED();
}

Result FbShareBufferManager::GetSharedBufferMemoryHandleId(u64* out_buffer_size,
                                                           s32* out_nvmap_handle,
                                                           SharedMemoryPoolLayout* out_pool_layout,
                                                           u64 buffer_id,
                                                           u64 applet_resource_user_id) {
    std::scoped_lock lk{m_guard};

    R_UNLESS(m_buffer_id > 0, VI::ResultNotFound);
    R_UNLESS(buffer_id == m_buffer_id, VI::ResultNotFound);

    *out_pool_layout = SharedBufferPoolLayout;
    *out_buffer_size = SharedBufferSize;
    *out_nvmap_handle = m_buffer_nvmap_handle;

    R_SUCCEED();
}

Result FbShareBufferManager::GetLayerFromId(VI::Layer** out_layer, u64 layer_id) {
    // Ensure the layer id is valid.
    R_UNLESS(m_layer_id > 0 && layer_id == m_layer_id, VI::ResultNotFound);

    // Get the layer.
    VI::Layer* layer = m_flinger.FindLayer(m_display_id, layer_id);
    R_UNLESS(layer != nullptr, VI::ResultNotFound);

    // We succeeded.
    *out_layer = layer;
    R_SUCCEED();
}

Result FbShareBufferManager::AcquireSharedFrameBuffer(android::Fence* out_fence,
                                                      std::array<s32, 4>& out_slot_indexes,
                                                      s64* out_target_slot, u64 layer_id) {
    std::scoped_lock lk{m_guard};

    // Get the layer.
    VI::Layer* layer;
    R_TRY(this->GetLayerFromId(std::addressof(layer), layer_id));

    // Get the producer.
    auto& producer = layer->GetBufferQueue();

    // Get the next buffer from the producer.
    s32 slot;
    R_UNLESS(producer.DequeueBuffer(std::addressof(slot), out_fence, SharedBufferAsync != 0,
                                    SharedBufferWidth, SharedBufferHeight,
                                    SharedBufferBlockLinearFormat, 0) == android::Status::NoError,
             VI::ResultOperationFailed);

    // Assign remaining outputs.
    *out_target_slot = slot;
    out_slot_indexes = {0, 1, -1, -1};

    // We succeeded.
    R_SUCCEED();
}

Result FbShareBufferManager::PresentSharedFrameBuffer(android::Fence fence,
                                                      Common::Rectangle<s32> crop_region,
                                                      u32 transform, s32 swap_interval,
                                                      u64 layer_id, s64 slot) {
    std::scoped_lock lk{m_guard};

    // Get the layer.
    VI::Layer* layer;
    R_TRY(this->GetLayerFromId(std::addressof(layer), layer_id));

    // Get the producer.
    auto& producer = layer->GetBufferQueue();

    // Request to queue the buffer.
    std::shared_ptr<android::GraphicBuffer> buffer;
    R_UNLESS(producer.RequestBuffer(static_cast<s32>(slot), std::addressof(buffer)) ==
                 android::Status::NoError,
             VI::ResultOperationFailed);

    // Queue the buffer to the producer.
    android::QueueBufferInput input{};
    android::QueueBufferOutput output{};
    input.crop = crop_region;
    input.fence = fence;
    input.transform = static_cast<android::NativeWindowTransform>(transform);
    input.swap_interval = swap_interval;
    R_UNLESS(producer.QueueBuffer(static_cast<s32>(slot), input, std::addressof(output)) ==
                 android::Status::NoError,
             VI::ResultOperationFailed);

    // We succeeded.
    R_SUCCEED();
}

Result FbShareBufferManager::GetSharedFrameBufferAcquirableEvent(Kernel::KReadableEvent** out_event,
                                                                 u64 layer_id) {
    std::scoped_lock lk{m_guard};

    // Get the layer.
    VI::Layer* layer;
    R_TRY(this->GetLayerFromId(std::addressof(layer), layer_id));

    // Get the producer.
    auto& producer = layer->GetBufferQueue();

    // Set the event.
    *out_event = std::addressof(producer.GetNativeHandle());

    // We succeeded.
    R_SUCCEED();
}

} // namespace Service::Nvnflinger
