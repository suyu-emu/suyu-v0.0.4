// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <mutex>

#include "common/common_types.h"
#include "core/hle/kernel/memory/page_heap.h"
#include "core/hle/result.h"

namespace Kernel::Memory {

class PageLinkedList;

class MemoryManager final : NonCopyable {
public:
    enum class Pool : u32 {
        Application = 0,
        Applet = 1,
        System = 2,
        SystemNonSecure = 3,

        Count,

        Shift = 4,
        Mask = (0xF << Shift),
    };

    enum class Direction : u32 {
        FromFront = 0,
        FromBack = 1,

        Shift = 0,
        Mask = (0xF << Shift),
    };

    MemoryManager() = default;

    constexpr std::size_t GetSize(Pool pool) const {
        return managers[static_cast<std::size_t>(pool)].GetSize();
    }

    void InitializeManager(Pool pool, u64 start_address, u64 end_address);
    VAddr AllocateContinuous(std::size_t num_pages, std::size_t align_pages, Pool pool,
                             Direction dir = Direction::FromFront);
    ResultCode Allocate(PageLinkedList& page_list, std::size_t num_pages, Pool pool,
                        Direction dir = Direction::FromFront);
    ResultCode Free(PageLinkedList& page_list, std::size_t num_pages, Pool pool,
                    Direction dir = Direction::FromFront);

    static constexpr std::size_t MaxManagerCount = 10;

private:
    class Impl final : NonCopyable {
    private:
        using RefCount = u16;

    private:
        PageHeap heap;
        Pool pool{};

    public:
        Impl() = default;

        std::size_t Initialize(Pool new_pool, u64 start_address, u64 end_address);

        VAddr AllocateBlock(s32 index) {
            return heap.AllocateBlock(index);
        }

        void Free(VAddr addr, std::size_t num_pages) {
            heap.Free(addr, num_pages);
        }

        constexpr std::size_t GetSize() const {
            return heap.GetSize();
        }

        constexpr VAddr GetAddress() const {
            return heap.GetAddress();
        }

        constexpr VAddr GetEndAddress() const {
            return heap.GetEndAddress();
        }
    };

private:
    std::array<std::mutex, static_cast<std::size_t>(Pool::Count)> pool_locks;
    std::array<Impl, MaxManagerCount> managers;
};

} // namespace Kernel::Memory
