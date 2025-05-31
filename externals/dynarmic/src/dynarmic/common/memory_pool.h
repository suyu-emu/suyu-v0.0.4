/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <cstddef>
#include <vector>

namespace Dynarmic::Common {

/// @tparam object_size Byte-size of objects to construct
/// @tparam slab_size Number of objects to have per slab
template<size_t object_size, size_t slab_size>
class Pool {
public:
    inline Pool() noexcept {
        AllocateNewSlab();
    }
    inline ~Pool() noexcept {
        std::free(current_slab);
        for (char* slab : slabs) {
            std::free(slab);
        }
    }

    Pool(const Pool&) = delete;
    Pool(Pool&&) = delete;

    Pool& operator=(const Pool&) = delete;
    Pool& operator=(Pool&&) = delete;

    /// @brief Returns a pointer to an `object_size`-bytes block of memory.
    [[nodiscard]] void* Alloc() noexcept {
        if (remaining == 0) {
            slabs.push_back(current_slab);
            AllocateNewSlab();
        }
        void* ret = static_cast<void*>(current_ptr);
        current_ptr += object_size;
        remaining--;
        return ret;
    }
private:
    /// @brief Allocates a completely new memory slab.
    /// Used when an entirely new slab is needed
    /// due the current one running out of usable space.
    void AllocateNewSlab() noexcept {
        current_slab = static_cast<char*>(std::malloc(object_size * slab_size));
        current_ptr = current_slab;
        remaining = slab_size;
    }

    std::vector<char*> slabs;
    char* current_slab = nullptr;
    char* current_ptr = nullptr;
    size_t remaining = 0;
};

}  // namespace Dynarmic::Common
