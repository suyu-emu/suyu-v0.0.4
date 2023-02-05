// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {
namespace {

constexpr bool IsValidSetMemoryPermission(MemoryPermission perm) {
    switch (perm) {
    case MemoryPermission::None:
    case MemoryPermission::Read:
    case MemoryPermission::ReadWrite:
        return true;
    default:
        return false;
    }
}

// Checks if address + size is greater than the given address
// This can return false if the size causes an overflow of a 64-bit type
// or if the given size is zero.
constexpr bool IsValidAddressRange(VAddr address, u64 size) {
    return address + size > address;
}

// Helper function that performs the common sanity checks for svcMapMemory
// and svcUnmapMemory. This is doable, as both functions perform their sanitizing
// in the same order.
Result MapUnmapMemorySanityChecks(const KPageTable& manager, VAddr dst_addr, VAddr src_addr,
                                  u64 size) {
    if (!Common::Is4KBAligned(dst_addr)) {
        LOG_ERROR(Kernel_SVC, "Destination address is not aligned to 4KB, 0x{:016X}", dst_addr);
        return ResultInvalidAddress;
    }

    if (!Common::Is4KBAligned(src_addr)) {
        LOG_ERROR(Kernel_SVC, "Source address is not aligned to 4KB, 0x{:016X}", src_addr);
        return ResultInvalidSize;
    }

    if (size == 0) {
        LOG_ERROR(Kernel_SVC, "Size is 0");
        return ResultInvalidSize;
    }

    if (!Common::Is4KBAligned(size)) {
        LOG_ERROR(Kernel_SVC, "Size is not aligned to 4KB, 0x{:016X}", size);
        return ResultInvalidSize;
    }

    if (!IsValidAddressRange(dst_addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Destination is not a valid address range, addr=0x{:016X}, size=0x{:016X}",
                  dst_addr, size);
        return ResultInvalidCurrentMemory;
    }

    if (!IsValidAddressRange(src_addr, size)) {
        LOG_ERROR(Kernel_SVC, "Source is not a valid address range, addr=0x{:016X}, size=0x{:016X}",
                  src_addr, size);
        return ResultInvalidCurrentMemory;
    }

    if (!manager.IsInsideAddressSpace(src_addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Source is not within the address space, addr=0x{:016X}, size=0x{:016X}",
                  src_addr, size);
        return ResultInvalidCurrentMemory;
    }

    if (manager.IsOutsideStackRegion(dst_addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Destination is not within the stack region, addr=0x{:016X}, size=0x{:016X}",
                  dst_addr, size);
        return ResultInvalidMemoryRegion;
    }

    if (manager.IsInsideHeapRegion(dst_addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Destination does not fit within the heap region, addr=0x{:016X}, "
                  "size=0x{:016X}",
                  dst_addr, size);
        return ResultInvalidMemoryRegion;
    }

    if (manager.IsInsideAliasRegion(dst_addr, size)) {
        LOG_ERROR(Kernel_SVC,
                  "Destination does not fit within the map region, addr=0x{:016X}, "
                  "size=0x{:016X}",
                  dst_addr, size);
        return ResultInvalidMemoryRegion;
    }

    return ResultSuccess;
}

} // namespace

Result SetMemoryPermission(Core::System& system, VAddr address, u64 size, MemoryPermission perm) {
    LOG_DEBUG(Kernel_SVC, "called, address=0x{:016X}, size=0x{:X}, perm=0x{:08X", address, size,
              perm);

    // Validate address / size.
    R_UNLESS(Common::IsAligned(address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(size, PageSize), ResultInvalidSize);
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS((address < address + size), ResultInvalidCurrentMemory);

    // Validate the permission.
    R_UNLESS(IsValidSetMemoryPermission(perm), ResultInvalidNewMemoryPermission);

    // Validate that the region is in range for the current process.
    auto& page_table = system.Kernel().CurrentProcess()->PageTable();
    R_UNLESS(page_table.Contains(address, size), ResultInvalidCurrentMemory);

    // Set the memory attribute.
    return page_table.SetMemoryPermission(address, size, perm);
}

Result SetMemoryAttribute(Core::System& system, VAddr address, u64 size, u32 mask, u32 attr) {
    LOG_DEBUG(Kernel_SVC,
              "called, address=0x{:016X}, size=0x{:X}, mask=0x{:08X}, attribute=0x{:08X}", address,
              size, mask, attr);

    // Validate address / size.
    R_UNLESS(Common::IsAligned(address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(size, PageSize), ResultInvalidSize);
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS((address < address + size), ResultInvalidCurrentMemory);

    // Validate the attribute and mask.
    constexpr u32 SupportedMask = static_cast<u32>(MemoryAttribute::Uncached);
    R_UNLESS((mask | attr) == mask, ResultInvalidCombination);
    R_UNLESS((mask | attr | SupportedMask) == SupportedMask, ResultInvalidCombination);

    // Validate that the region is in range for the current process.
    auto& page_table{system.Kernel().CurrentProcess()->PageTable()};
    R_UNLESS(page_table.Contains(address, size), ResultInvalidCurrentMemory);

    // Set the memory attribute.
    return page_table.SetMemoryAttribute(address, size, mask, attr);
}

Result SetMemoryAttribute32(Core::System& system, u32 address, u32 size, u32 mask, u32 attr) {
    return SetMemoryAttribute(system, address, size, mask, attr);
}

/// Maps a memory range into a different range.
Result MapMemory(Core::System& system, VAddr dst_addr, VAddr src_addr, u64 size) {
    LOG_TRACE(Kernel_SVC, "called, dst_addr=0x{:X}, src_addr=0x{:X}, size=0x{:X}", dst_addr,
              src_addr, size);

    auto& page_table{system.Kernel().CurrentProcess()->PageTable()};

    if (const Result result{MapUnmapMemorySanityChecks(page_table, dst_addr, src_addr, size)};
        result.IsError()) {
        return result;
    }

    return page_table.MapMemory(dst_addr, src_addr, size);
}

Result MapMemory32(Core::System& system, u32 dst_addr, u32 src_addr, u32 size) {
    return MapMemory(system, dst_addr, src_addr, size);
}

/// Unmaps a region that was previously mapped with svcMapMemory
Result UnmapMemory(Core::System& system, VAddr dst_addr, VAddr src_addr, u64 size) {
    LOG_TRACE(Kernel_SVC, "called, dst_addr=0x{:X}, src_addr=0x{:X}, size=0x{:X}", dst_addr,
              src_addr, size);

    auto& page_table{system.Kernel().CurrentProcess()->PageTable()};

    if (const Result result{MapUnmapMemorySanityChecks(page_table, dst_addr, src_addr, size)};
        result.IsError()) {
        return result;
    }

    return page_table.UnmapMemory(dst_addr, src_addr, size);
}

Result UnmapMemory32(Core::System& system, u32 dst_addr, u32 src_addr, u32 size) {
    return UnmapMemory(system, dst_addr, src_addr, size);
}

} // namespace Kernel::Svc
