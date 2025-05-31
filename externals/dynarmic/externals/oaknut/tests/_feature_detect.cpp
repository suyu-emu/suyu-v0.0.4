// SPDX-FileCopyrightText: Copyright (c) 2022 merryhime <https://mary.rs>
// SPDX-License-Identifier: MIT

#include <cstdio>

#include <catch2/catch_test_macros.hpp>

#include "architecture.hpp"

#ifdef ON_ARM64

#    include "oaknut/feature_detection/feature_detection.hpp"
#    include "oaknut/feature_detection/feature_detection_idregs.hpp"

using namespace oaknut;

TEST_CASE("Print CPU features (Default)")
{
    CpuFeatures features = detect_features();

    std::fputs("CPU Features: ", stdout);

#    define OAKNUT_CPU_FEATURE(name)        \
        if (features.has(CpuFeature::name)) \
            std::fputs(#name " ", stdout);
#    include "oaknut/impl/cpu_feature.inc.hpp"
#    undef OAKNUT_CPU_FEATURE

    std::fputs("\n", stdout);
}

#    if OAKNUT_SUPPORTS_READING_ID_REGISTERS == 1

TEST_CASE("Print CPU features (Using CPUID)")
{
    std::optional<id::IdRegisters> id_regs = read_id_registers();
    REQUIRE(!!id_regs);

    CpuFeatures features = detect_features_via_id_registers(*id_regs);

    std::fputs("CPU Features (CPUID method): ", stdout);

#        define OAKNUT_CPU_FEATURE(name)        \
            if (features.has(CpuFeature::name)) \
                std::fputs(#name " ", stdout);
#        include "oaknut/impl/cpu_feature.inc.hpp"
#        undef OAKNUT_CPU_FEATURE

    std::fputs("\n", stdout);
}

#    elif OAKNUT_SUPPORTS_READING_ID_REGISTERS == 2

TEST_CASE("Print CPU features (Using CPUID)")
{
    const std::size_t core_count = get_core_count();
    for (std::size_t core_index = 0; core_index < core_count; core_index++) {
        std::optional<id::IdRegisters> id_regs = read_id_registers(core_index);
        REQUIRE(!!id_regs);

        CpuFeatures features = detect_features_via_id_registers(*id_regs);

        std::printf("CPU Features (CPUID method - Core %zu): ", core_index);

#        define OAKNUT_CPU_FEATURE(name)        \
            if (features.has(CpuFeature::name)) \
                std::fputs(#name " ", stdout);
#        include "oaknut/impl/cpu_feature.inc.hpp"
#        undef OAKNUT_CPU_FEATURE

        std::fputs("\n", stdout);
    }
}

#    endif

#endif
