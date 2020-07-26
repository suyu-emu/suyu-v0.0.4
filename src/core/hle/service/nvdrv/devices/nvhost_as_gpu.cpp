// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <utility>

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/service/nvdrv/devices/nvhost_as_gpu.h"
#include "core/hle/service/nvdrv/devices/nvmap.h"
#include "core/memory.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_base.h"

namespace Service::Nvidia::Devices {

namespace NvErrCodes {
constexpr u32 Success{};
constexpr u32 OutOfMemory{static_cast<u32>(-12)};
constexpr u32 InvalidInput{static_cast<u32>(-22)};
} // namespace NvErrCodes

nvhost_as_gpu::nvhost_as_gpu(Core::System& system, std::shared_ptr<nvmap> nvmap_dev)
    : nvdevice(system), nvmap_dev(std::move(nvmap_dev)) {}
nvhost_as_gpu::~nvhost_as_gpu() = default;

u32 nvhost_as_gpu::ioctl(Ioctl command, const std::vector<u8>& input, const std::vector<u8>& input2,
                         std::vector<u8>& output, std::vector<u8>& output2, IoctlCtrl& ctrl,
                         IoctlVersion version) {
    LOG_DEBUG(Service_NVDRV, "called, command=0x{:08X}, input_size=0x{:X}, output_size=0x{:X}",
              command.raw, input.size(), output.size());

    switch (static_cast<IoctlCommand>(command.raw)) {
    case IoctlCommand::IocInitalizeExCommand:
        return InitalizeEx(input, output);
    case IoctlCommand::IocAllocateSpaceCommand:
        return AllocateSpace(input, output);
    case IoctlCommand::IocMapBufferExCommand:
        return MapBufferEx(input, output);
    case IoctlCommand::IocBindChannelCommand:
        return BindChannel(input, output);
    case IoctlCommand::IocGetVaRegionsCommand:
        return GetVARegions(input, output);
    case IoctlCommand::IocUnmapBufferCommand:
        return UnmapBuffer(input, output);
    default:
        break;
    }

    if (static_cast<IoctlCommand>(command.cmd.Value()) == IoctlCommand::IocRemapCommand) {
        return Remap(input, output);
    }

    UNIMPLEMENTED_MSG("Unimplemented ioctl command");
    return 0;
}

u32 nvhost_as_gpu::InitalizeEx(const std::vector<u8>& input, std::vector<u8>& output) {
    IoctlInitalizeEx params{};
    std::memcpy(&params, input.data(), input.size());

    LOG_WARNING(Service_NVDRV, "(STUBBED) called, big_page_size=0x{:X}", params.big_page_size);

    return 0;
}

u32 nvhost_as_gpu::AllocateSpace(const std::vector<u8>& input, std::vector<u8>& output) {
    IoctlAllocSpace params{};
    std::memcpy(&params, input.data(), input.size());

    LOG_DEBUG(Service_NVDRV, "called, pages={:X}, page_size={:X}, flags={:X}", params.pages,
              params.page_size, params.flags);

    const auto size{static_cast<u64>(params.pages) * static_cast<u64>(params.page_size)};
    if ((params.flags & AddressSpaceFlags::FixedOffset) != AddressSpaceFlags::None) {
        params.offset = *system.GPU().MemoryManager().AllocateFixed(params.offset, size);
    } else {
        params.offset = system.GPU().MemoryManager().Allocate(size, params.align);
    }

    auto result{NvErrCodes::Success};
    if (!params.offset) {
        LOG_CRITICAL(Service_NVDRV, "allocation failed for size {}", size);
        result = NvErrCodes::OutOfMemory;
    }

    std::memcpy(output.data(), &params, output.size());
    return result;
}

u32 nvhost_as_gpu::Remap(const std::vector<u8>& input, std::vector<u8>& output) {
    const auto num_entries = input.size() / sizeof(IoctlRemapEntry);

    LOG_DEBUG(Service_NVDRV, "called, num_entries=0x{:X}", num_entries);

    auto result{NvErrCodes::Success};
    std::vector<IoctlRemapEntry> entries(num_entries);
    std::memcpy(entries.data(), input.data(), input.size());

    for (const auto& entry : entries) {
        LOG_DEBUG(Service_NVDRV, "remap entry, offset=0x{:X} handle=0x{:X} pages=0x{:X}",
                  entry.offset, entry.nvmap_handle, entry.pages);

        const auto object{nvmap_dev->GetObject(entry.nvmap_handle)};
        if (!object) {
            LOG_CRITICAL(Service_NVDRV, "invalid nvmap_handle={:X}", entry.nvmap_handle);
            result = NvErrCodes::InvalidInput;
            break;
        }

        const auto offset{static_cast<GPUVAddr>(entry.offset) << 0x10};
        const auto size{static_cast<u64>(entry.pages) << 0x10};
        const auto map_offset{static_cast<u64>(entry.map_offset) << 0x10};
        const auto addr{system.GPU().MemoryManager().Map(object->addr + map_offset, offset, size)};

        if (!addr) {
            LOG_CRITICAL(Service_NVDRV, "map returned an invalid address!");
            result = NvErrCodes::InvalidInput;
            break;
        }
    }

    std::memcpy(output.data(), entries.data(), output.size());
    return result;
}

u32 nvhost_as_gpu::MapBufferEx(const std::vector<u8>& input, std::vector<u8>& output) {
    IoctlMapBufferEx params{};
    std::memcpy(&params, input.data(), input.size());

    LOG_DEBUG(Service_NVDRV,
              "called, flags={:X}, nvmap_handle={:X}, buffer_offset={}, mapping_size={}"
              ", offset={}",
              params.flags, params.nvmap_handle, params.buffer_offset, params.mapping_size,
              params.offset);

    const auto object{nvmap_dev->GetObject(params.nvmap_handle)};
    if (!object) {
        LOG_CRITICAL(Service_NVDRV, "invalid nvmap_handle={:X}", params.nvmap_handle);
        std::memcpy(output.data(), &params, output.size());
        return NvErrCodes::InvalidInput;
    }

    // The real nvservices doesn't make a distinction between handles and ids, and
    // object can only have one handle and it will be the same as its id. Assert that this is the
    // case to prevent unexpected behavior.
    ASSERT(object->id == params.nvmap_handle);
    auto& gpu = system.GPU();

    u64 page_size{params.page_size};
    if (!page_size) {
        page_size = object->align;
    }

    if ((params.flags & AddressSpaceFlags::Remap) != AddressSpaceFlags::None) {
        if (const auto buffer_map{FindBufferMap(params.offset)}; buffer_map) {
            const auto cpu_addr{static_cast<VAddr>(buffer_map->CpuAddr() + params.buffer_offset)};
            const auto gpu_addr{static_cast<GPUVAddr>(params.offset + params.buffer_offset)};

            if (!gpu.MemoryManager().Map(cpu_addr, gpu_addr, params.mapping_size)) {
                LOG_CRITICAL(Service_NVDRV,
                             "remap failed, flags={:X}, nvmap_handle={:X}, buffer_offset={}, "
                             "mapping_size = {}, offset={}",
                             params.flags, params.nvmap_handle, params.buffer_offset,
                             params.mapping_size, params.offset);

                std::memcpy(output.data(), &params, output.size());
                return NvErrCodes::InvalidInput;
            }

            std::memcpy(output.data(), &params, output.size());
            return NvErrCodes::Success;
        } else {
            LOG_CRITICAL(Service_NVDRV, "address not mapped offset={}", params.offset);

            std::memcpy(output.data(), &params, output.size());
            return NvErrCodes::InvalidInput;
        }
    }

    // We can only map objects that have already been assigned a CPU address.
    ASSERT(object->status == nvmap::Object::Status::Allocated);

    const auto physical_address{object->addr + params.buffer_offset};
    u64 size{params.mapping_size};
    if (!size) {
        size = object->size;
    }

    const bool is_alloc{(params.flags & AddressSpaceFlags::FixedOffset) == AddressSpaceFlags::None};
    if (is_alloc) {
        params.offset = gpu.MemoryManager().MapAllocate(physical_address, size, page_size);
    } else {
        params.offset = gpu.MemoryManager().Map(physical_address, params.offset, size);
    }

    auto result{NvErrCodes::Success};
    if (!params.offset) {
        LOG_CRITICAL(Service_NVDRV, "failed to map size={}", size);
        result = NvErrCodes::InvalidInput;
    } else {
        AddBufferMap(params.offset, size, physical_address, is_alloc);
    }

    std::memcpy(output.data(), &params, output.size());
    return result;
}

u32 nvhost_as_gpu::UnmapBuffer(const std::vector<u8>& input, std::vector<u8>& output) {
    IoctlUnmapBuffer params{};
    std::memcpy(&params, input.data(), input.size());

    LOG_DEBUG(Service_NVDRV, "called, offset=0x{:X}", params.offset);

    if (const auto size{RemoveBufferMap(params.offset)}; size) {
        system.GPU().MemoryManager().Unmap(params.offset, *size);
    } else {
        LOG_ERROR(Service_NVDRV, "invalid offset=0x{:X}", params.offset);
    }

    std::memcpy(output.data(), &params, output.size());
    return NvErrCodes::Success;
}

u32 nvhost_as_gpu::BindChannel(const std::vector<u8>& input, std::vector<u8>& output) {
    IoctlBindChannel params{};
    std::memcpy(&params, input.data(), input.size());

    LOG_DEBUG(Service_NVDRV, "called, fd={:X}", params.fd);

    channel = params.fd;
    return 0;
}

u32 nvhost_as_gpu::GetVARegions(const std::vector<u8>& input, std::vector<u8>& output) {
    IoctlGetVaRegions params{};
    std::memcpy(&params, input.data(), input.size());

    LOG_WARNING(Service_NVDRV, "(STUBBED) called, buf_addr={:X}, buf_size={:X}", params.buf_addr,
                params.buf_size);

    params.buf_size = 0x30;
    params.regions[0].offset = 0x04000000;
    params.regions[0].page_size = 0x1000;
    params.regions[0].pages = 0x3fbfff;

    params.regions[1].offset = 0x04000000;
    params.regions[1].page_size = 0x10000;
    params.regions[1].pages = 0x1bffff;

    // TODO(ogniK): This probably can stay stubbed but should add support way way later

    std::memcpy(output.data(), &params, output.size());
    return 0;
}

std::optional<nvhost_as_gpu::BufferMap> nvhost_as_gpu::FindBufferMap(GPUVAddr gpu_addr) const {
    const auto end{buffer_mappings.upper_bound(gpu_addr)};
    for (auto iter{buffer_mappings.begin()}; iter != end; ++iter) {
        if (gpu_addr >= iter->second.StartAddr() && gpu_addr < iter->second.EndAddr()) {
            return iter->second;
        }
    }

    return {};
}

void nvhost_as_gpu::AddBufferMap(GPUVAddr gpu_addr, std::size_t size, VAddr cpu_addr,
                                 bool is_allocated) {
    buffer_mappings[gpu_addr] = {gpu_addr, size, cpu_addr, is_allocated};
}

std::optional<std::size_t> nvhost_as_gpu::RemoveBufferMap(GPUVAddr gpu_addr) {
    if (const auto iter{buffer_mappings.find(gpu_addr)}; iter != buffer_mappings.end()) {
        std::size_t size{};

        if (iter->second.IsAllocated()) {
            size = iter->second.Size();
        }

        buffer_mappings.erase(iter);

        return size;
    }

    return {};
}

} // namespace Service::Nvidia::Devices
