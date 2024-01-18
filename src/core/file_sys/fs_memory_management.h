// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>
#include "common/alignment.h"

namespace FileSys {

std::mutex g_mutex;

constexpr size_t RequiredAlignment = alignof(u64);

void* AllocateUnsafe(size_t size) {
    /* Allocate. */
    void* const ptr = ::operator new(size, std::align_val_t{RequiredAlignment});

    /* Check alignment. */
    ASSERT(Common::IsAligned(reinterpret_cast<uintptr_t>(ptr), RequiredAlignment));

    /* Return allocated pointer. */
    return ptr;
}

void DeallocateUnsafe(void* ptr, size_t size) {
    /* Deallocate the pointer. */
    ::operator delete(ptr, std::align_val_t{RequiredAlignment});
}

void* Allocate(size_t size) {
    /* Lock the allocator. */
    std::scoped_lock lk(g_mutex);

    return AllocateUnsafe(size);
}

void Deallocate(void* ptr, size_t size) {
    /* If the pointer is non-null, deallocate it. */
    if (ptr != nullptr) {
        /* Lock the allocator. */
        std::scoped_lock lk(g_mutex);

        DeallocateUnsafe(ptr, size);
    }
}

} // namespace FileSys
