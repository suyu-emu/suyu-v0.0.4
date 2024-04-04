#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("AMOCAS.D", "[Zacas]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOCAS_D(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x2877BFAF);

    as.RewindBuffer();

    as.AMOCAS_D(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x2C77BFAF);

    as.RewindBuffer();

    as.AMOCAS_D(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x2A77BFAF);

    as.RewindBuffer();

    as.AMOCAS_D(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x2E77BFAF);
}

TEST_CASE("AMOCAS.Q", "[Zacas]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOCAS_Q(Ordering::None, x30, x6, x14);
    REQUIRE(value == 0x28674F2F);

    as.RewindBuffer();

    as.AMOCAS_Q(Ordering::AQ, x30, x6, x14);
    REQUIRE(value == 0x2C674F2F);

    as.RewindBuffer();

    as.AMOCAS_Q(Ordering::RL, x30, x6, x14);
    REQUIRE(value == 0x2A674F2F);

    as.RewindBuffer();

    as.AMOCAS_Q(Ordering::AQRL, x30, x6, x14);
    REQUIRE(value == 0x2E674F2F);
}

TEST_CASE("AMOCAS.W", "[Zacas]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOCAS_W(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x2877AFAF);

    as.RewindBuffer();

    as.AMOCAS_W(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x2C77AFAF);

    as.RewindBuffer();

    as.AMOCAS_W(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x2A77AFAF);

    as.RewindBuffer();

    as.AMOCAS_W(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x2E77AFAF);
}
