// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <fmt/ranges.h>

#include "common/common_types.h"
#include "shader_recompiler/exception.h"

namespace Shader::IR {

enum class FlowTest : u64 {
#define SRIR_FLOW_TEST_LIST \
    SRIR_FLOW_TEST_ELEM(F) \
    SRIR_FLOW_TEST_ELEM(LT) \
    SRIR_FLOW_TEST_ELEM(EQ) \
    SRIR_FLOW_TEST_ELEM(LE) \
    SRIR_FLOW_TEST_ELEM(GT) \
    SRIR_FLOW_TEST_ELEM(NE) \
    SRIR_FLOW_TEST_ELEM(GE) \
    SRIR_FLOW_TEST_ELEM(NUM) \
    SRIR_FLOW_TEST_ELEM(NaN) \
    SRIR_FLOW_TEST_ELEM(LTU) \
    SRIR_FLOW_TEST_ELEM(EQU) \
    SRIR_FLOW_TEST_ELEM(LEU) \
    SRIR_FLOW_TEST_ELEM(GTU) \
    SRIR_FLOW_TEST_ELEM(NEU) \
    SRIR_FLOW_TEST_ELEM(GEU) \
    SRIR_FLOW_TEST_ELEM(T) \
    SRIR_FLOW_TEST_ELEM(OFF) \
    SRIR_FLOW_TEST_ELEM(LO) \
    SRIR_FLOW_TEST_ELEM(SFF) \
    SRIR_FLOW_TEST_ELEM(LS) \
    SRIR_FLOW_TEST_ELEM(HI) \
    SRIR_FLOW_TEST_ELEM(SFT) \
    SRIR_FLOW_TEST_ELEM(HS) \
    SRIR_FLOW_TEST_ELEM(OFT) \
    SRIR_FLOW_TEST_ELEM(CSM_TA) \
    SRIR_FLOW_TEST_ELEM(CSM_TR) \
    SRIR_FLOW_TEST_ELEM(CSM_MX) \
    SRIR_FLOW_TEST_ELEM(FCSM_TA) \
    SRIR_FLOW_TEST_ELEM(FCSM_TR) \
    SRIR_FLOW_TEST_ELEM(FCSM_MX) \
    SRIR_FLOW_TEST_ELEM(RLE) \
    SRIR_FLOW_TEST_ELEM(RGT)
#define SRIR_FLOW_TEST_ELEM(n) n,
    SRIR_FLOW_TEST_LIST
#undef SRIR_FLOW_TEST_ELEM
};

[[nodiscard]] inline std::string NameOf(FlowTest flow_test) {
    switch (flow_test) {
#define SRIR_FLOW_TEST_ELEM(n) case FlowTest::n: return #n;
    SRIR_FLOW_TEST_LIST
#undef SRIR_FLOW_TEST_ELEM
    default:
        return fmt::format("<invalid flow test {}>", int(flow_test));
    }
}

} // namespace Shader::IR

template <>
struct fmt::formatter<Shader::IR::FlowTest> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Shader::IR::FlowTest& flow_test, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", Shader::IR::NameOf(flow_test));
    }
};
