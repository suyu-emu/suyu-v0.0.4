#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("AES32DSI", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AES32DSI(x31, x31, x31, 0b11);
    REQUIRE(value == 0xEBFF8FB3);

    as.RewindBuffer();

    as.AES32DSI(x1, x2, x3, 0b10);
    REQUIRE(value == 0xAA3100B3);
}

TEST_CASE("AES32DSMI", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AES32DSMI(x31, x31, x31, 0b11);
    REQUIRE(value == 0xEFFF8FB3);

    as.RewindBuffer();

    as.AES32DSMI(x1, x2, x3, 0b10);
    REQUIRE(value == 0xAE3100B3);
}

TEST_CASE("AES32ESI", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AES32ESI(x31, x31, x31, 0b11);
    REQUIRE(value == 0xE3FF8FB3);

    as.RewindBuffer();

    as.AES32ESI(x1, x2, x3, 0b10);
    REQUIRE(value == 0xA23100B3);
}

TEST_CASE("AES32ESMI", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AES32ESMI(x31, x31, x31, 0b11);
    REQUIRE(value == 0xE7FF8FB3);

    as.RewindBuffer();

    as.AES32ESMI(x1, x2, x3, 0b10);
    REQUIRE(value == 0xA63100B3);
}

TEST_CASE("AES64DS", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AES64DS(x31, x31, x31);
    REQUIRE(value == 0x3BFF8FB3);

    as.RewindBuffer();

    as.AES64DS(x1, x2, x3);
    REQUIRE(value == 0x3A3100B3);
}

TEST_CASE("AES64DSM", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AES64DSM(x31, x31, x31);
    REQUIRE(value == 0x3FFF8FB3);

    as.RewindBuffer();

    as.AES64DSM(x1, x2, x3);
    REQUIRE(value == 0x3E3100B3);
}

TEST_CASE("AES64ES", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AES64ES(x31, x31, x31);
    REQUIRE(value == 0x33FF8FB3);

    as.RewindBuffer();

    as.AES64ES(x1, x2, x3);
    REQUIRE(value == 0x323100B3);
}

TEST_CASE("AES64ESM", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AES64ESM(x31, x31, x31);
    REQUIRE(value == 0x37FF8FB3);

    as.RewindBuffer();

    as.AES64ESM(x1, x2, x3);
    REQUIRE(value == 0x363100B3);
}

TEST_CASE("AES64IM", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AES64IM(x31, x31);
    REQUIRE(value == 0x300F9F93);

    as.RewindBuffer();

    as.AES64IM(x1, x2);
    REQUIRE(value == 0x30011093);
}

TEST_CASE("AES64KS1I", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AES64KS1I(x31, x31, 0xA);
    REQUIRE(value == 0x31AF9F93);

    as.RewindBuffer();

    as.AES64KS1I(x1, x2, 0x5);
    REQUIRE(value == 0x31511093);
}

TEST_CASE("AES64KS2", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AES64KS2(x31, x31, x31);
    REQUIRE(value == 0x7FFF8FB3);

    as.RewindBuffer();

    as.AES64KS2(x1, x2, x3);
    REQUIRE(value == 0x7E3100B3);
}

TEST_CASE("SHA256SIG0", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SHA256SIG0(x31, x31);
    REQUIRE(value == 0x102F9F93);

    as.RewindBuffer();

    as.SHA256SIG0(x1, x2);
    REQUIRE(value == 0x10211093);
}

TEST_CASE("SHA256SIG1", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SHA256SIG1(x31, x31);
    REQUIRE(value == 0x103F9F93);

    as.RewindBuffer();

    as.SHA256SIG1(x1, x2);
    REQUIRE(value == 0x10311093);
}

TEST_CASE("SHA256SUM0", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SHA256SUM0(x31, x31);
    REQUIRE(value == 0x100F9F93);

    as.RewindBuffer();

    as.SHA256SUM0(x1, x2);
    REQUIRE(value == 0x10011093);
}

TEST_CASE("SHA256SUM1", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SHA256SUM1(x31, x31);
    REQUIRE(value == 0x101F9F93);

    as.RewindBuffer();

    as.SHA256SUM1(x1, x2);
    REQUIRE(value == 0x10111093);
}

TEST_CASE("SHA512SIG0", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SHA512SIG0(x31, x31);
    REQUIRE(value == 0x106F9F93);

    as.RewindBuffer();

    as.SHA512SIG0(x1, x2);
    REQUIRE(value == 0x10611093);
}

TEST_CASE("SHA512SIG0H", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SHA512SIG0H(x31, x31, x31);
    REQUIRE(value == 0x5DFF8FB3);

    as.RewindBuffer();

    as.SHA512SIG0H(x1, x2, x3);
    REQUIRE(value == 0x5C3100B3);
}

TEST_CASE("SHA512SIG0L", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SHA512SIG0L(x31, x31, x31);
    REQUIRE(value == 0x55FF8FB3);

    as.RewindBuffer();

    as.SHA512SIG0L(x1, x2, x3);
    REQUIRE(value == 0x543100B3);
}

TEST_CASE("SHA512SIG1", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SHA512SIG1(x31, x31);
    REQUIRE(value == 0x107F9F93);

    as.RewindBuffer();

    as.SHA512SIG1(x1, x2);
    REQUIRE(value == 0x10711093);
}

TEST_CASE("SHA512SIG1H", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SHA512SIG1H(x31, x31, x31);
    REQUIRE(value == 0x5FFF8FB3);

    as.RewindBuffer();

    as.SHA512SIG1H(x1, x2, x3);
    REQUIRE(value == 0x5E3100B3);
}

TEST_CASE("SHA512SIG1L", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SHA512SIG1L(x31, x31, x31);
    REQUIRE(value == 0x57FF8FB3);

    as.RewindBuffer();

    as.SHA512SIG1L(x1, x2, x3);
    REQUIRE(value == 0x563100B3);
}

TEST_CASE("SHA512SUM0", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SHA512SUM0(x31, x31);
    REQUIRE(value == 0x104F9F93);

    as.RewindBuffer();

    as.SHA512SUM0(x1, x2);
    REQUIRE(value == 0x10411093);
}

TEST_CASE("SHA512SUM0R", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SHA512SUM0R(x31, x31, x31);
    REQUIRE(value == 0x51FF8FB3);

    as.RewindBuffer();

    as.SHA512SUM0R(x1, x2, x3);
    REQUIRE(value == 0x503100B3);
}

TEST_CASE("SHA512SUM1", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SHA512SUM1(x31, x31);
    REQUIRE(value == 0x105F9F93);

    as.RewindBuffer();

    as.SHA512SUM1(x1, x2);
    REQUIRE(value == 0x10511093);
}

TEST_CASE("SHA512SUM1R", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SHA512SUM1R(x31, x31, x31);
    REQUIRE(value == 0x53FF8FB3);

    as.RewindBuffer();

    as.SHA512SUM1R(x1, x2, x3);
    REQUIRE(value == 0x523100B3);
}

TEST_CASE("SM3P0", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SM3P0(x31, x31);
    REQUIRE(value == 0x108F9F93);

    as.RewindBuffer();

    as.SM3P0(x1, x2);
    REQUIRE(value == 0x10811093);
}

TEST_CASE("SM3P1", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SM3P1(x31, x31);
    REQUIRE(value == 0x109F9F93);

    as.RewindBuffer();

    as.SM3P1(x1, x2);
    REQUIRE(value == 0x10911093);
}

TEST_CASE("SM4ED", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SM4ED(x31, x31, x31, 0b11);
    REQUIRE(value == 0xF1FF8FB3);

    as.RewindBuffer();

    as.SM4ED(x1, x2, x3, 0b10);
    REQUIRE(value == 0xB03100B3);
}

TEST_CASE("SM4KS", "[rvk]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SM4KS(x31, x31, x31, 0b11);
    REQUIRE(value == 0xF5FF8FB3);

    as.RewindBuffer();

    as.SM4KS(x1, x2, x3, 0b10);
    REQUIRE(value == 0xB43100B3);
}
