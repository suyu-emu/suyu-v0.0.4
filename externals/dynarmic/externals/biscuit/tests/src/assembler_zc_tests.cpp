#include <catch/catch.hpp>

#include <array>
#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("C.LBU", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_LBU(x12, 0, x15);
    REQUIRE(value == 0x8390U);

    as.RewindBuffer();

    as.C_LBU(x12, 1, x15);
    REQUIRE(value == 0x83D0U);

    as.RewindBuffer();

    as.C_LBU(x12, 2, x15);
    REQUIRE(value == 0x83B0U);

    as.RewindBuffer();

    as.C_LBU(x12, 3, x15);
    REQUIRE(value == 0x83F0U);
}

TEST_CASE("C.LH", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_LH(x12, 0, x15);
    REQUIRE(value == 0x87D0U);

    as.RewindBuffer();

    as.C_LH(x12, 2, x15);
    REQUIRE(value == 0x87F0U);
}

TEST_CASE("C.LHU", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_LHU(x12, 0, x15);
    REQUIRE(value == 0x8790U);

    as.RewindBuffer();

    as.C_LHU(x12, 2, x15);
    REQUIRE(value == 0x87B0U);
}

TEST_CASE("C.SB", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_SB(x12, 0, x15);
    REQUIRE(value == 0x8B90U);

    as.RewindBuffer();

    as.C_SB(x12, 1, x15);
    REQUIRE(value == 0x8BD0U);

    as.RewindBuffer();

    as.C_SB(x12, 2, x15);
    REQUIRE(value == 0x8BB0U);

    as.RewindBuffer();

    as.C_SB(x12, 3, x15);
    REQUIRE(value == 0x8BF0U);
}

TEST_CASE("C.SH", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_SH(x12, 0, x15);
    REQUIRE(value == 0x8F90U);

    as.RewindBuffer();

    as.C_SH(x12, 2, x15);
    REQUIRE(value == 0x8FB0U);
}

TEST_CASE("C.SEXT.B", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_SEXT_B(x12);
    REQUIRE(value == 0x9E65);

    as.RewindBuffer();

    as.C_SEXT_B(x15);
    REQUIRE(value == 0x9FE5);
}

TEST_CASE("C.SEXT.H", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_SEXT_H(x12);
    REQUIRE(value == 0x9E6D);

    as.RewindBuffer();

    as.C_SEXT_H(x15);
    REQUIRE(value == 0x9FED);
}

TEST_CASE("C.ZEXT.B", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_ZEXT_B(x12);
    REQUIRE(value == 0x9E61);

    as.RewindBuffer();

    as.C_ZEXT_B(x15);
    REQUIRE(value == 0x9FE1);
}

TEST_CASE("C.ZEXT.H", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_ZEXT_H(x12);
    REQUIRE(value == 0x9E69);

    as.RewindBuffer();

    as.C_ZEXT_H(x15);
    REQUIRE(value == 0x9FE9);
}

TEST_CASE("C.ZEXT.W", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_ZEXT_W(x12);
    REQUIRE(value == 0x9E71);

    as.RewindBuffer();

    as.C_ZEXT_W(x15);
    REQUIRE(value == 0x9FF1);
}

TEST_CASE("C.MUL", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_MUL(x12, x15);
    REQUIRE(value == 0x9E5D);

    as.RewindBuffer();

    as.C_MUL(x15, x12);
    REQUIRE(value == 0x9FD1);
}

TEST_CASE("C.NOT", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_NOT(x12);
    REQUIRE(value == 0x9E75);

    as.RewindBuffer();

    as.C_NOT(x15);
    REQUIRE(value == 0x9FF5);
}

TEST_CASE("CM.MVA01S", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.CM_MVA01S(s7, s6);
    REQUIRE(value == 0xAFFA);

    as.RewindBuffer();

    as.CM_MVA01S(s3, s4);
    REQUIRE(value == 0xADF2);
}

TEST_CASE("CM.MVSA01", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.CM_MVSA01(s7, s6);
    REQUIRE(value == 0xAFBA);

    as.RewindBuffer();

    as.CM_MVSA01(s3, s4);
    REQUIRE(value == 0xADB2);
}

TEST_CASE("CM.JALT", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 32; i <= 255; i++) {
        const uint32_t op_base = 0xA002;
        const uint32_t op = op_base | (i << 2);

        as.CM_JALT(i);
        REQUIRE(value == op);

        as.RewindBuffer();
    }
}

TEST_CASE("CM.JT", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 0; i <= 31; i++) {
        const uint32_t op_base = 0xA002;
        const uint32_t op = op_base | (i << 2);

        as.CM_JT(i);
        REQUIRE(value == op);

        as.RewindBuffer();
    }
}

constexpr std::array stack_adj_bases_rv32{
    0, 0, 0, 0, 16, 16, 16, 16,
    32, 32, 32, 32, 48, 48, 48, 64,
};
constexpr std::array stack_adj_bases_rv64{
    0, 0, 0, 0, 16, 16, 32, 32,
    48, 48, 64, 64, 80, 80, 96, 112,
};

TEST_CASE("CM.POP", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.CM_POP({ra}, 16);
    REQUIRE(value == 0xBA42);
    as.RewindBuffer();

    // s10 intentionally omitted, since no direct encoding for it exists.
    uint32_t rlist = 5;
    for (const GPR sreg : {s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s11}) {
        const auto op_base = 0xBA02U;
        const auto stack_adj_base = stack_adj_bases_rv64[rlist];

        for (int32_t i = 0; i <= 3; i++) {
            const auto op = op_base | (rlist << 4) | uint32_t(i) << 2;

            as.CM_POP({ra, {s0, sreg}}, stack_adj_base + (16 * i));
            REQUIRE(value == op);
            as.RewindBuffer();
        }

        rlist++;
    }
}

TEST_CASE("CM.POP (RV32)", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.CM_POP({ra}, 16);
    REQUIRE(value == 0xBA42);
    as.RewindBuffer();

    // s10 intentionally omitted, since no direct encoding for it exists.
    uint32_t rlist = 5;
    for (const GPR sreg : {s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s11}) {
        const auto op_base = 0xBA02U;
        const auto stack_adj_base = stack_adj_bases_rv32[rlist];

        for (int32_t i = 0; i <= 3; i++) {
            const auto op = op_base | (rlist << 4) | uint32_t(i) << 2;

            as.CM_POP({ra, {s0, sreg}}, stack_adj_base + (16 * i));
            REQUIRE(value == op);
            as.RewindBuffer();
        }

        rlist++;
    }
}

TEST_CASE("CM.POPRET", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.CM_POPRET({ra}, 16);
    REQUIRE(value == 0xBE42);
    as.RewindBuffer();

    // s10 intentionally omitted, since no direct encoding for it exists.
    uint32_t rlist = 5;
    for (const GPR sreg : {s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s11}) {
        const auto op_base = 0xBE02U;
        const auto stack_adj_base = stack_adj_bases_rv64[rlist];

        for (int32_t i = 0; i <= 3; i++) {
            const auto op = op_base | (rlist << 4) | uint32_t(i) << 2;

            as.CM_POPRET({ra, {s0, sreg}}, stack_adj_base + (16 * i));
            REQUIRE(value == op);
            as.RewindBuffer();
        }

        rlist++;
    }
}

TEST_CASE("CM.POPRET (RV32)", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.CM_POPRET({ra}, 16);
    REQUIRE(value == 0xBE42);
    as.RewindBuffer();

    // s10 intentionally omitted, since no direct encoding for it exists.
    uint32_t rlist = 5;
    for (const GPR sreg : {s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s11}) {
        const auto op_base = 0xBE02U;
        const auto stack_adj_base = stack_adj_bases_rv32[rlist];

        for (int32_t i = 0; i <= 3; i++) {
            const auto op = op_base | (rlist << 4) | uint32_t(i) << 2;

            as.CM_POPRET({ra, {s0, sreg}}, stack_adj_base + (16 * i));
            REQUIRE(value == op);
            as.RewindBuffer();
        }

        rlist++;
    }
}

TEST_CASE("CM.POPRETZ", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.CM_POPRETZ({ra}, 16);
    REQUIRE(value == 0xBC42);
    as.RewindBuffer();

    // s10 intentionally omitted, since no direct encoding for it exists.
    uint32_t rlist = 5;
    for (const GPR sreg : {s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s11}) {
        const auto op_base = 0xBC02U;
        const auto stack_adj_base = stack_adj_bases_rv64[rlist];

        for (int32_t i = 0; i <= 3; i++) {
            const auto op = op_base | (rlist << 4) | uint32_t(i) << 2;

            as.CM_POPRETZ({ra, {s0, sreg}}, stack_adj_base + (16 * i));
            REQUIRE(value == op);
            as.RewindBuffer();
        }

        rlist++;
    }
}

TEST_CASE("CM.POPRETZ (RV32)", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.CM_POPRETZ({ra}, 16);
    REQUIRE(value == 0xBC42);
    as.RewindBuffer();

    // s10 intentionally omitted, since no direct encoding for it exists.
    uint32_t rlist = 5;
    for (const GPR sreg : {s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s11}) {
        const auto op_base = 0xBC02U;
        const auto stack_adj_base = stack_adj_bases_rv32[rlist];

        for (int32_t i = 0; i <= 3; i++) {
            const auto op = op_base | (rlist << 4) | uint32_t(i) << 2;

            as.CM_POPRETZ({ra, {s0, sreg}}, stack_adj_base + (16 * i));
            REQUIRE(value == op);
            as.RewindBuffer();
        }

        rlist++;
    }
}

TEST_CASE("CM.PUSH", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.CM_PUSH({ra}, -16);
    REQUIRE(value == 0xB842);
    as.RewindBuffer();

    // s10 intentionally omitted, since no direct encoding for it exists.
    uint32_t rlist = 5;
    for (const GPR sreg : {s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s11}) {
        const auto op_base = 0xB802U;
        const auto stack_adj_base = stack_adj_bases_rv64[rlist];

        for (int32_t i = 0; i <= 3; i++) {
            const auto op = op_base | (rlist << 4) | uint32_t(i) << 2;

            as.CM_PUSH({ra, {s0, sreg}}, -stack_adj_base + (-16 * i));
            REQUIRE(value == op);
            as.RewindBuffer();
        }

        rlist++;
    }
}

TEST_CASE("CM.PUSH (RV32)", "[Zc]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.CM_PUSH({ra}, -16);
    REQUIRE(value == 0xB842);
    as.RewindBuffer();

    // s10 intentionally omitted, since no direct encoding for it exists.
    uint32_t rlist = 5;
    for (const GPR sreg : {s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s11}) {
        const auto op_base = 0xB802U;
        const auto stack_adj_base = stack_adj_bases_rv32[rlist];

        for (int32_t i = 0; i <= 3; i++) {
            const auto op = op_base | (rlist << 4) | uint32_t(i) << 2;

            as.CM_PUSH({ra, {s0, sreg}}, -stack_adj_base + (-16 * i));
            REQUIRE(value == op);
            as.RewindBuffer();
        }

        rlist++;
    }
}
