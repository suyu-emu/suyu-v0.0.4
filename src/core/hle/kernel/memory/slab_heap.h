// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// This file references various implementation details from Atmosphere, an open-source firmware for
// the Nintendo Switch. Copyright 2018-2020 Atmosphere-NX.

#pragma once

#include <atomic>

#include "common/assert.h"
#include "common/common_types.h"

namespace Kernel::Memory {

namespace impl {

class SlabHeapImpl final : NonCopyable {
public:
    struct Node {
        Node* next{};
    };

    constexpr SlabHeapImpl() = default;

    void Initialize(std::size_t size) {
        ASSERT(head == nullptr);
        obj_size = size;
    }

    constexpr std::size_t GetObjectSize() const {
        return obj_size;
    }

    Node* GetHead() const {
        return head;
    }

    void* Allocate() {
        Node* ret = head.load();

        do {
            if (ret == nullptr) {
                break;
            }
        } while (!head.compare_exchange_weak(ret, ret->next));

        return ret;
    }

    void Free(void* obj) {
        Node* node = static_cast<Node*>(obj);

        Node* cur_head = head.load();
        do {
            node->next = cur_head;
        } while (!head.compare_exchange_weak(cur_head, node));
    }

private:
    std::atomic<Node*> head{};
    std::size_t obj_size{};
};

} // namespace impl

class SlabHeapBase : NonCopyable {
public:
    constexpr SlabHeapBase() = default;

    constexpr bool Contains(uintptr_t addr) const {
        return start <= addr && addr < end;
    }

    constexpr std::size_t GetSlabHeapSize() const {
        return (end - start) / GetObjectSize();
    }

    constexpr std::size_t GetObjectSize() const {
        return impl.GetObjectSize();
    }

    constexpr uintptr_t GetSlabHeapAddress() const {
        return start;
    }

    std::size_t GetObjectIndexImpl(const void* obj) const {
        return (reinterpret_cast<uintptr_t>(obj) - start) / GetObjectSize();
    }

    std::size_t GetPeakIndex() const {
        return GetObjectIndexImpl(reinterpret_cast<const void*>(peak));
    }

    void* AllocateImpl() {
        return impl.Allocate();
    }

    void FreeImpl(void* obj) {
        // Don't allow freeing an object that wasn't allocated from this heap
        ASSERT(Contains(reinterpret_cast<uintptr_t>(obj)));
        impl.Free(obj);
    }

    void InitializeImpl(std::size_t obj_size, void* memory, std::size_t memory_size) {
        // Ensure we don't initialize a slab using null memory
        ASSERT(memory != nullptr);

        // Initialize the base allocator
        impl.Initialize(obj_size);

        // Set our tracking variables
        const std::size_t num_obj = (memory_size / obj_size);
        start = reinterpret_cast<uintptr_t>(memory);
        end = start + num_obj * obj_size;
        peak = start;

        // Free the objects
        u8* cur = reinterpret_cast<u8*>(end);

        for (std::size_t i{}; i < num_obj; i++) {
            cur -= obj_size;
            impl.Free(cur);
        }
    }

private:
    using Impl = impl::SlabHeapImpl;

    Impl impl;
    uintptr_t peak{};
    uintptr_t start{};
    uintptr_t end{};
};

template <typename T>
class SlabHeap final : public SlabHeapBase {
public:
    constexpr SlabHeap() : SlabHeapBase() {}

    void Initialize(void* memory, std::size_t memory_size) {
        InitializeImpl(sizeof(T), memory, memory_size);
    }

    T* Allocate() {
        T* obj = static_cast<T*>(AllocateImpl());
        if (obj != nullptr) {
            new (obj) T();
        }
        return obj;
    }

    void Free(T* obj) {
        FreeImpl(obj);
    }

    constexpr std::size_t GetObjectIndex(const T* obj) const {
        return GetObjectIndexImpl(obj);
    }
};

} // namespace Kernel::Memory
