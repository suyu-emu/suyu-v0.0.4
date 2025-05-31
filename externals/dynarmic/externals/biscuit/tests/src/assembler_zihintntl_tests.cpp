#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("C.NTL.ALL", "[Zihintntl]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_NTL_ALL();
    REQUIRE(value == 0x9016);
}

TEST_CASE("C.NTL.S1", "[Zihintntl]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_NTL_S1();
    REQUIRE(value == 0x9012);
}

TEST_CASE("C.NTL.P1", "[Zihintntl]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_NTL_P1();
    REQUIRE(value == 0x900A);
}

TEST_CASE("C.NTL.PALL", "[Zihintntl]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.C_NTL_PALL();
    REQUIRE(value == 0x900E);
}

TEST_CASE("NTL.ALL", "[Zihintntl]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.NTL_ALL();
    REQUIRE(value == 0x00500033);
}

TEST_CASE("NTL.S1", "[Zihintntl]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.NTL_S1();
    REQUIRE(value == 0x00400033);
}

TEST_CASE("NTL.P1", "[Zihintntl]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.NTL_P1();
    REQUIRE(value == 0x00200033);
}

TEST_CASE("NTL.PALL", "[Zihintntl]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.NTL_PALL();
    REQUIRE(value == 0x00300033);
}
