/* This file is part of the dynarmic project.
 * Copyright (c) 2024 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <cstdint>
#include <new>

#include <sys/mman.h>

namespace Dynarmic::Backend::RV64 {

class CodeBlock {
public:
    explicit CodeBlock(std::size_t size)
            : memsize(size) {
        mem = (u8*)mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0);

        if (mem == nullptr)
            throw std::bad_alloc{};
    }

    ~CodeBlock() {
        if (mem == nullptr)
            return;

        munmap(mem, memsize);
    }

    template<typename T>
    T ptr() const {
        static_assert(std::is_pointer_v<T> || std::is_same_v<T, uptr> || std::is_same_v<T, sptr>);
        return reinterpret_cast<T>(mem);
    }

protected:
    u8* mem;
    size_t memsize = 0;
};
}  // namespace Dynarmic::Backend::RV64
