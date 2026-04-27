// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <utility>
#include <vector>

#include "common/common_types.h"

namespace VideoCommon {

class InvalidationAccumulator {
public:
    InvalidationAccumulator() = default;
    ~InvalidationAccumulator() = default;

    void Add(GPUVAddr address, size_t size) noexcept {
        auto const end_address = start_address + accumulated_size;
        if (!(address >= start_address && address + size <= end_address)) {
            size = ((address + size + atomicity_size_mask) & atomicity_mask) - address;
            address = address & atomicity_mask;
            if (start_address == 0) {
                start_address = address;
                accumulated_size = size;
                // will now have collected as this point
            } else if (address != end_address) {
                buffer.emplace_back(start_address, accumulated_size);
                start_address = address;
                accumulated_size = size;
            } else {
                accumulated_size += size;
            }
        }
    }

    template <typename F>
    [[nodiscard]] bool InvalidateAll(F&& f) noexcept {
        if (start_address > 0) {
            for (auto [address, size] : buffer)
                f(address, size);
            f(start_address, accumulated_size);
            buffer.clear();
            start_address = 0;
            accumulated_size = 0;
            return true;
        }
        return false;
    }

private:
    static constexpr size_t atomicity_bits = 5;
    static constexpr size_t atomicity_size = 1ULL << atomicity_bits;
    static constexpr size_t atomicity_size_mask = atomicity_size - 1;
    static constexpr size_t atomicity_mask = ~atomicity_size_mask;
    std::vector<std::pair<VAddr, size_t>> buffer;
    GPUVAddr start_address = 0;
    size_t accumulated_size = 0;
};

} // namespace VideoCommon
