#include <catch/catch.hpp>

#include <array>
#include <cstring>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

static constexpr std::array fli_constants{
    0xBFF0000000000000ULL,  // -1.0
    0x0010000000000000ULL,  // Minimum positive normal
    0x3EF0000000000000ULL,  // 1.0 * 2^-16
    0x3F00000000000000ULL,  // 1.0 * 2^-15
    0x3F70000000000000ULL,  // 1.0 * 2^-8
    0x3F80000000000000ULL,  // 1.0 * 2^-7
    0x3FB0000000000000ULL,  // 1.0 * 2^-4
    0x3FC0000000000000ULL,  // 1.0 * 2^-3
    0x3FD0000000000000ULL,  // 0.25
    0x3FD4000000000000ULL,  // 0.3125
    0x3FD8000000000000ULL,  // 0.375
    0x3FDC000000000000ULL,  // 0.4375
    0x3FE0000000000000ULL,  // 0.5
    0x3FE4000000000000ULL,  // 0.625
    0x3FE8000000000000ULL,  // 0.75
    0x3FEC000000000000ULL,  // 0.875
    0x3FF0000000000000ULL,  // 1.0
    0x3FF4000000000000ULL,  // 1.25
    0x3FF8000000000000ULL,  // 1.5
    0x3FFC000000000000ULL,  // 1.75
    0x4000000000000000ULL,  // 2.0
    0x4004000000000000ULL,  // 2.5
    0x4008000000000000ULL,  // 3
    0x4010000000000000ULL,  // 4
    0x4020000000000000ULL,  // 8
    0x4030000000000000ULL,  // 16
    0x4060000000000000ULL,  // 2^7
    0x4070000000000000ULL,  // 2^8
    0x40E0000000000000ULL,  // 2^15
    0x40F0000000000000ULL,  // 2^16
    0x7FF0000000000000ULL,  // +inf
    0x7FF8000000000000ULL,  // Canonical NaN
};

TEST_CASE("FLI.D", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (size_t i = 0; i < fli_constants.size(); i++) {
        const auto constant = fli_constants[i];
        double fconstant{};
        std::memcpy(&fconstant, &constant, sizeof(fconstant));

        as.FLI_D(f10, fconstant);

        const auto op_base = 0xF2100553;
        const auto verify = op_base | (i << 15);
        REQUIRE(value == verify);

        as.RewindBuffer();
    }
}

TEST_CASE("FLI.H", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (size_t i = 0; i < fli_constants.size(); i++) {
        const auto constant = fli_constants[i];
        double fconstant{};
        std::memcpy(&fconstant, &constant, sizeof(fconstant));

        as.FLI_H(f10, fconstant);

        const auto op_base = 0xF4100553;
        const auto verify = op_base | (i << 15);
        REQUIRE(value == verify);

        as.RewindBuffer();
    }
}

TEST_CASE("FLI.S", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (size_t i = 0; i < fli_constants.size(); i++) {
        const auto constant = fli_constants[i];
        double fconstant{};
        std::memcpy(&fconstant, &constant, sizeof(fconstant));

        as.FLI_S(f10, fconstant);

        const auto op_base = 0xF0100553;
        const auto verify = op_base | (i << 15);
        REQUIRE(value == verify);

        as.RewindBuffer();
    }
}

TEST_CASE("FMINM.D", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

     as.FMINM_D(f20, f12, f10);
     REQUIRE(value == 0x2AA62A53);
}

TEST_CASE("FMINM.H", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

     as.FMINM_H(f20, f12, f10);
     REQUIRE(value == 0x2CA62A53);
}

TEST_CASE("FMINM.Q", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

     as.FMINM_Q(f20, f12, f10);
     REQUIRE(value == 0x2EA62A53);
}

TEST_CASE("FMINM.S", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

     as.FMINM_S(f20, f12, f10);
     REQUIRE(value == 0x28A62A53);
}

TEST_CASE("FMAXM.D", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

     as.FMAXM_D(f20, f12, f10);
     REQUIRE(value == 0x2AA63A53);
}

TEST_CASE("FMAXM.H", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

     as.FMAXM_H(f20, f12, f10);
     REQUIRE(value == 0x2CA63A53);
}

TEST_CASE("FMAXM.Q", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

     as.FMAXM_Q(f20, f12, f10);
     REQUIRE(value == 0x2EA63A53);
}

TEST_CASE("FMAXM.S", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

     as.FMAXM_S(f20, f12, f10);
     REQUIRE(value == 0x28A63A53);
}

TEST_CASE("FROUND.D", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FROUND_D(f31, f7, RMode::RNE);
    REQUIRE(value == 0x42438FD3);

    as.RewindBuffer();

    as.FROUND_D(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4243CFD3);

    as.RewindBuffer();

    as.FROUND_D(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4243FFD3);
}

TEST_CASE("FROUND.H", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FROUND_H(f31, f7, RMode::RNE);
    REQUIRE(value == 0x44438FD3);

    as.RewindBuffer();

    as.FROUND_H(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4443CFD3);

    as.RewindBuffer();

    as.FROUND_H(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4443FFD3);
}

TEST_CASE("FROUND.Q", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FROUND_Q(f31, f7, RMode::RNE);
    REQUIRE(value == 0x46438FD3);

    as.RewindBuffer();

    as.FROUND_Q(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4643CFD3);

    as.RewindBuffer();

    as.FROUND_Q(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4643FFD3);
}

TEST_CASE("FROUND.S", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FROUND_S(f31, f7, RMode::RNE);
    REQUIRE(value == 0x40438FD3);

    as.RewindBuffer();

    as.FROUND_S(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4043CFD3);

    as.RewindBuffer();

    as.FROUND_S(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4043FFD3);
}

TEST_CASE("FROUNDNX.D", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FROUNDNX_D(f31, f7, RMode::RNE);
    REQUIRE(value == 0x42538FD3);

    as.RewindBuffer();

    as.FROUNDNX_D(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4253CFD3);

    as.RewindBuffer();

    as.FROUNDNX_D(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4253FFD3);
}

TEST_CASE("FROUNDNX.H", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FROUNDNX_H(f31, f7, RMode::RNE);
    REQUIRE(value == 0x44538FD3);

    as.RewindBuffer();

    as.FROUNDNX_H(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4453CFD3);

    as.RewindBuffer();

    as.FROUNDNX_H(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4453FFD3);
}

TEST_CASE("FROUNDNX.Q", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FROUNDNX_Q(f31, f7, RMode::RNE);
    REQUIRE(value == 0x46538FD3);

    as.RewindBuffer();

    as.FROUNDNX_Q(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4653CFD3);

    as.RewindBuffer();

    as.FROUNDNX_Q(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4653FFD3);
}

TEST_CASE("FROUNDNX.S", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FROUNDNX_S(f31, f7, RMode::RNE);
    REQUIRE(value == 0x40538FD3);

    as.RewindBuffer();

    as.FROUNDNX_S(f31, f7, RMode::RMM);
    REQUIRE(value == 0x4053CFD3);

    as.RewindBuffer();

    as.FROUNDNX_S(f31, f7, RMode::DYN);
    REQUIRE(value == 0x4053FFD3);
}

TEST_CASE("FCVTMOD.W.D", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FCVTMOD_W_D(x31, f7);
    REQUIRE(value == 0xC2839FD3);
}

TEST_CASE("FMVH.X.D", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMVH_X_D(x31, f7);
    REQUIRE(value == 0xE2138FD3);
}

TEST_CASE("FMVH.X.Q", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FMVH_X_Q(x31, f7);
    REQUIRE(value == 0xE6138FD3);
}

TEST_CASE("FMVP.D.X", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.FMVP_D_X(f31, x7, x8);
    REQUIRE(value == 0xB2838FD3);
}

TEST_CASE("FMVP.Q.X", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FMVP_Q_X(f31, x7, x8);
    REQUIRE(value == 0xB6838FD3);
}

TEST_CASE("FLEQ.D", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FLEQ_D(x31, f7, f15);
    REQUIRE(value == 0xA2F3CFD3);
}

TEST_CASE("FLTQ.D", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FLTQ_D(x31, f7, f15);
    REQUIRE(value == 0xA2F3DFD3);
}

TEST_CASE("FLEQ.H", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FLEQ_H(x31, f7, f15);
    REQUIRE(value == 0xA4F3CFD3);
}

TEST_CASE("FLTQ.H", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FLTQ_H(x31, f7, f15);
    REQUIRE(value == 0xA4F3DFD3);
}

TEST_CASE("FLEQ.Q", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FLEQ_Q(x31, f7, f15);
    REQUIRE(value == 0xA6F3CFD3);
}

TEST_CASE("FLTQ.Q", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FLTQ_Q(x31, f7, f15);
    REQUIRE(value == 0xA6F3DFD3);
}

TEST_CASE("FLEQ.S", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FLEQ_S(x31, f7, f15);
    REQUIRE(value == 0xA0F3CFD3);
}

TEST_CASE("FLTQ.S", "[Zfa]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.FLTQ_S(x31, f7, f15);
    REQUIRE(value == 0xA0F3DFD3);
}
