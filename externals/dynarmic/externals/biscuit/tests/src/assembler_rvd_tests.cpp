#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("FADD.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FADD_D(f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0x03A38FD3);

    as.RewindBuffer();

    as.FADD_D(f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0x03A3CFD3);

    as.RewindBuffer();

    as.FADD_D(f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0x03A3FFD3);
}

TEST_CASE("FCLASS.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCLASS_D(x31, f7);
    REQUIRE(value == 0xE2039FD3);

    as.RewindBuffer();

    as.FCLASS_D(x7, f31);
    REQUIRE(value == 0xE20F93D3);
}

TEST_CASE("FCVT.D.S", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_D_S(f31, f7, RMode::RNE);
    REQUIRE(value == 0x42038FD3);

    as.RewindBuffer();

    as.FCVT_D_S(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4203CFD3);

    as.RewindBuffer();

    as.FCVT_D_S(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4203FFD3);
}

TEST_CASE("FCVT.D.W", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_D_W(f31, x7, RMode::RNE);
    REQUIRE(value == 0xD2038FD3);

    as.RewindBuffer();

    as.FCVT_D_W(f31, x7, RMode::RMM);
    REQUIRE(value == 0xD203CFD3);

    as.RewindBuffer();

    as.FCVT_D_W(f31, x7, RMode::DYN);
    REQUIRE(value == 0xD203FFD3);
}

TEST_CASE("FCVT.D.WU", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_D_WU(f31, x7, RMode::RNE);
    REQUIRE(value == 0xD2138FD3);

    as.RewindBuffer();

    as.FCVT_D_WU(f31, x7, RMode::RMM);
    REQUIRE(value == 0xD213CFD3);

    as.RewindBuffer();

    as.FCVT_D_WU(f31, x7, RMode::DYN);
    REQUIRE(value == 0xD213FFD3);
}

TEST_CASE("FCVT.L.D", "[rv64d]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVT_L_D(x31, f7, RMode::RNE);
    REQUIRE(value == 0xC2238FD3);

    as.RewindBuffer();

    as.FCVT_L_D(x31, f7, RMode::RMM);
    REQUIRE(value == 0xC223CFD3);

    as.RewindBuffer();

    as.FCVT_L_D(x31, f7, RMode::DYN);
    REQUIRE(value == 0xC223FFD3);
}

TEST_CASE("FCVT.LU.D", "[rv64d]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVT_LU_D(x31, f7, RMode::RNE);
    REQUIRE(value == 0xC2338FD3);

    as.RewindBuffer();

    as.FCVT_LU_D(x31, f7, RMode::RMM);
    REQUIRE(value == 0xC233CFD3);

    as.RewindBuffer();

    as.FCVT_LU_D(x31, f7, RMode::DYN);
    REQUIRE(value == 0xC233FFD3);
}

TEST_CASE("FCVT.D.L", "[rv64d]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVT_D_L(f31, x7, RMode::RNE);
    REQUIRE(value == 0xD2238FD3);

    as.RewindBuffer();

    as.FCVT_D_L(f31, x7, RMode::RMM);
    REQUIRE(value == 0xD223CFD3);

    as.RewindBuffer();

    as.FCVT_D_L(f31, x7, RMode::DYN);
    REQUIRE(value == 0xD223FFD3);
}

TEST_CASE("FCVT.D.LU", "[rv64d]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVT_D_LU(f31, x7, RMode::RNE);
    REQUIRE(value == 0xD2338FD3);

    as.RewindBuffer();

    as.FCVT_D_LU(f31, x7, RMode::RMM);
    REQUIRE(value == 0xD233CFD3);

    as.RewindBuffer();

    as.FCVT_D_LU(f31, x7, RMode::DYN);
    REQUIRE(value == 0xD233FFD3);
}

TEST_CASE("FCVT.W.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_W_D(x31, f7, RMode::RNE);
    REQUIRE(value == 0xC2038FD3);

    as.RewindBuffer();

    as.FCVT_W_D(x31, f7, RMode::RMM);
    REQUIRE(value == 0xC203CFD3);

    as.RewindBuffer();

    as.FCVT_W_D(x31, f7, RMode::DYN);
    REQUIRE(value == 0xC203FFD3);
}

TEST_CASE("FCVT.WU.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_WU_D(x31, f7, RMode::RNE);
    REQUIRE(value == 0xC2138FD3);

    as.RewindBuffer();

    as.FCVT_WU_D(x31, f7, RMode::RMM);
    REQUIRE(value == 0xC213CFD3);

    as.RewindBuffer();

    as.FCVT_WU_D(x31, f7, RMode::DYN);
    REQUIRE(value == 0xC213FFD3);
}

TEST_CASE("FCVT.S.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_S_D(f31, f7, RMode::RNE);
    REQUIRE(value == 0x40138FD3);

    as.RewindBuffer();

    as.FCVT_S_D(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4013CFD3);

    as.RewindBuffer();

    as.FCVT_S_D(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4013FFD3);
}

TEST_CASE("FDIV.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FDIV_D(f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0x1BA38FD3);

    as.RewindBuffer();

    as.FDIV_D(f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0x1BA3CFD3);

    as.RewindBuffer();

    as.FDIV_D(f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0x1BA3FFD3);
}

TEST_CASE("FEQ.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FEQ_D(x31, f7, f26);
    REQUIRE(value == 0xA3A3AFD3);

    as.RewindBuffer();

    as.FEQ_D(x31, f26, f7);
    REQUIRE(value == 0xA27D2FD3);
}

TEST_CASE("FLE.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FLE_D(x31, f7, f26);
    REQUIRE(value == 0xA3A38FD3);

    as.RewindBuffer();

    as.FLE_D(x31, f26, f7);
    REQUIRE(value == 0xA27D0FD3);
}

TEST_CASE("FLT.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FLT_D(x31, f7, f26);
    REQUIRE(value == 0xA3A39FD3);

    as.RewindBuffer();

    as.FLT_D(x31, f26, f7);
    REQUIRE(value == 0xA27D1FD3);
}

TEST_CASE("FLD", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FLD(f15, 1024, x31);
    REQUIRE(value == 0x400FB787);

    as.RewindBuffer();

    as.FLD(f15, 1536, x31);
    REQUIRE(value == 0x600FB787);

    as.RewindBuffer();

    as.FLD(f15, -1, x31);
    REQUIRE(value == 0xFFFFB787);
}

TEST_CASE("FMADD.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMADD_D(f15, f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0xD27F87C3);

    as.RewindBuffer();

    as.FMADD_D(f15, f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0xD27FC7C3);

    as.RewindBuffer();

    as.FMADD_D(f15, f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0xD27FF7C3);
}

TEST_CASE("FMAX.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMAX_D(f31, f7, f26);
    REQUIRE(value == 0x2BA39FD3);

    as.RewindBuffer();

    as.FMAX_D(f31, f31, f31);
    REQUIRE(value == 0x2BFF9FD3);
}

TEST_CASE("FMIN.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMIN_D(f31, f7, f26);
    REQUIRE(value == 0x2BA38FD3);

    as.RewindBuffer();

    as.FMIN_D(f31, f31, f31);
    REQUIRE(value == 0x2BFF8FD3);
}

TEST_CASE("FMSUB.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMSUB_D(f15, f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0xD27F87C7);

    as.RewindBuffer();

    as.FMSUB_D(f15, f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0xD27FC7C7);

    as.RewindBuffer();

    as.FMSUB_D(f15, f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0xD27FF7C7);
}

TEST_CASE("FMUL.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMUL_D(f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0x13A38FD3);

    as.RewindBuffer();

    as.FMUL_D(f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0x13A3CFD3);

    as.RewindBuffer();

    as.FMUL_D(f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0x13A3FFD3);
}

TEST_CASE("FMV.D.X", "[rv64d]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FMV_D_X(f31, x7);
    REQUIRE(value == 0xF2038FD3);

    as.RewindBuffer();

    as.FMV_D_X(f7, x31);
    REQUIRE(value == 0xF20F83D3);
}

TEST_CASE("FMV.X.D", "[rv64d]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FMV_X_D(x31, f7);
    REQUIRE(value == 0xE2038FD3);

    as.RewindBuffer();

    as.FMV_X_D(x7, f31);
    REQUIRE(value == 0xE20F83D3);
}

TEST_CASE("FNMADD.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FNMADD_D(f15, f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0xD27F87CF);

    as.RewindBuffer();

    as.FNMADD_D(f15, f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0xD27FC7CF);

    as.RewindBuffer();

    as.FNMADD_D(f15, f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0xD27FF7CF);
}

TEST_CASE("FNMSUB.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FNMSUB_D(f15, f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0xD27F87CB);

    as.RewindBuffer();

    as.FNMSUB_D(f15, f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0xD27FC7CB);

    as.RewindBuffer();

    as.FNMSUB_D(f15, f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0xD27FF7CB);
}

TEST_CASE("FSGNJ.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSGNJ_D(f31, f7, f26);
    REQUIRE(value == 0x23A38FD3);

    as.RewindBuffer();

    as.FSGNJ_D(f31, f31, f31);
    REQUIRE(value == 0x23FF8FD3);
}

TEST_CASE("FSGNJN.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSGNJN_D(f31, f7, f26);
    REQUIRE(value == 0x23A39FD3);

    as.RewindBuffer();

    as.FSGNJN_D(f31, f31, f31);
    REQUIRE(value == 0x23FF9FD3);
}

TEST_CASE("FSGNJX.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSGNJX_D(f31, f7, f26);
    REQUIRE(value == 0x23A3AFD3);

    as.RewindBuffer();

    as.FSGNJX_D(f31, f31, f31);
    REQUIRE(value == 0x23FFAFD3);
}

TEST_CASE("FSQRT.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSQRT_D(f31, f7, RMode::RNE);
    REQUIRE(value == 0x5A038FD3);

    as.RewindBuffer();

    as.FSQRT_D(f31, f7, RMode::RMM);
    REQUIRE(value == 0x5A03CFD3);

    as.RewindBuffer();

    as.FSQRT_D(f31, f7, RMode::DYN);
    REQUIRE(value == 0x5A03FFD3);
}

TEST_CASE("FSUB.D", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSUB_D(f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0x0BA38FD3);

    as.RewindBuffer();

    as.FSUB_D(f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0x0BA3CFD3);

    as.RewindBuffer();

    as.FSUB_D(f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0x0BA3FFD3);
}

TEST_CASE("FSD", "[rv32d]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSD(f31, 1024, x15);
    REQUIRE(value == 0x41F7B027);

    as.RewindBuffer();

    as.FSD(f31, 1536, x15);
    REQUIRE(value == 0x61F7B027);

    as.RewindBuffer();

    as.FSD(f31, -1, x15);
    REQUIRE(value == 0xFFF7BFA7);
}
