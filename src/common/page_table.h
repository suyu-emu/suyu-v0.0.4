// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <tuple>

#include "common/common_types.h"
#include "common/memory_hook.h"
#include "common/virtual_buffer.h"

namespace Common {

enum class PageType : u8 {
    /// Page is unmapped and should cause an access error.
    Unmapped,
    /// Page is mapped to regular memory. This is the only type you can get pointers to.
    Memory,
    /// Page is mapped to regular memory, but also needs to check for rasterizer cache flushing and
    /// invalidation
    RasterizerCachedMemory,
    /// Page is mapped to a I/O region. Writing and reading to this page is handled by functions.
    Special,
    /// Page is allocated for use.
    Allocated,
};

struct SpecialRegion {
    enum class Type {
        DebugHook,
        IODevice,
    } type;

    MemoryHookPointer handler;

    [[nodiscard]] bool operator<(const SpecialRegion& other) const {
        return std::tie(type, handler) < std::tie(other.type, other.handler);
    }

    [[nodiscard]] bool operator==(const SpecialRegion& other) const {
        return std::tie(type, handler) == std::tie(other.type, other.handler);
    }
};

/**
 * A (reasonably) fast way of allowing switchable and remappable process address spaces. It loosely
 * mimics the way a real CPU page table works.
 */
struct PageTable {
    PageTable();
    ~PageTable() noexcept;

    PageTable(const PageTable&) = delete;
    PageTable& operator=(const PageTable&) = delete;

    PageTable(PageTable&&) noexcept = default;
    PageTable& operator=(PageTable&&) noexcept = default;

    /**
     * Resizes the page table to be able to accomodate enough pages within
     * a given address space.
     *
     * @param address_space_width_in_bits The address size width in bits.
     * @param page_size_in_bits           The page size in bits.
     * @param has_attribute               Whether or not this page has any backing attributes.
     */
    void Resize(std::size_t address_space_width_in_bits, std::size_t page_size_in_bits,
                bool has_attribute);

    /**
     * Vector of memory pointers backing each page. An entry can only be non-null if the
     * corresponding entry in the `attributes` vector is of type `Memory`.
     */
    VirtualBuffer<u8*> pointers;

    VirtualBuffer<u64> backing_addr;

    VirtualBuffer<PageType> attributes;
};

} // namespace Common
