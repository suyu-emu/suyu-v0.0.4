// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include "common/common_types.h"
#include "common/memory_hook.h"

namespace Common {
struct PageTable;
}

namespace Core {
class System;
}

namespace Kernel {
class Process;
}

namespace Memory {

/**
 * Page size used by the ARM architecture. This is the smallest granularity with which memory can
 * be mapped.
 */
constexpr std::size_t PAGE_BITS = 12;
constexpr u64 PAGE_SIZE = 1ULL << PAGE_BITS;
constexpr u64 PAGE_MASK = PAGE_SIZE - 1;

/// Virtual user-space memory regions
enum : VAddr {
    /// TLS (Thread-Local Storage) related.
    TLS_ENTRY_SIZE = 0x200,

    /// Application stack
    DEFAULT_STACK_SIZE = 0x100000,

    /// Kernel Virtual Address Range
    KERNEL_REGION_VADDR = 0xFFFFFF8000000000,
    KERNEL_REGION_SIZE = 0x7FFFE00000,
    KERNEL_REGION_END = KERNEL_REGION_VADDR + KERNEL_REGION_SIZE,
};

/// Central class that handles all memory operations and state.
class Memory {
public:
    explicit Memory(Core::System& system);
    ~Memory();

    Memory(const Memory&) = delete;
    Memory& operator=(const Memory&) = delete;

    Memory(Memory&&) = default;
    Memory& operator=(Memory&&) = default;

    /**
     * Maps an allocated buffer onto a region of the emulated process address space.
     *
     * @param page_table The page table of the emulated process.
     * @param base       The address to start mapping at. Must be page-aligned.
     * @param size       The amount of bytes to map. Must be page-aligned.
     * @param target     Buffer with the memory backing the mapping. Must be of length at least
     *                   `size`.
     */
    void MapMemoryRegion(Common::PageTable& page_table, VAddr base, u64 size, u8* target);

    /**
     * Maps a region of the emulated process address space as a IO region.
     *
     * @param page_table   The page table of the emulated process.
     * @param base         The address to start mapping at. Must be page-aligned.
     * @param size         The amount of bytes to map. Must be page-aligned.
     * @param mmio_handler The handler that backs the mapping.
     */
    void MapIoRegion(Common::PageTable& page_table, VAddr base, u64 size,
                     Common::MemoryHookPointer mmio_handler);

    /**
     * Unmaps a region of the emulated process address space.
     *
     * @param page_table The page table of the emulated process.
     * @param base       The address to begin unmapping at.
     * @param size       The amount of bytes to unmap.
     */
    void UnmapRegion(Common::PageTable& page_table, VAddr base, u64 size);

    /**
     * Adds a memory hook to intercept reads and writes to given region of memory.
     *
     * @param page_table The page table of the emulated process
     * @param base       The starting address to apply the hook to.
     * @param size       The size of the memory region to apply the hook to, in bytes.
     * @param hook       The hook to apply to the region of memory.
     */
    void AddDebugHook(Common::PageTable& page_table, VAddr base, u64 size,
                      Common::MemoryHookPointer hook);

    /**
     * Removes a memory hook from a given range of memory.
     *
     * @param page_table The page table of the emulated process.
     * @param base       The starting address to remove the hook from.
     * @param size       The size of the memory region to remove the hook from, in bytes.
     * @param hook       The hook to remove from the specified region of memory.
     */
    void RemoveDebugHook(Common::PageTable& page_table, VAddr base, u64 size,
                         Common::MemoryHookPointer hook);

    /**
     * Checks whether or not the supplied address is a valid virtual
     * address for the given process.
     *
     * @param process The emulated process to check the address against.
     * @param vaddr   The virtual address to check the validity of.
     *
     * @returns True if the given virtual address is valid, false otherwise.
     */
    bool IsValidVirtualAddress(const Kernel::Process& process, VAddr vaddr) const;

    /**
     * Checks whether or not the supplied address is a valid virtual
     * address for the current process.
     *
     * @param vaddr The virtual address to check the validity of.
     *
     * @returns True if the given virtual address is valid, false otherwise.
     */
    bool IsValidVirtualAddress(VAddr vaddr) const;

    /**
     * Gets a pointer to the given address.
     *
     * @param vaddr Virtual address to retrieve a pointer to.
     *
     * @returns The pointer to the given address, if the address is valid.
     *          If the address is not valid, nullptr will be returned.
     */
    u8* GetPointer(VAddr vaddr);

    /**
     * Gets a pointer to the given address.
     *
     * @param vaddr Virtual address to retrieve a pointer to.
     *
     * @returns The pointer to the given address, if the address is valid.
     *          If the address is not valid, nullptr will be returned.
     */
    const u8* GetPointer(VAddr vaddr) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

/// Changes the currently active page table to that of
/// the given process instance.
void SetCurrentPageTable(Kernel::Process& process);

/// Determines if the given VAddr is a kernel address
bool IsKernelVirtualAddress(VAddr vaddr);

u8 Read8(VAddr addr);
u16 Read16(VAddr addr);
u32 Read32(VAddr addr);
u64 Read64(VAddr addr);

void Write8(VAddr addr, u8 data);
void Write16(VAddr addr, u16 data);
void Write32(VAddr addr, u32 data);
void Write64(VAddr addr, u64 data);

void ReadBlock(const Kernel::Process& process, VAddr src_addr, void* dest_buffer, std::size_t size);
void ReadBlock(VAddr src_addr, void* dest_buffer, std::size_t size);
void WriteBlock(const Kernel::Process& process, VAddr dest_addr, const void* src_buffer,
                std::size_t size);
void WriteBlock(VAddr dest_addr, const void* src_buffer, std::size_t size);
void ZeroBlock(const Kernel::Process& process, VAddr dest_addr, std::size_t size);
void CopyBlock(VAddr dest_addr, VAddr src_addr, std::size_t size);

std::string ReadCString(VAddr vaddr, std::size_t max_length);

/**
 * Mark each page touching the region as cached.
 */
void RasterizerMarkRegionCached(VAddr vaddr, u64 size, bool cached);

} // namespace Memory
