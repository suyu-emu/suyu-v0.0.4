// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Kernel {

class KernelCore;

/// This is a placeholder class to manage slab heaps for kernel objects. For now, we just allocate
/// these with new/delete, but this can be re-implemented later to allocate these in emulated
/// memory.

template <typename T>
class KSlabHeap final : NonCopyable {
public:
    KSlabHeap() = default;

    void Initialize([[maybe_unused]] void* memory, [[maybe_unused]] std::size_t memory_size) {
        // Placeholder that should initialize the backing slab heap implementation.
    }

    T* Allocate() {
        return new T();
    }

    T* AllocateWithKernel(KernelCore& kernel) {
        return new T(kernel);
    }

    void Free(T* obj) {
        delete obj;
    }
};

} // namespace Kernel
