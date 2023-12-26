// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <vector>

#include "common/heap_tracker.h"
#include "common/logging/log.h"

namespace Common {

namespace {

constexpr s64 MaxResidentMapCount = 0x8000;

} // namespace

HeapTracker::HeapTracker(Common::HostMemory& buffer) : m_buffer(buffer) {}
HeapTracker::~HeapTracker() = default;

void HeapTracker::Map(size_t virtual_offset, size_t host_offset, size_t length,
                      MemoryPermission perm, bool is_separate_heap) {
    // When mapping other memory, map pages immediately.
    if (!is_separate_heap) {
        m_buffer.Map(virtual_offset, host_offset, length, perm, false);
        return;
    }

    {
        // We are mapping part of a separate heap.
        std::scoped_lock lk{m_lock};

        auto* const map = new SeparateHeapMap{
            .vaddr = virtual_offset,
            .paddr = host_offset,
            .size = length,
            .tick = m_tick++,
            .perm = perm,
            .is_resident = false,
        };

        // Insert into mappings.
        m_map_count++;
        m_mappings.insert(*map);
    }

    // Finally, map.
    this->DeferredMapSeparateHeap(virtual_offset);
}

void HeapTracker::Unmap(size_t virtual_offset, size_t size, bool is_separate_heap) {
    // If this is a separate heap...
    if (is_separate_heap) {
        std::scoped_lock lk{m_lock};

        const SeparateHeapMap key{
            .vaddr = virtual_offset,
        };

        // Split at the boundaries of the region we are removing.
        this->SplitHeapMapLocked(virtual_offset);
        this->SplitHeapMapLocked(virtual_offset + size);

        // Erase all mappings in range.
        auto it = m_mappings.find(key);
        while (it != m_mappings.end() && it->vaddr < virtual_offset + size) {
            // Get underlying item.
            auto* const item = std::addressof(*it);

            // If resident, erase from resident map.
            if (item->is_resident) {
                ASSERT(--m_resident_map_count >= 0);
                m_resident_mappings.erase(m_resident_mappings.iterator_to(*item));
            }

            // Erase from map.
            it = m_mappings.erase(it);
            ASSERT(--m_map_count >= 0);

            // Free the item.
            delete item;
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
    VAddr cur = virtual_offset;
    VAddr end = virtual_offset + size;

    while (cur < end) {
        VAddr next = cur;
        bool should_protect = false;

        {
            std::scoped_lock lk2{m_lock};

            const SeparateHeapMap key{
                .vaddr = next,
            };

            // Try to get the next mapping corresponding to this address.
            const auto it = m_mappings.nfind(key);

            if (it == m_mappings.end()) {
                // There are no separate heap mappings remaining.
                next = end;
                should_protect = true;
            } else if (it->vaddr == cur) {
                // We are in range.
                // Update permission bits.
                it->perm = perm;

                // Determine next address and whether we should protect.
                next = cur + it->size;
                should_protect = it->is_resident;
            } else /* if (it->vaddr > cur) */ {
                // We weren't in range, but there is a block coming up that will be.
                next = it->vaddr;
                should_protect = true;
            }
        }

        // Clamp to end.
        next = std::min(next, end);

        // Reprotect, if we need to.
        if (should_protect) {
            m_buffer.Protect(cur, next - cur, perm);
        }

        // Advance.
        cur = next;
    }
}

bool HeapTracker::DeferredMapSeparateHeap(u8* fault_address) {
    if (m_buffer.IsInVirtualRange(fault_address)) {
        return this->DeferredMapSeparateHeap(fault_address - m_buffer.VirtualBasePointer());
    }

    return false;
}

bool HeapTracker::DeferredMapSeparateHeap(size_t virtual_offset) {
    bool rebuild_required = false;

    {
        std::scoped_lock lk{m_lock};

        // Check to ensure this was a non-resident separate heap mapping.
        const auto it = this->GetNearestHeapMapLocked(virtual_offset);
        if (it == m_mappings.end() || it->is_resident) {
            return false;
        }

        // Update tick before possible rebuild.
        it->tick = m_tick++;

        // Check if we need to rebuild.
        if (m_resident_map_count > MaxResidentMapCount) {
            rebuild_required = true;
        }

        // Map the area.
        m_buffer.Map(it->vaddr, it->paddr, it->size, it->perm, false);

        // This map is now resident.
        it->is_resident = true;
        m_resident_map_count++;
        m_resident_mappings.insert(*it);
    }

    if (rebuild_required) {
        // A rebuild was required, so perform it now.
        this->RebuildSeparateHeapAddressSpace();
    }

    return true;
}

void HeapTracker::RebuildSeparateHeapAddressSpace() {
    std::scoped_lock lk{m_rebuild_lock, m_lock};

    ASSERT(!m_resident_mappings.empty());

    // Unmap so we have at least 4 maps available.
    const size_t desired_count = std::min(m_resident_map_count, MaxResidentMapCount - 4);
    const size_t evict_count = m_resident_map_count - desired_count;
    auto it = m_resident_mappings.begin();

    for (size_t i = 0; i < evict_count && it != m_resident_mappings.end(); i++) {
        // Unmark and unmap.
        it->is_resident = false;
        m_buffer.Unmap(it->vaddr, it->size, false);

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
    const auto it = this->GetNearestHeapMapLocked(offset);
    if (it == m_mappings.end() || it->vaddr == offset) {
        // Not contained or no split required.
        return;
    }

    // Cache the original values.
    auto* const left = std::addressof(*it);
    const size_t orig_size = left->size;

    // Adjust the left map.
    const size_t left_size = offset - left->vaddr;
    left->size = left_size;

    // Create the new right map.
    auto* const right = new SeparateHeapMap{
        .vaddr = left->vaddr + left_size,
        .paddr = left->paddr + left_size,
        .size = orig_size - left_size,
        .tick = left->tick,
        .perm = left->perm,
        .is_resident = left->is_resident,
    };

    // Insert the new right map.
    m_map_count++;
    m_mappings.insert(*right);

    // If resident, also insert into resident map.
    if (right->is_resident) {
        m_resident_mappings.insert(*right);
        m_resident_map_count++;
    }
}

HeapTracker::AddrTree::iterator HeapTracker::GetNearestHeapMapLocked(VAddr offset) {
    const SeparateHeapMap key{
        .vaddr = offset,
    };

    return m_mappings.find(key);
}

} // namespace Common
