// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cstring>
#include <span>

#include "common/assert.h"
#include "common/common_types.h"

namespace Vulkan::LineLoop {

inline void CopyWithClosureRaw(std::span<u8> dst, std::span<const u8> src, size_t element_size) {
    ASSERT_MSG(dst.size() == src.size() + element_size, "Invalid line loop copy sizes");
    if (src.empty()) {
        if (!dst.empty()) {
            std::fill(dst.begin(), dst.end(), u8{0});
        }
        return;
    }
    std::memcpy(dst.data(), src.data(), src.size());
    std::memcpy(dst.data() + src.size(), src.data(), element_size);
}

inline void GenerateSequentialWithClosureRaw(std::span<u8> dst, size_t element_size,
                                             u64 start_value = 0) {
    if (dst.empty()) {
        return;
    }
    const size_t last = dst.size() - element_size;
    size_t offset = 0;
    u64 value = start_value;
    while (offset < last) {
        std::memcpy(dst.data() + offset, &value, element_size);
        offset += element_size;
        ++value;
    }
    std::memcpy(dst.data() + offset, &start_value, element_size);
}

template <typename T>
inline void CopyWithClosure(std::span<T> dst, std::span<const T> src) {
    ASSERT_MSG(dst.size() == src.size() + 1, "Invalid destination size for line loop copy");
    if (src.empty()) {
        if (!dst.empty()) {
            dst.front() = {};
        }
        return;
    }
    std::copy(src.begin(), src.end(), dst.begin());
    dst.back() = src.front();
}

template <typename T>
inline void GenerateSequentialWithClosure(std::span<T> dst, T start_value = {}) {
    if (dst.empty()) {
        return;
    }
    const size_t last = dst.size() - 1;
    for (size_t i = 0; i < last; ++i) {
        dst[i] = static_cast<T>(start_value + static_cast<T>(i));
    }
    dst.back() = start_value;
}

} // namespace Vulkan::LineLoop
