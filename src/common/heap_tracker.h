// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>
#include <shared_mutex>
#include <ankerl/unordered_dense.h>
#include "common/host_memory.h"

namespace Common {

struct SeparateHeapMap {
    PAddr paddr{}; //8
    std::size_t size{}; //8 (16)
    std::size_t tick{}; //8 (24)
    // 4 bits needed, sync with host_memory.h if needed
    MemoryPermission perm : 4 = MemoryPermission::Read;
    bool is_resident : 1 = false;
};
static_assert(sizeof(SeparateHeapMap) == 32); //half a cache line! good for coherency

class HeapTracker {
public:
    explicit HeapTracker(Common::HostMemory& buffer);
    ~HeapTracker();
    void Map(size_t virtual_offset, size_t host_offset, size_t length, MemoryPermission perm, bool is_separate_heap);
    void Unmap(size_t virtual_offset, size_t size, bool is_separate_heap);
    void Protect(size_t virtual_offset, size_t length, MemoryPermission perm);
    inline u8* VirtualBasePointer() noexcept {
        return m_buffer.VirtualBasePointer();
    }
private:
    // TODO: You may want to "fake-map" the first 2GB of 64-bit address space
    // and dedicate it entirely to a recursive PTE mapping :)
    // However Ankerl is way better than using an RB tree, in all senses
    using AddrTree = ankerl::unordered_dense::map<VAddr, SeparateHeapMap>;
    AddrTree m_mappings;
    using TicksTree = ankerl::unordered_dense::map<VAddr, SeparateHeapMap>;
    TicksTree m_resident_mappings;
private:
    void SplitHeapMap(VAddr offset, size_t size);
    void SplitHeapMapLocked(VAddr offset);
    void RebuildSeparateHeapAddressSpace();
    inline HeapTracker::AddrTree::iterator GetNearestHeapMapLocked(VAddr offset) noexcept {
        return m_mappings.find(offset);
    }
private:
    Common::HostMemory& m_buffer;
    const s64 m_max_resident_map_count;
    std::shared_mutex m_rebuild_lock{};
    std::mutex m_lock{};
    s64 m_map_count{};
    s64 m_resident_map_count{};
    size_t m_tick{};
};

} // namespace Common
