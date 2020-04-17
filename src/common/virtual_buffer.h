// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_funcs.h"

namespace Common {

void* AllocateMemoryPages(std::size_t size);
void FreeMemoryPages(void* base, std::size_t size);

template <typename T>
class VirtualBuffer final : NonCopyable {
public:
    constexpr VirtualBuffer() = default;
    explicit VirtualBuffer(std::size_t count) : alloc_size{count * sizeof(T)} {
        base_ptr = reinterpret_cast<T*>(AllocateMemoryPages(alloc_size));
    }

    ~VirtualBuffer() {
        FreeMemoryPages(base_ptr, alloc_size);
    }

    void resize(std::size_t count) {
        FreeMemoryPages(base_ptr, alloc_size);

        alloc_size = count * sizeof(T);
        base_ptr = reinterpret_cast<T*>(AllocateMemoryPages(alloc_size));
    }

    constexpr const T& operator[](std::size_t index) const {
        return base_ptr[index];
    }

    constexpr T& operator[](std::size_t index) {
        return base_ptr[index];
    }

    constexpr T* data() {
        return base_ptr;
    }

    constexpr const T* data() const {
        return base_ptr;
    }

    constexpr std::size_t size() const {
        return alloc_size / sizeof(T);
    }

private:
    std::size_t alloc_size{};
    T* base_ptr{};
};

} // namespace Common
