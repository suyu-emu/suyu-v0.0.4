// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>

#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/intrusive_red_black_tree.h"
#include "core/hle/kernel/k_auto_object.h"
#include "core/hle/kernel/k_auto_object_container.h"
#include "core/hle/kernel/k_light_lock.h"
#include "core/hle/kernel/k_slab_heap.h"

namespace Kernel {

template <class Derived>
class KSlabAllocated {
private:
    static inline KSlabHeap<Derived> s_slab_heap;

public:
    constexpr KSlabAllocated() = default;

    size_t GetSlabIndex() const {
        return s_slab_heap.GetIndex(static_cast<const Derived*>(this));
    }

public:
    static void InitializeSlabHeap(void* memory, size_t memory_size) {
        s_slab_heap.Initialize(memory, memory_size);
    }

    static Derived* Allocate() {
        return s_slab_heap.Allocate();
    }

    static void Free(Derived* obj) {
        s_slab_heap.Free(obj);
    }

    static size_t GetObjectSize() {
        return s_slab_heap.GetObjectSize();
    }
    static size_t GetSlabHeapSize() {
        return s_slab_heap.GetSlabHeapSize();
    }
    static size_t GetPeakIndex() {
        return s_slab_heap.GetPeakIndex();
    }
    static uintptr_t GetSlabHeapAddress() {
        return s_slab_heap.GetSlabHeapAddress();
    }

    static size_t GetNumRemaining() {
        return s_slab_heap.GetNumRemaining();
    }
};

template <typename Derived, typename Base>
class KAutoObjectWithSlabHeapAndContainer : public Base {
    static_assert(std::is_base_of<KAutoObjectWithList, Base>::value);

private:
    static inline KSlabHeap<Derived> s_slab_heap;
    static inline KAutoObjectWithListContainer s_container;

private:
    static Derived* Allocate() {
        return s_slab_heap.Allocate();
    }

    static void Free(Derived* obj) {
        s_slab_heap.Free(obj);
    }

public:
    class ListAccessor : public KAutoObjectWithListContainer::ListAccessor {
    public:
        ListAccessor() : KAutoObjectWithListContainer::ListAccessor(s_container) {}
        ~ListAccessor() = default;
    };

public:
    constexpr KAutoObjectWithSlabHeapAndContainer() : Base() {}
    virtual ~KAutoObjectWithSlabHeapAndContainer() {}

    virtual void Destroy() override {
        const bool is_initialized = this->IsInitialized();
        uintptr_t arg = 0;
        if (is_initialized) {
            s_container.Unregister(this);
            arg = this->GetPostDestroyArgument();
            this->Finalize();
        }
        Free(static_cast<Derived*>(this));
        if (is_initialized) {
            Derived::PostDestroy(arg);
        }
    }

    virtual bool IsInitialized() const {
        return true;
    }
    virtual uintptr_t GetPostDestroyArgument() const {
        return 0;
    }

    size_t GetSlabIndex() const {
        return s_slab_heap.GetObjectIndex(static_cast<const Derived*>(this));
    }

public:
    static void InitializeSlabHeap(void* memory, size_t memory_size) {
        s_slab_heap.Initialize(memory, memory_size);
        s_container.Initialize();
    }

    static Derived* Create() {
        Derived* obj = Allocate();
        if (AMS_LIKELY(obj != nullptr)) {
            KAutoObject::Create(obj);
        }
        return obj;
    }

    static void Register(Derived* obj) {
        return s_container.Register(obj);
    }

    static size_t GetObjectSize() {
        return s_slab_heap.GetObjectSize();
    }
    static size_t GetSlabHeapSize() {
        return s_slab_heap.GetSlabHeapSize();
    }
    static size_t GetPeakIndex() {
        return s_slab_heap.GetPeakIndex();
    }
    static uintptr_t GetSlabHeapAddress() {
        return s_slab_heap.GetSlabHeapAddress();
    }

    static size_t GetNumRemaining() {
        return s_slab_heap.GetNumRemaining();
    }
};

} // namespace Kernel
