// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <vector>
#include "common/common_types.h"
#include "core/hle/result.h"
#include "core/memory.h"
#include "core/memory_hook.h"

namespace Kernel {

enum class VMAType : u8 {
    /// VMA represents an unmapped region of the address space.
    Free,
    /// VMA is backed by a ref-counted allocate memory block.
    AllocatedMemoryBlock,
    /// VMA is backed by a raw, unmanaged pointer.
    BackingMemory,
    /// VMA is mapped to MMIO registers at a fixed PAddr.
    MMIO,
    // TODO(yuriks): Implement MemoryAlias to support MAP/UNMAP
};

/// Permissions for mapped memory blocks
enum class VMAPermission : u8 {
    None = 0,
    Read = 1,
    Write = 2,
    Execute = 4,

    ReadWrite = Read | Write,
    ReadExecute = Read | Execute,
    WriteExecute = Write | Execute,
    ReadWriteExecute = Read | Write | Execute,
};

/// Set of values returned in MemoryInfo.state by svcQueryMemory.
enum class MemoryState : u32 {
    Unmapped = 0x0,
    Io = 0x1,
    Normal = 0x2,
    CodeStatic = 0x3,
    CodeMutable = 0x4,
    Heap = 0x5,
    Shared = 0x6,
    ModuleCodeStatic = 0x8,
    ModuleCodeMutable = 0x9,
    IpcBuffer0 = 0xA,
    Mapped = 0xB,
    ThreadLocal = 0xC,
    TransferMemoryIsolated = 0xD,
    TransferMemory = 0xE,
    ProcessMemory = 0xF,
    IpcBuffer1 = 0x11,
    IpcBuffer3 = 0x12,
    KernelStack = 0x13,
};

/**
 * Represents a VMA in an address space. A VMA is a contiguous region of virtual addressing space
 * with homogeneous attributes across its extents. In this particular implementation each VMA is
 * also backed by a single host memory allocation.
 */
struct VirtualMemoryArea {
    /// Virtual base address of the region.
    VAddr base = 0;
    /// Size of the region.
    u64 size = 0;

    VMAType type = VMAType::Free;
    VMAPermission permissions = VMAPermission::None;
    /// Tag returned by svcQueryMemory. Not otherwise used.
    MemoryState meminfo_state = MemoryState::Unmapped;

    // Settings for type = AllocatedMemoryBlock
    /// Memory block backing this VMA.
    std::shared_ptr<std::vector<u8>> backing_block = nullptr;
    /// Offset into the backing_memory the mapping starts from.
    std::size_t offset = 0;

    // Settings for type = BackingMemory
    /// Pointer backing this VMA. It will not be destroyed or freed when the VMA is removed.
    u8* backing_memory = nullptr;

    // Settings for type = MMIO
    /// Physical address of the register area this VMA maps to.
    PAddr paddr = 0;
    Memory::MemoryHookPointer mmio_handler = nullptr;

    /// Tests if this area can be merged to the right with `next`.
    bool CanBeMergedWith(const VirtualMemoryArea& next) const;
};

/**
 * Manages a process' virtual addressing space. This class maintains a list of allocated and free
 * regions in the address space, along with their attributes, and allows kernel clients to
 * manipulate it, adjusting the page table to match.
 *
 * This is similar in idea and purpose to the VM manager present in operating system kernels, with
 * the main difference being that it doesn't have to support swapping or memory mapping of files.
 * The implementation is also simplified by not having to allocate page frames. See these articles
 * about the Linux kernel for an explantion of the concept and implementation:
 *  - http://duartes.org/gustavo/blog/post/how-the-kernel-manages-your-memory/
 *  - http://duartes.org/gustavo/blog/post/page-cache-the-affair-between-memory-and-files/
 */
class VMManager final {
public:
    /**
     * The maximum amount of address space managed by the kernel.
     * @todo This was selected arbitrarily, and should be verified for Switch OS.
     */
    static constexpr VAddr MAX_ADDRESS{0x1000000000ULL};

    /**
     * A map covering the entirety of the managed address space, keyed by the `base` field of each
     * VMA. It must always be modified by splitting or merging VMAs, so that the invariant
     * `elem.base + elem.size == next.base` is preserved, and mergeable regions must always be
     * merged when possible so that no two similar and adjacent regions exist that have not been
     * merged.
     */
    std::map<VAddr, VirtualMemoryArea> vma_map;
    using VMAHandle = decltype(vma_map)::const_iterator;

    VMManager();
    ~VMManager();

    /// Clears the address space map, re-initializing with a single free area.
    void Reset();

    /// Finds the VMA in which the given address is included in, or `vma_map.end()`.
    VMAHandle FindVMA(VAddr target) const;

    // TODO(yuriks): Should these functions actually return the handle?

    /**
     * Maps part of a ref-counted block of memory at a given address.
     *
     * @param target The guest address to start the mapping at.
     * @param block The block to be mapped.
     * @param offset Offset into `block` to map from.
     * @param size Size of the mapping.
     * @param state MemoryState tag to attach to the VMA.
     */
    ResultVal<VMAHandle> MapMemoryBlock(VAddr target, std::shared_ptr<std::vector<u8>> block,
                                        std::size_t offset, u64 size, MemoryState state);

    /**
     * Maps an unmanaged host memory pointer at a given address.
     *
     * @param target The guest address to start the mapping at.
     * @param memory The memory to be mapped.
     * @param size Size of the mapping.
     * @param state MemoryState tag to attach to the VMA.
     */
    ResultVal<VMAHandle> MapBackingMemory(VAddr target, u8* memory, u64 size, MemoryState state);

    /**
     * Maps a memory-mapped IO region at a given address.
     *
     * @param target The guest address to start the mapping at.
     * @param paddr The physical address where the registers are present.
     * @param size Size of the mapping.
     * @param state MemoryState tag to attach to the VMA.
     * @param mmio_handler The handler that will implement read and write for this MMIO region.
     */
    ResultVal<VMAHandle> MapMMIO(VAddr target, PAddr paddr, u64 size, MemoryState state,
                                 Memory::MemoryHookPointer mmio_handler);

    /// Unmaps a range of addresses, splitting VMAs as necessary.
    ResultCode UnmapRange(VAddr target, u64 size);

    /// Changes the permissions of the given VMA.
    VMAHandle Reprotect(VMAHandle vma, VMAPermission new_perms);

    /// Changes the permissions of a range of addresses, splitting VMAs as necessary.
    ResultCode ReprotectRange(VAddr target, u64 size, VMAPermission new_perms);

    /**
     * Scans all VMAs and updates the page table range of any that use the given vector as backing
     * memory. This should be called after any operation that causes reallocation of the vector.
     */
    void RefreshMemoryBlockMappings(const std::vector<u8>* block);

    /// Dumps the address space layout to the log, for debugging
    void LogLayout() const;

    /// Gets the total memory usage, used by svcGetInfo
    u64 GetTotalMemoryUsage() const;

    /// Gets the total heap usage, used by svcGetInfo
    u64 GetTotalHeapUsage() const;

    /// Gets the total address space base address, used by svcGetInfo
    VAddr GetAddressSpaceBaseAddr() const;

    /// Gets the total address space address size, used by svcGetInfo
    u64 GetAddressSpaceSize() const;

    /// Each VMManager has its own page table, which is set as the main one when the owning process
    /// is scheduled.
    Memory::PageTable page_table;

private:
    using VMAIter = decltype(vma_map)::iterator;

    /// Converts a VMAHandle to a mutable VMAIter.
    VMAIter StripIterConstness(const VMAHandle& iter);

    /// Unmaps the given VMA.
    VMAIter Unmap(VMAIter vma);

    /**
     * Carves a VMA of a specific size at the specified address by splitting Free VMAs while doing
     * the appropriate error checking.
     */
    ResultVal<VMAIter> CarveVMA(VAddr base, u64 size);

    /**
     * Splits the edges of the given range of non-Free VMAs so that there is a VMA split at each
     * end of the range.
     */
    ResultVal<VMAIter> CarveVMARange(VAddr base, u64 size);

    /**
     * Splits a VMA in two, at the specified offset.
     * @returns the right side of the split, with the original iterator becoming the left side.
     */
    VMAIter SplitVMA(VMAIter vma, u64 offset_in_vma);

    /**
     * Checks for and merges the specified VMA with adjacent ones if possible.
     * @returns the merged VMA or the original if no merging was possible.
     */
    VMAIter MergeAdjacent(VMAIter vma);

    /// Updates the pages corresponding to this VMA so they match the VMA's attributes.
    void UpdatePageTableForVMA(const VirtualMemoryArea& vma);
};
} // namespace Kernel
