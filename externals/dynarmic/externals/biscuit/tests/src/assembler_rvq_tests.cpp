#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("FADD.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FADD_Q(f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0x07A38FD3);

    as.RewindBuffer();

    as.FADD_Q(f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0x07A3CFD3);

    as.RewindBuffer();

    as.FADD_Q(f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0x07A3FFD3);
}

TEST_CASE("FCLASS.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCLASS_Q(x31, f7);
    REQUIRE(value == 0xE6039FD3);

    as.RewindBuffer();

    as.FCLASS_Q(x7, f31);
    REQUIRE(value == 0xE60F93D3);
}

TEST_CASE("FCVT.Q.D", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_Q_D(f31, f7, RMode::RNE);
    REQUIRE(value == 0x46138FD3);

    as.RewindBuffer();

    as.FCVT_Q_D(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4613CFD3);

    as.RewindBuffer();

    as.FCVT_Q_D(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4613FFD3);
}

TEST_CASE("FCVT.Q.S", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_Q_S(f31, f7, RMode::RNE);
    REQUIRE(value == 0x46038FD3);

    as.RewindBuffer();

    as.FCVT_Q_S(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4603CFD3);

    as.RewindBuffer();

    as.FCVT_Q_S(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4603FFD3);
}

TEST_CASE("FCVT.Q.W", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_Q_W(f31, x7, RMode::RNE);
    REQUIRE(value == 0xD6038FD3);

    as.RewindBuffer();

    as.FCVT_Q_W(f31, x7, RMode::RMM);
    REQUIRE(value == 0xD603CFD3);

    as.RewindBuffer();

    as.FCVT_Q_W(f31, x7, RMode::DYN);
    REQUIRE(value == 0xD603FFD3);
}

TEST_CASE("FCVT.Q.WU", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_Q_WU(f31, x7, RMode::RNE);
    REQUIRE(value == 0xD6138FD3);

    as.RewindBuffer();

    as.FCVT_Q_WU(f31, x7, RMode::RMM);
    REQUIRE(value == 0xD613CFD3);

    as.RewindBuffer();

    as.FCVT_Q_WU(f31, x7, RMode::DYN);
    REQUIRE(value == 0xD613FFD3);
}

TEST_CASE("FCVT.L.Q", "[rv64q]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVT_L_Q(x31, f7, RMode::RNE);
    REQUIRE(value == 0xC6238FD3);

    as.RewindBuffer();

    as.FCVT_L_Q(x31, f7, RMode::RMM);
    REQUIRE(value == 0xC623CFD3);

    as.RewindBuffer();

    as.FCVT_L_Q(x31, f7, RMode::DYN);
    REQUIRE(value == 0xC623FFD3);
}

TEST_CASE("FCVT.LU.Q", "[rv64q]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVT_LU_Q(x31, f7, RMode::RNE);
    REQUIRE(value == 0xC6338FD3);

    as.RewindBuffer();

    as.FCVT_LU_Q(x31, f7, RMode::RMM);
    REQUIRE(value == 0xC633CFD3);

    as.RewindBuffer();

    as.FCVT_LU_Q(x31, f7, RMode::DYN);
    REQUIRE(value == 0xC633FFD3);
}

TEST_CASE("FCVT.Q.L", "[rv64q]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVT_Q_L(f31, x7, RMode::RNE);
    REQUIRE(value == 0xD6238FD3);

    as.RewindBuffer();

    as.FCVT_Q_L(f31, x7, RMode::RMM);
    REQUIRE(value == 0xD623CFD3);

    as.RewindBuffer();

    as.FCVT_Q_L(f31, x7, RMode::DYN);
    REQUIRE(value == 0xD623FFD3);
}

TEST_CASE("FCVT.Q.LU", "[rv64q]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVT_Q_LU(f31, x7, RMode::RNE);
    REQUIRE(value == 0xD6338FD3);

    as.RewindBuffer();

    as.FCVT_Q_LU(f31, x7, RMode::RMM);
    REQUIRE(value == 0xD633CFD3);

    as.RewindBuffer();

    as.FCVT_Q_LU(f31, x7, RMode::DYN);
    REQUIRE(value == 0xD633FFD3);
}

TEST_CASE("FCVT.W.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_W_Q(x31, f7, RMode::RNE);
    REQUIRE(value == 0xC6038FD3);

    as.RewindBuffer();

    as.FCVT_W_Q(x31, f7, RMode::RMM);
    REQUIRE(value == 0xC603CFD3);

    as.RewindBuffer();

    as.FCVT_W_Q(x31, f7, RMode::DYN);
    REQUIRE(value == 0xC603FFD3);
}

TEST_CASE("FCVT.WU.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_WU_Q(x31, f7, RMode::RNE);
    REQUIRE(value == 0xC6138FD3);

    as.RewindBuffer();

    as.FCVT_WU_Q(x31, f7, RMode::RMM);
    REQUIRE(value == 0xC613CFD3);

    as.RewindBuffer();

    as.FCVT_WU_Q(x31, f7, RMode::DYN);
    REQUIRE(value == 0xC613FFD3);
}

TEST_CASE("FCVT.D.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_D_Q(f31, f7, RMode::RNE);
    REQUIRE(value == 0x42338FD3);

    as.RewindBuffer();

    as.FCVT_D_Q(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4233CFD3);

    as.RewindBuffer();

    as.FCVT_D_Q(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4233FFD3);
}

TEST_CASE("FCVT.S.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FCVT_S_Q(f31, f7, RMode::RNE);
    REQUIRE(value == 0x40338FD3);

    as.RewindBuffer();

    as.FCVT_S_Q(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4033CFD3);

    as.RewindBuffer();

    as.FCVT_S_Q(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4033FFD3);
}

TEST_CASE("FDIV.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FDIV_Q(f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0x1FA38FD3);

    as.RewindBuffer();

    as.FDIV_Q(f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0x1FA3CFD3);

    as.RewindBuffer();

    as.FDIV_Q(f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0x1FA3FFD3);
}

TEST_CASE("FEQ.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FEQ_Q(x31, f7, f26);
    REQUIRE(value == 0xA7A3AFD3);

    as.RewindBuffer();

    as.FEQ_Q(x31, f26, f7);
    REQUIRE(value == 0xA67D2FD3);
}

TEST_CASE("FLE.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FLE_Q(x31, f7, f26);
    REQUIRE(value == 0xA7A38FD3);

    as.RewindBuffer();

    as.FLE_Q(x31, f26, f7);
    REQUIRE(value == 0xA67D0FD3);
}

TEST_CASE("FLT.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FLT_Q(x31, f7, f26);
    REQUIRE(value == 0xA7A39FD3);

    as.RewindBuffer();

    as.FLT_Q(x31, f26, f7);
    REQUIRE(value == 0xA67D1FD3);
}

TEST_CASE("FLQ", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FLQ(f15, 1024, x31);
    REQUIRE(value == 0x400FC787);

    as.RewindBuffer();

    as.FLQ(f15, 1536, x31);
    REQUIRE(value == 0x600FC787);

    as.RewindBuffer();

    as.FLQ(f15, -1, x31);
    REQUIRE(value == 0xFFFFC787);
}

TEST_CASE("FMADD.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMADD_Q(f15, f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0xD67F87C3);

    as.RewindBuffer();

    as.FMADD_Q(f15, f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0xD67FC7C3);

    as.RewindBuffer();

    as.FMADD_Q(f15, f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0xD67FF7C3);
}

TEST_CASE("FMAX.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMAX_Q(f31, f7, f26);
    REQUIRE(value == 0x2FA39FD3);

    as.RewindBuffer();

    as.FMAX_Q(f31, f31, f31);
    REQUIRE(value == 0x2FFF9FD3);
}

TEST_CASE("FMIN.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMIN_Q(f31, f7, f26);
    REQUIRE(value == 0x2FA38FD3);

    as.RewindBuffer();

    as.FMIN_Q(f31, f31, f31);
    REQUIRE(value == 0x2FFF8FD3);
}

TEST_CASE("FMSUB.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMSUB_Q(f15, f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0xD67F87C7);

    as.RewindBuffer();

    as.FMSUB_Q(f15, f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0xD67FC7C7);

    as.RewindBuffer();

    as.FMSUB_Q(f15, f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0xD67FF7C7);
}

TEST_CASE("FMUL.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMUL_Q(f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0x17A38FD3);

    as.RewindBuffer();

    as.FMUL_Q(f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0x17A3CFD3);

    as.RewindBuffer();

    as.FMUL_Q(f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0x17A3FFD3);
}

TEST_CASE("FNMADD.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FNMADD_Q(f15, f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0xD67F87CF);

    as.RewindBuffer();

    as.FNMADD_Q(f15, f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0xD67FC7CF);

    as.RewindBuffer();

    as.FNMADD_Q(f15, f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0xD67FF7CF);
}

TEST_CASE("FNMSUB.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FNMSUB_Q(f15, f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0xD67F87CB);

    as.RewindBuffer();

    as.FNMSUB_Q(f15, f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0xD67FC7CB);

    as.RewindBuffer();

    as.FNMSUB_Q(f15, f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0xD67FF7CB);
}

TEST_CASE("FSGNJ.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSGNJ_Q(f31, f7, f26);
    REQUIRE(value == 0x27A38FD3);

    as.RewindBuffer();

    as.FSGNJ_Q(f31, f31, f31);
    REQUIRE(value == 0x27FF8FD3);
}

TEST_CASE("FSGNJN.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSGNJN_Q(f31, f7, f26);
    REQUIRE(value == 0x27A39FD3);

    as.RewindBuffer();

    as.FSGNJN_Q(f31, f31, f31);
    REQUIRE(value == 0x27FF9FD3);
}

TEST_CASE("FSGNJX.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSGNJX_Q(f31, f7, f26);
    REQUIRE(value == 0x27A3AFD3);

    as.RewindBuffer();

    as.FSGNJX_Q(f31, f31, f31);
    REQUIRE(value == 0x27FFAFD3);
}

TEST_CASE("FSQRT.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSQRT_Q(f31, f7, RMode::RNE);
    REQUIRE(value == 0x5E038FD3);

    as.RewindBuffer();

    as.FSQRT_Q(f31, f7, RMode::RMM);
    REQUIRE(value == 0x5E03CFD3);

    as.RewindBuffer();

    as.FSQRT_Q(f31, f7, RMode::DYN);
    REQUIRE(value == 0x5E03FFD3);
}

TEST_CASE("FSUB.Q", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSUB_Q(f31, f7, f26, RMode::RNE);
    REQUIRE(value == 0x0FA38FD3);

    as.RewindBuffer();

    as.FSUB_Q(f31, f7, f26, RMode::RMM);
    REQUIRE(value == 0x0FA3CFD3);

    as.RewindBuffer();

    as.FSUB_Q(f31, f7, f26, RMode::DYN);
    REQUIRE(value == 0x0FA3FFD3);
}

TEST_CASE("FSQ", "[rv32q]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FSQ(f31, 1024, x15);
    REQUIRE(value == 0x41F7C027);

    as.RewindBuffer();

    as.FSQ(f31, 1536, x15);
    REQUIRE(value == 0x61F7C027);

    as.RewindBuffer();

    as.FSQ(f31, -1, x15);
    REQUIRE(value == 0xFFF7CFA7);
}
