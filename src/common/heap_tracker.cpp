// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <fstream>
#include "common/heap_tracker.h"
#include "common/logging/log.h"
#include "common/assert.h"

namespace Common {

namespace {

s64 GetMaxPermissibleResidentMapCount() {
    // Default value.
    s64 value = 65530;

    // Try to read how many mappings we can make.
    std::ifstream s("/proc/sys/vm/max_map_count");
    s >> value;

    // Print, for debug.
    LOG_INFO(HW_Memory, "Current maximum map count: {}", value);

    // Allow 20000 maps for other code and to account for split inaccuracy.
    return std::max<s64>(value - 20000, 0);
}

} // namespace

HeapTracker::HeapTracker(Common::HostMemory& buffer)
    : m_buffer(buffer), m_max_resident_map_count(GetMaxPermissibleResidentMapCount()) {}
HeapTracker::~HeapTracker() = default;

void HeapTracker::Map(size_t virtual_offset, size_t host_offset, size_t length,
                      MemoryPermission perm, bool is_separate_heap) {
    bool rebuild_required = false;
    // When mapping other memory, map pages immediately.
    if (!is_separate_heap) {
        m_buffer.Map(virtual_offset, host_offset, length, perm, false);
        return;
    }
    {
        // We are mapping part of a separate heap and insert into mappings.
        std::scoped_lock lk{m_lock};
        m_map_count++;
        const auto it = m_mappings.insert_or_assign(virtual_offset, SeparateHeapMap{
            .paddr = host_offset,
            .size = length,
            .tick = m_tick++,
            .perm = perm,
            .is_resident = false,
        });
        // Update tick before possible rebuild.
        it.first->second.tick = m_tick++;
        // Check if we need to rebuild.
        if (m_resident_map_count >= m_max_resident_map_count)
            rebuild_required = true;
        // Map the area.
        m_buffer.Map(it.first->first, it.first->second.paddr, it.first->second.size, it.first->second.perm, false);
        // This map is now resident.
        it.first->second.is_resident = true;
        m_resident_map_count++;
        m_resident_mappings.insert(*it.first);
    }
    // A rebuild was required, so perform it now.
    if (rebuild_required)
        this->RebuildSeparateHeapAddressSpace();
}

void HeapTracker::Unmap(size_t virtual_offset, size_t size, bool is_separate_heap) {
    // If this is a separate heap...
    if (is_separate_heap) {
        std::scoped_lock lk{m_lock};
        // Split at the boundaries of the region we are removing.
        this->SplitHeapMapLocked(virtual_offset);
        this->SplitHeapMapLocked(virtual_offset + size);
        // Erase all mappings in range.
        auto it = m_mappings.find(virtual_offset);
        while (it != m_mappings.end() && it->first < virtual_offset + size) {
            // If resident, erase from resident map.
            if (it->second.is_resident) {
                ASSERT(--m_resident_map_count >= 0);
                m_resident_mappings.erase(m_resident_mappings.find(it->first));
            }
            // Erase from map.
            ASSERT(--m_map_count >= 0);
            it = m_mappings.erase(it);
        }
    }
    // Unmap pages.
    m_buffer.Unmap(virtual_offset, size, false);
}

void HeapTracker::Protect(size_t virtual_offset, size_t size, MemoryPermission perm) {
    // Ensure no rebuild occurs while reprotecting.
    std::shared_lock lk{m_rebuild_lock};

    // Split at the boundaries of the region we are reprotecting.
    this->SplitHeapMap(virtual_offset, size);

    // Declare tracking variables.
    const VAddr end = virtual_offset + size;
    VAddr cur = virtual_offset;

    while (cur < end) {
        VAddr next = cur;
        bool should_protect = false;

        {
            std::scoped_lock lk2{m_lock};
            // Try to get the next mapping corresponding to this address.
            const auto it = m_mappings.find(next);
            if (it == m_mappings.end()) {
                // There are no separate heap mappings remaining.
                next = end;
                should_protect = true;
            } else if (it->first == cur) {
                // We are in range.
                // Update permission bits.
                it->second.perm = perm;

                // Determine next address and whether we should protect.
                next = cur + it->second.size;
                should_protect = it->second.is_resident;
            } else /* if (it->vaddr > cur) */ {
                // We weren't in range, but there is a block coming up that will be.
                next = it->first;
                should_protect = true;
            }
        }

        // Clamp to end.
        next = std::min(next, end);
        // Reprotect, if we need to.
        if (should_protect)
            m_buffer.Protect(cur, next - cur, perm);
        // Advance.
        cur = next;
    }
}

void HeapTracker::RebuildSeparateHeapAddressSpace() {
    std::scoped_lock lk{m_rebuild_lock, m_lock};
    ASSERT(!m_resident_mappings.empty());
    // Dump half of the mappings.
    // Despite being worse in theory, this has proven to be better in practice than more
    // regularly dumping a smaller amount, because it significantly reduces average case
    // lock contention.
    std::size_t const desired_count = std::min(m_resident_map_count, m_max_resident_map_count) / 2;
    std::size_t const evict_count = m_resident_map_count - desired_count;
    auto it = m_resident_mappings.begin();
    for (std::size_t i = 0; i < evict_count && it != m_resident_mappings.end(); i++) {
        // Unmark and unmap.
        it->second.is_resident = false;
        m_buffer.Unmap(it->first, it->second.size, false);
        // Advance.
        ASSERT(--m_resident_map_count >= 0);
        it = m_resident_mappings.erase(it);
    }
}

void HeapTracker::SplitHeapMap(VAddr offset, size_t size) {
    std::scoped_lock lk{m_lock};
    this->SplitHeapMapLocked(offset);
    this->SplitHeapMapLocked(offset + size);
}

void HeapTracker::SplitHeapMapLocked(VAddr offset) {
    auto it = this->GetNearestHeapMapLocked(offset);
    if (it != m_mappings.end() && it->first != offset) {
        // Adjust left iterator
        auto const orig_size = it->second.size;
        auto const left_size = offset - it->first;
        it->second.size = left_size;
        // Insert the new right map.
        auto const right = SeparateHeapMap{
            .paddr = it->second.paddr + left_size,
            .size = orig_size - left_size,
            .tick = it->second.tick,
            .perm = it->second.perm,
            .is_resident = it->second.is_resident,
        };
        m_map_count++;
        auto rit = m_mappings.insert_or_assign(it->first + left_size, right);
        if (rit.first->second.is_resident) {
            m_resident_map_count++;
            m_resident_mappings.insert(*rit.first);
        }
    }
}

} // namespace Common
