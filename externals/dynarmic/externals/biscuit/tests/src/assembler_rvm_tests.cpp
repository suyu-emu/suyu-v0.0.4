#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("DIV", "[rv32m]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.DIV(x31, x15, x20);
    REQUIRE(value == 0x0347CFB3);

    as.RewindBuffer();

    as.DIV(x31, x20, x15);
    REQUIRE(value == 0x02FA4FB3);

    as.RewindBuffer();

    as.DIV(x20, x31, x15);
    REQUIRE(value == 0x02FFCA33);
}

TEST_CASE("DIVW", "[rv64m]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.DIVW(x31, x15, x20);
    REQUIRE(value == 0x0347CFBB);

    as.RewindBuffer();

    as.DIVW(x31, x20, x15);
    REQUIRE(value == 0x02FA4FBB);

    as.RewindBuffer();

    as.DIVW(x20, x31, x15);
    REQUIRE(value == 0x02FFCA3B);
}

TEST_CASE("DIVU", "[rv32m]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.DIVU(x31, x15, x20);
    REQUIRE(value == 0x0347DFB3);

    as.RewindBuffer();

    as.DIVU(x31, x20, x15);
    REQUIRE(value == 0x02FA5FB3);

    as.RewindBuffer();

    as.DIVU(x20, x31, x15);
    REQUIRE(value == 0x02FFDA33);
}

TEST_CASE("DIVUW", "[rv64m]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.DIVUW(x31, x15, x20);
    REQUIRE(value == 0x0347DFBB);

    as.RewindBuffer();

    as.DIVUW(x31, x20, x15);
    REQUIRE(value == 0x02FA5FBB);

    as.RewindBuffer();

    as.DIVUW(x20, x31, x15);
    REQUIRE(value == 0x02FFDA3B);
}

TEST_CASE("MUL", "[rv32m]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.MUL(x31, x15, x20);
    REQUIRE(value == 0x03478FB3);

    as.RewindBuffer();

    as.MUL(x31, x20, x15);
    REQUIRE(value == 0x02FA0FB3);

    as.RewindBuffer();

    as.MUL(x20, x31, x15);
    REQUIRE(value == 0x02FF8A33);
}

TEST_CASE("MULH", "[rv32m]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.MULH(x31, x15, x20);
    REQUIRE(value == 0x03479FB3);

    as.RewindBuffer();

    as.MULH(x31, x20, x15);
    REQUIRE(value == 0x02FA1FB3);

    as.RewindBuffer();

    as.MULH(x20, x31, x15);
    REQUIRE(value == 0x02FF9A33);
}

TEST_CASE("MULW", "[rv64m]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.MULW(x31, x15, x20);
    REQUIRE(value == 0x03478FBB);

    as.RewindBuffer();

    as.MULW(x31, x20, x15);
    REQUIRE(value == 0x02FA0FBB);

    as.RewindBuffer();

    as.MULW(x20, x31, x15);
    REQUIRE(value == 0x02FF8A3B);
}

TEST_CASE("MULHSU", "[rv32m]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.MULHSU(x31, x15, x20);
    REQUIRE(value == 0x0347AFB3);

    as.RewindBuffer();

    as.MULHSU(x31, x20, x15);
    REQUIRE(value == 0x02FA2FB3);

    as.RewindBuffer();

    as.MULHSU(x20, x31, x15);
    REQUIRE(value == 0x02FFAA33);
}

TEST_CASE("MULHU", "[rv32m]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.MULHU(x31, x15, x20);
    REQUIRE(value == 0x0347BFB3);

    as.RewindBuffer();

    as.MULHU(x31, x20, x15);
    REQUIRE(value == 0x02FA3FB3);

    as.RewindBuffer();

    as.MULHU(x20, x31, x15);
    REQUIRE(value == 0x02FFBA33);
}

TEST_CASE("REM", "[rv32m]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.REM(x31, x15, x20);
    REQUIRE(value == 0x0347EFB3);

    as.RewindBuffer();

    as.REM(x31, x20, x15);
    REQUIRE(value == 0x02FA6FB3);

    as.RewindBuffer();

    as.REM(x20, x31, x15);
    REQUIRE(value == 0x02FFEA33);
}

TEST_CASE("REMW", "[rv64m]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.REMW(x31, x15, x20);
    REQUIRE(value == 0x0347EFBB);

    as.RewindBuffer();

    as.REMW(x31, x20, x15);
    REQUIRE(value == 0x02FA6FBB);

    as.RewindBuffer();

    as.REMW(x20, x31, x15);
    REQUIRE(value == 0x02FFEA3B);
}

TEST_CASE("REMU", "[rv32m]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.REMU(x31, x15, x20);
    REQUIRE(value == 0x0347FFB3);

    as.RewindBuffer();

    as.REMU(x31, x20, x15);
    REQUIRE(value == 0x02FA7FB3);

    as.RewindBuffer();

    as.REMU(x20, x31, x15);
    REQUIRE(value == 0x02FFFA33);
}

TEST_CASE("REMUW", "[rv64m]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.REMUW(x31, x15, x20);
    REQUIRE(value == 0x0347FFBB);

    as.RewindBuffer();

    as.REMUW(x31, x20, x15);
    REQUIRE(value == 0x02FA7FBB);

    as.RewindBuffer();

    as.REMUW(x20, x31, x15);
    REQUIRE(value == 0x02FFFA3B);
}
