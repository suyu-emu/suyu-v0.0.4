#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("FADD.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FADD_H(f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0x05A38FD3);

    as.RewindBuffer();

    as.FADD_H(f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0x05A3CFD3);

    as.RewindBuffer();

    as.FADD_H(f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0x05A3FFD3);
}

TEST_CASE("FADD.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FADD_S(f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0x01A38FD3);

    as.RewindBuffer();

    as.FADD_S(f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0x01A3CFD3);

    as.RewindBuffer();

    as.FADD_S(f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0x01A3FFD3);
}

TEST_CASE("FCLASS.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCLASS_H(x31, f7);
    REQUIRE(value == 0xE4039FD3);

    as.RewindBuffer();

    as.FCLASS_H(x7, f31);
    REQUIRE(value == 0xE40F93D3);
}

TEST_CASE("FCLASS.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCLASS_S(x31, f7);
    REQUIRE(value == 0xE0039FD3);

    as.RewindBuffer();

    as.FCLASS_S(x7, f31);
    REQUIRE(value == 0xE00F93D3);
}

TEST_CASE("FCVT.D.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_D_H(f31, f7, RMode::RNE);
    REQUIRE(value == 0x42238FD3);

    as.RewindBuffer();

    as.FCVT_D_H(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4223CFD3);

    as.RewindBuffer();

    as.FCVT_D_H(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4223FFD3);
}

TEST_CASE("FCVT.H.D", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_H_D(f31, f7, RMode::RNE);
    REQUIRE(value == 0x44138FD3);

    as.RewindBuffer();

    as.FCVT_H_D(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4413CFD3);

    as.RewindBuffer();

    as.FCVT_H_D(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4413FFD3);
}

TEST_CASE("FCVT.H.Q", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_H_Q(f31, f7, RMode::RNE);
    REQUIRE(value == 0x44338FD3);

    as.RewindBuffer();

    as.FCVT_H_Q(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4433CFD3);

    as.RewindBuffer();

    as.FCVT_H_Q(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4433FFD3);
}

TEST_CASE("FCVT.H.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_H_S(f31, f7, RMode::RNE);
    REQUIRE(value == 0x44038FD3);

    as.RewindBuffer();

    as.FCVT_H_S(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4403CFD3);

    as.RewindBuffer();

    as.FCVT_H_S(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4403FFD3);
}

TEST_CASE("FCVT.Q.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_Q_H(f31, f7, RMode::RNE);
    REQUIRE(value == 0x46238FD3);

    as.RewindBuffer();

    as.FCVT_Q_H(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4623CFD3);

    as.RewindBuffer();

    as.FCVT_Q_H(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4623FFD3);
}

TEST_CASE("FCVT.S.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_S_H(f31, f7, RMode::RNE);
    REQUIRE(value == 0x40238FD3);

    as.RewindBuffer();

    as.FCVT_S_H(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4023CFD3);

    as.RewindBuffer();

    as.FCVT_S_H(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4023FFD3);
}

TEST_CASE("FCVT.H.W", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_H_W(f31, x7, RMode::RNE);
    REQUIRE(value == 0xD4038FD3);

    as.RewindBuffer();

    as.FCVT_H_W(f31, x7, RMode::RMM);
    REQUIRE(value == 0xD403CFD3);

    as.RewindBuffer();

    as.FCVT_H_W(f31, x7, RMode::DYN);
    REQUIRE(value == 0xD403FFD3);
}

TEST_CASE("FCVT.S.W", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_S_W(f31, x7, RMode::RNE);
    REQUIRE(value == 0xD0038FD3);

    as.RewindBuffer();

    as.FCVT_S_W(f31, x7, RMode::RMM);
    REQUIRE(value == 0xD003CFD3);

    as.RewindBuffer();

    as.FCVT_S_W(f31, x7, RMode::DYN);
    REQUIRE(value == 0xD003FFD3);
}

TEST_CASE("FCVT.H.WU", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_H_WU(f31, x7, RMode::RNE);
    REQUIRE(value == 0xD4138FD3);

    as.RewindBuffer();

    as.FCVT_H_WU(f31, x7, RMode::RMM);
    REQUIRE(value == 0xD413CFD3);

    as.RewindBuffer();

    as.FCVT_H_WU(f31, x7, RMode::DYN);
    REQUIRE(value == 0xD413FFD3);
}

TEST_CASE("FCVT.S.WU", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_S_WU(f31, x7, RMode::RNE);
    REQUIRE(value == 0xD0138FD3);

    as.RewindBuffer();

    as.FCVT_S_WU(f31, x7, RMode::RMM);
    REQUIRE(value == 0xD013CFD3);

    as.RewindBuffer();

    as.FCVT_S_WU(f31, x7, RMode::DYN);
    REQUIRE(value == 0xD013FFD3);
}

TEST_CASE("FCVT.L.H", "[rv64f]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVT_L_H(x31, f7, RMode::RNE);
    REQUIRE(value == 0xC4238FD3);

    as.RewindBuffer();

    as.FCVT_L_H(x31, f7, RMode::RMM);
    REQUIRE(value == 0xC423CFD3);

    as.RewindBuffer();

    as.FCVT_L_H(x31, f7, RMode::DYN);
    REQUIRE(value == 0xC423FFD3);
}

TEST_CASE("FCVT.L.S", "[rv64f]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVT_L_S(x31, f7, RMode::RNE);
    REQUIRE(value == 0xC0238FD3);

    as.RewindBuffer();

    as.FCVT_L_S(x31, f7, RMode::RMM);
    REQUIRE(value == 0xC023CFD3);

    as.RewindBuffer();

    as.FCVT_L_S(x31, f7, RMode::DYN);
    REQUIRE(value == 0xC023FFD3);
}

TEST_CASE("FCVT.LU.H", "[rv64f]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVT_LU_H(x31, f7, RMode::RNE);
    REQUIRE(value == 0xC4338FD3);

    as.RewindBuffer();

    as.FCVT_LU_H(x31, f7, RMode::RMM);
    REQUIRE(value == 0xC433CFD3);

    as.RewindBuffer();

    as.FCVT_LU_H(x31, f7, RMode::DYN);
    REQUIRE(value == 0xC433FFD3);
}

TEST_CASE("FCVT.LU.S", "[rv64f]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVT_LU_S(x31, f7, RMode::RNE);
    REQUIRE(value == 0xC0338FD3);

    as.RewindBuffer();

    as.FCVT_LU_S(x31, f7, RMode::RMM);
    REQUIRE(value == 0xC033CFD3);

    as.RewindBuffer();

    as.FCVT_LU_S(x31, f7, RMode::DYN);
    REQUIRE(value == 0xC033FFD3);
}

TEST_CASE("FCVT.H.L", "[rv64f]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVT_H_L(f31, x7, RMode::RNE);
    REQUIRE(value == 0xD4238FD3);

    as.RewindBuffer();

    as.FCVT_H_L(f31, x7, RMode::RMM);
    REQUIRE(value == 0xD423CFD3);

    as.RewindBuffer();

    as.FCVT_H_L(f31, x7, RMode::DYN);
    REQUIRE(value == 0xD423FFD3);
}

TEST_CASE("FCVT.S.L", "[rv64f]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVT_S_L(f31, x7, RMode::RNE);
    REQUIRE(value == 0xD0238FD3);

    as.RewindBuffer();

    as.FCVT_S_L(f31, x7, RMode::RMM);
    REQUIRE(value == 0xD023CFD3);

    as.RewindBuffer();

    as.FCVT_S_L(f31, x7, RMode::DYN);
    REQUIRE(value == 0xD023FFD3);
}

TEST_CASE("FCVT.H.LU", "[rv64f]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVT_H_LU(f31, x7, RMode::RNE);
    REQUIRE(value == 0xD4338FD3);

    as.RewindBuffer();

    as.FCVT_H_LU(f31, x7, RMode::RMM);
    REQUIRE(value == 0xD433CFD3);

    as.RewindBuffer();

    as.FCVT_H_LU(f31, x7, RMode::DYN);
    REQUIRE(value == 0xD433FFD3);
}

TEST_CASE("FCVT.S.LU", "[rv64f]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVT_S_LU(f31, x7, RMode::RNE);
    REQUIRE(value == 0xD0338FD3);

    as.RewindBuffer();

    as.FCVT_S_LU(f31, x7, RMode::RMM);
    REQUIRE(value == 0xD033CFD3);

    as.RewindBuffer();

    as.FCVT_S_LU(f31, x7, RMode::DYN);
    REQUIRE(value == 0xD033FFD3);
}

TEST_CASE("FCVT.W.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_W_H(x31, f7, RMode::RNE);
    REQUIRE(value == 0xC4038FD3);

    as.RewindBuffer();

    as.FCVT_W_H(x31, f7, RMode::RMM);
    REQUIRE(value == 0xC403CFD3);

    as.RewindBuffer();

    as.FCVT_W_H(x31, f7, RMode::DYN);
    REQUIRE(value == 0xC403FFD3);
}

TEST_CASE("FCVT.W.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_W_S(x31, f7, RMode::RNE);
    REQUIRE(value == 0xC0038FD3);

    as.RewindBuffer();

    as.FCVT_W_S(x31, f7, RMode::RMM);
    REQUIRE(value == 0xC003CFD3);

    as.RewindBuffer();

    as.FCVT_W_S(x31, f7, RMode::DYN);
    REQUIRE(value == 0xC003FFD3);
}

TEST_CASE("FCVT.WU.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_WU_H(x31, f7, RMode::RNE);
    REQUIRE(value == 0xC4138FD3);

    as.RewindBuffer();

    as.FCVT_WU_H(x31, f7, RMode::RMM);
    REQUIRE(value == 0xC413CFD3);

    as.RewindBuffer();

    as.FCVT_WU_H(x31, f7, RMode::DYN);
    REQUIRE(value == 0xC413FFD3);
}

TEST_CASE("FCVT.WU.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_WU_S(x31, f7, RMode::RNE);
    REQUIRE(value == 0xC0138FD3);

    as.RewindBuffer();

    as.FCVT_WU_S(x31, f7, RMode::RMM);
    REQUIRE(value == 0xC013CFD3);

    as.RewindBuffer();

    as.FCVT_WU_S(x31, f7, RMode::DYN);
    REQUIRE(value == 0xC013FFD3);
}

TEST_CASE("FDIV.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FDIV_H(f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0x1DA38FD3);

    as.RewindBuffer();

    as.FDIV_H(f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0x1DA3CFD3);

    as.RewindBuffer();

    as.FDIV_H(f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0x1DA3FFD3);
}

TEST_CASE("FDIV.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FDIV_S(f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0x19A38FD3);

    as.RewindBuffer();

    as.FDIV_S(f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0x19A3CFD3);

    as.RewindBuffer();

    as.FDIV_S(f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0x19A3FFD3);
}

TEST_CASE("FEQ.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FEQ_H(x31, f7, f26);
    REQUIRE(value == 0xA5A3AFD3);

    as.RewindBuffer();

    as.FEQ_H(x31, f26, f7);
    REQUIRE(value == 0xA47D2FD3);
}

TEST_CASE("FEQ.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FEQ_S(x31, f7, f26);
    REQUIRE(value == 0xA1A3AFD3);

    as.RewindBuffer();

    as.FEQ_S(x31, f26, f7);
    REQUIRE(value == 0xA07D2FD3);
}

TEST_CASE("FLE.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FLE_H(x31, f7, f26);
    REQUIRE(value == 0xA5A38FD3);

    as.RewindBuffer();

    as.FLE_H(x31, f26, f7);
    REQUIRE(value == 0xA47D0FD3);
}

TEST_CASE("FLE.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FLE_S(x31, f7, f26);
    REQUIRE(value == 0xA1A38FD3);

    as.RewindBuffer();

    as.FLE_S(x31, f26, f7);
    REQUIRE(value == 0xA07D0FD3);
}

TEST_CASE("FLH", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FLH(f15, 1024, x31);
    REQUIRE(value == 0x400F9787);

    as.RewindBuffer();

    as.FLH(f15, 1536, x31);
    REQUIRE(value == 0x600F9787);

    as.RewindBuffer();

    as.FLH(f15, -1, x31);
    REQUIRE(value == 0xFFFF9787);
}

TEST_CASE("FLT.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FLT_H(x31, f7, f26);
    REQUIRE(value == 0xA5A39FD3);

    as.RewindBuffer();

    as.FLT_H(x31, f26, f7);
    REQUIRE(value == 0xA47D1FD3);
}

TEST_CASE("FLT.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FLT_S(x31, f7, f26);
    REQUIRE(value == 0xA1A39FD3);

    as.RewindBuffer();

    as.FLT_S(x31, f26, f7);
    REQUIRE(value == 0xA07D1FD3);
}

TEST_CASE("FLW", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FLW(f15, 1024, x31);
    REQUIRE(value == 0x400FA787);

    as.RewindBuffer();

    as.FLW(f15, 1536, x31);
    REQUIRE(value == 0x600FA787);

    as.RewindBuffer();

    as.FLW(f15, -1, x31);
    REQUIRE(value == 0xFFFFA787);
}

TEST_CASE("FMADD.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMADD_H(f15, f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0xD47F87C3);

    as.RewindBuffer();

    as.FMADD_H(f15, f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0xD47FC7C3);

    as.RewindBuffer();

    as.FMADD_H(f15, f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0xD47FF7C3);
}

TEST_CASE("FMADD.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMADD_S(f15, f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0xD07F87C3);

    as.RewindBuffer();

    as.FMADD_S(f15, f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0xD07FC7C3);

    as.RewindBuffer();

    as.FMADD_S(f15, f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0xD07FF7C3);
}

TEST_CASE("FMAX.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMAX_H(f31, f7, f26);
    REQUIRE(value == 0x2DA39FD3);

    as.RewindBuffer();

    as.FMAX_H(f31, f31, f31);
    REQUIRE(value == 0x2DFF9FD3);
}

TEST_CASE("FMAX.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMAX_S(f31, f7, f26);
    REQUIRE(value == 0x29A39FD3);

    as.RewindBuffer();

    as.FMAX_S(f31, f31, f31);
    REQUIRE(value == 0x29FF9FD3);
}

TEST_CASE("FMIN.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMIN_H(f31, f7, f26);
    REQUIRE(value == 0x2DA38FD3);

    as.RewindBuffer();

    as.FMIN_H(f31, f31, f31);
    REQUIRE(value == 0x2DFF8FD3);
}

TEST_CASE("FMIN.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMIN_S(f31, f7, f26);
    REQUIRE(value == 0x29A38FD3);

    as.RewindBuffer();

    as.FMIN_S(f31, f31, f31);
    REQUIRE(value == 0x29FF8FD3);
}

TEST_CASE("FMSUB.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMSUB_H(f15, f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0xD47F87C7);

    as.RewindBuffer();

    as.FMSUB_H(f15, f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0xD47FC7C7);

    as.RewindBuffer();

    as.FMSUB_H(f15, f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0xD47FF7C7);
}

TEST_CASE("FMSUB.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMSUB_S(f15, f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0xD07F87C7);

    as.RewindBuffer();

    as.FMSUB_S(f15, f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0xD07FC7C7);

    as.RewindBuffer();

    as.FMSUB_S(f15, f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0xD07FF7C7);
}

TEST_CASE("FMUL.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMUL_H(f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0x15A38FD3);

    as.RewindBuffer();

    as.FMUL_H(f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0x15A3CFD3);

    as.RewindBuffer();

    as.FMUL_H(f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0x15A3FFD3);
}

TEST_CASE("FMUL.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMUL_S(f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0x11A38FD3);

    as.RewindBuffer();

    as.FMUL_S(f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0x11A3CFD3);

    as.RewindBuffer();

    as.FMUL_S(f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0x11A3FFD3);
}

TEST_CASE("FMV.H.X", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMV_H_X(f31, x7);
    REQUIRE(value == 0xF4038FD3);

    as.RewindBuffer();

    as.FMV_H_X(f7, x31);
    REQUIRE(value == 0xF40F83D3);
}

TEST_CASE("FMV.W.X", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMV_W_X(f31, x7);
    REQUIRE(value == 0xF0038FD3);

    as.RewindBuffer();

    as.FMV_W_X(f7, x31);
    REQUIRE(value == 0xF00F83D3);
}

TEST_CASE("FMV.X.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMV_X_H(x31, f7);
    REQUIRE(value == 0xE4038FD3);

    as.RewindBuffer();

    as.FMV_X_H(x7, f31);
    REQUIRE(value == 0xE40F83D3);
}

TEST_CASE("FMV.X.W", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMV_X_W(x31, f7);
    REQUIRE(value == 0xE0038FD3);

    as.RewindBuffer();

    as.FMV_X_W(x7, f31);
    REQUIRE(value == 0xE00F83D3);
}

TEST_CASE("FNMADD.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FNMADD_H(f15, f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0xD47F87CF);

    as.RewindBuffer();

    as.FNMADD_H(f15, f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0xD47FC7CF);

    as.RewindBuffer();

    as.FNMADD_H(f15, f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0xD47FF7CF);
}

TEST_CASE("FNMADD.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FNMADD_S(f15, f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0xD07F87CF);

    as.RewindBuffer();

    as.FNMADD_S(f15, f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0xD07FC7CF);

    as.RewindBuffer();

    as.FNMADD_S(f15, f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0xD07FF7CF);
}

TEST_CASE("FNMSUB.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FNMSUB_H(f15, f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0xD47F87CB);

    as.RewindBuffer();

    as.FNMSUB_H(f15, f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0xD47FC7CB);

    as.RewindBuffer();

    as.FNMSUB_H(f15, f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0xD47FF7CB);
}

TEST_CASE("FNMSUB.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FNMSUB_S(f15, f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0xD07F87CB);

    as.RewindBuffer();

    as.FNMSUB_S(f15, f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0xD07FC7CB);

    as.RewindBuffer();

    as.FNMSUB_S(f15, f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0xD07FF7CB);
}

TEST_CASE("FSGNJ.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSGNJ_H(f31, f7, f26);
    REQUIRE(value == 0x25A38FD3);

    as.RewindBuffer();

    as.FSGNJ_H(f31, f31, f31);
    REQUIRE(value == 0x25FF8FD3);
}

TEST_CASE("FSGNJ.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSGNJ_S(f31, f7, f26);
    REQUIRE(value == 0x21A38FD3);

    as.RewindBuffer();

    as.FSGNJ_S(f31, f31, f31);
    REQUIRE(value == 0x21FF8FD3);
}

TEST_CASE("FSGNJN.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSGNJN_H(f31, f7, f26);
    REQUIRE(value == 0x25A39FD3);

    as.RewindBuffer();

    as.FSGNJN_H(f31, f31, f31);
    REQUIRE(value == 0x25FF9FD3);
}

TEST_CASE("FSGNJN.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSGNJN_S(f31, f7, f26);
    REQUIRE(value == 0x21A39FD3);

    as.RewindBuffer();

    as.FSGNJN_S(f31, f31, f31);
    REQUIRE(value == 0x21FF9FD3);
}

TEST_CASE("FSGNJX.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSGNJX_H(f31, f7, f26);
    REQUIRE(value == 0x25A3AFD3);

    as.RewindBuffer();

    as.FSGNJX_H(f31, f31, f31);
    REQUIRE(value == 0x25FFAFD3);
}

TEST_CASE("FSGNJX.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSGNJX_S(f31, f7, f26);
    REQUIRE(value == 0x21A3AFD3);

    as.RewindBuffer();

    as.FSGNJX_S(f31, f31, f31);
    REQUIRE(value == 0x21FFAFD3);
}

TEST_CASE("FSH", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSH(f31, 1024, x15);
    REQUIRE(value == 0x41F79027);

    as.RewindBuffer();

    as.FSH(f31, 1536, x15);
    REQUIRE(value == 0x61F79027);

    as.RewindBuffer();

    as.FSH(f31, -1, x15);
    REQUIRE(value == 0xFFF79FA7);
}

TEST_CASE("FSQRT.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSQRT_H(f31, f7, RMode::RNE);
    REQUIRE(value == 0x5C038FD3);

    as.RewindBuffer();

    as.FSQRT_H(f31, f7, RMode::RMM);
    REQUIRE(value == 0x5C03CFD3);

    as.RewindBuffer();

    as.FSQRT_H(f31, f7, RMode::DYN);
    REQUIRE(value == 0x5C03FFD3);
}

TEST_CASE("FSQRT.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSQRT_S(f31, f7, RMode::RNE);
    REQUIRE(value == 0x58038FD3);

    as.RewindBuffer();

    as.FSQRT_S(f31, f7, RMode::RMM);
    REQUIRE(value == 0x5803CFD3);

    as.RewindBuffer();

    as.FSQRT_S(f31, f7, RMode::DYN);
    REQUIRE(value == 0x5803FFD3);
}

TEST_CASE("FSUB.H", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSUB_H(f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0x0DA38FD3);

    as.RewindBuffer();

    as.FSUB_H(f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0x0DA3CFD3);

    as.RewindBuffer();

    as.FSUB_H(f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0x0DA3FFD3);
}

TEST_CASE("FSUB.S", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSUB_S(f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0x09A38FD3);

    as.RewindBuffer();

    as.FSUB_S(f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0x09A3CFD3);

    as.RewindBuffer();

    as.FSUB_S(f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0x09A3FFD3);
}

TEST_CASE("FSW", "[rv32f]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSW(f31, 1024, x15);
    REQUIRE(value == 0x41F7A027);

    as.RewindBuffer();

    as.FSW(f31, 1536, x15);
    REQUIRE(value == 0x61F7A027);

    as.RewindBuffer();

    as.FSW(f31, -1, x15);
    REQUIRE(value == 0xFFF7AFA7);
}
