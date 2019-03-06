// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "video_core/gpu.h"
#include "video_core/memory_manager.h"

namespace Tegra::Engines {

#define KEPLER_COMPUTE_REG_INDEX(field_name)                                                       \
    (offsetof(Tegra::Engines::KeplerCompute::Regs, field_name) / sizeof(u32))

class KeplerCompute final {
public:
    explicit KeplerCompute(MemoryManager& memory_manager);
    ~KeplerCompute();

    static constexpr std::size_t NumConstBuffers = 8;

    struct Regs {
        static constexpr std::size_t NUM_REGS = 0xCF8;

        union {
            struct {
                INSERT_PADDING_WORDS(0xAF);

                u32 launch;

                INSERT_PADDING_WORDS(0xC48);
            };
            std::array<u32, NUM_REGS> reg_array;
        };
    } regs{};
    static_assert(sizeof(Regs) == Regs::NUM_REGS * sizeof(u32),
                  "KeplerCompute Regs has wrong size");

    MemoryManager& memory_manager;

    /// Write the value to the register identified by method.
    void CallMethod(const GPU::MethodCall& method_call);
};

#define ASSERT_REG_POSITION(field_name, position)                                                  \
    static_assert(offsetof(KeplerCompute::Regs, field_name) == position * 4,                       \
                  "Field " #field_name " has invalid position")

ASSERT_REG_POSITION(launch, 0xAF);

#undef ASSERT_REG_POSITION

} // namespace Tegra::Engines
