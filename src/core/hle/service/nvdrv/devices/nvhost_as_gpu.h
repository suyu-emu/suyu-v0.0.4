// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/service/nvdrv/devices/nvdevice.h"

namespace Service::Nvidia::Devices {

class nvmap;

enum class AddressSpaceFlags : u32 {
    None = 0x0,
    FixedOffset = 0x1,
    Remap = 0x100,
};
DECLARE_ENUM_FLAG_OPERATORS(AddressSpaceFlags);

class nvhost_as_gpu final : public nvdevice {
public:
    explicit nvhost_as_gpu(Core::System& system, std::shared_ptr<nvmap> nvmap_dev);
    ~nvhost_as_gpu() override;

    NvResult Ioctl1(Ioctl command, const std::vector<u8>& input, std::vector<u8>& output) override;
    NvResult Ioctl2(Ioctl command, const std::vector<u8>& input,
                    const std::vector<u8>& inline_input, std::vector<u8>& output) override;
    NvResult Ioctl3(Ioctl command, const std::vector<u8>& input, std::vector<u8>& output,
                    std::vector<u8>& inline_output) override;

private:
    class BufferMap final {
    public:
        constexpr BufferMap() = default;

        constexpr BufferMap(GPUVAddr start_addr, std::size_t size)
            : start_addr{start_addr}, end_addr{start_addr + size} {}

        constexpr BufferMap(GPUVAddr start_addr, std::size_t size, VAddr cpu_addr,
                            bool is_allocated)
            : start_addr{start_addr}, end_addr{start_addr + size}, cpu_addr{cpu_addr},
              is_allocated{is_allocated} {}

        constexpr VAddr StartAddr() const {
            return start_addr;
        }

        constexpr VAddr EndAddr() const {
            return end_addr;
        }

        constexpr std::size_t Size() const {
            return end_addr - start_addr;
        }

        constexpr VAddr CpuAddr() const {
            return cpu_addr;
        }

        constexpr bool IsAllocated() const {
            return is_allocated;
        }

    private:
        GPUVAddr start_addr{};
        GPUVAddr end_addr{};
        VAddr cpu_addr{};
        bool is_allocated{};
    };

    struct IoctlInitalizeEx {
        u32_le big_page_size{}; // depends on GPU's available_big_page_sizes; 0=default
        s32_le as_fd{};         // ignored; passes 0
        u32_le flags{};         // passes 0
        u32_le reserved{};      // ignored; passes 0
        u64_le unk0{};
        u64_le unk1{};
        u64_le unk2{};
    };
    static_assert(sizeof(IoctlInitalizeEx) == 40, "IoctlInitalizeEx is incorrect size");

    struct IoctlAllocSpace {
        u32_le pages{};
        u32_le page_size{};
        AddressSpaceFlags flags{};
        INSERT_PADDING_WORDS(1);
        union {
            u64_le offset;
            u64_le align;
        };
    };
    static_assert(sizeof(IoctlAllocSpace) == 24, "IoctlInitalizeEx is incorrect size");

    struct IoctlFreeSpace {
        u64_le offset{};
        u32_le pages{};
        u32_le page_size{};
    };
    static_assert(sizeof(IoctlFreeSpace) == 16, "IoctlFreeSpace is incorrect size");

    struct IoctlRemapEntry {
        u16_le flags{};
        u16_le kind{};
        u32_le nvmap_handle{};
        u32_le map_offset{};
        u32_le offset{};
        u32_le pages{};
    };
    static_assert(sizeof(IoctlRemapEntry) == 20, "IoctlRemapEntry is incorrect size");

    struct IoctlMapBufferEx {
        AddressSpaceFlags flags{}; // bit0: fixed_offset, bit2: cacheable
        u32_le kind{};             // -1 is default
        u32_le nvmap_handle{};
        u32_le page_size{}; // 0 means don't care
        s64_le buffer_offset{};
        u64_le mapping_size{};
        s64_le offset{};
    };
    static_assert(sizeof(IoctlMapBufferEx) == 40, "IoctlMapBufferEx is incorrect size");

    struct IoctlUnmapBuffer {
        s64_le offset{};
    };
    static_assert(sizeof(IoctlUnmapBuffer) == 8, "IoctlUnmapBuffer is incorrect size");

    struct IoctlBindChannel {
        s32_le fd{};
    };
    static_assert(sizeof(IoctlBindChannel) == 4, "IoctlBindChannel is incorrect size");

    struct IoctlVaRegion {
        u64_le offset{};
        u32_le page_size{};
        INSERT_PADDING_WORDS(1);
        u64_le pages{};
    };
    static_assert(sizeof(IoctlVaRegion) == 24, "IoctlVaRegion is incorrect size");

    struct IoctlGetVaRegions {
        u64_le buf_addr{}; // (contained output user ptr on linux, ignored)
        u32_le buf_size{}; // forced to 2*sizeof(struct va_region)
        u32_le reserved{};
        IoctlVaRegion regions[2]{};
    };
    static_assert(sizeof(IoctlGetVaRegions) == 16 + sizeof(IoctlVaRegion) * 2,
                  "IoctlGetVaRegions is incorrect size");

    s32 channel{};

    NvResult InitalizeEx(const std::vector<u8>& input, std::vector<u8>& output);
    NvResult AllocateSpace(const std::vector<u8>& input, std::vector<u8>& output);
    NvResult Remap(const std::vector<u8>& input, std::vector<u8>& output);
    NvResult MapBufferEx(const std::vector<u8>& input, std::vector<u8>& output);
    NvResult UnmapBuffer(const std::vector<u8>& input, std::vector<u8>& output);
    NvResult FreeSpace(const std::vector<u8>& input, std::vector<u8>& output);
    NvResult BindChannel(const std::vector<u8>& input, std::vector<u8>& output);

    NvResult GetVARegions(const std::vector<u8>& input, std::vector<u8>& output);
    NvResult GetVARegions(const std::vector<u8>& input, std::vector<u8>& output,
                          std::vector<u8>& inline_output);

    std::optional<BufferMap> FindBufferMap(GPUVAddr gpu_addr) const;
    void AddBufferMap(GPUVAddr gpu_addr, std::size_t size, VAddr cpu_addr, bool is_allocated);
    std::optional<std::size_t> RemoveBufferMap(GPUVAddr gpu_addr);

    std::shared_ptr<nvmap> nvmap_dev;

    // This is expected to be ordered, therefore we must use a map, not unordered_map
    std::map<GPUVAddr, BufferMap> buffer_mappings;
};

} // namespace Service::Nvidia::Devices
