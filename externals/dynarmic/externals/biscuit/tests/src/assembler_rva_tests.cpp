#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("AMOADD.D", "[rv64a]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOADD_D(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x0077BFAF);

    as.RewindBuffer();

    as.AMOADD_D(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x0477BFAF);

    as.RewindBuffer();

    as.AMOADD_D(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x0277BFAF);

    as.RewindBuffer();

    as.AMOADD_D(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x0677BFAF);
}

TEST_CASE("AMOADD.W", "[rv32a]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOADD_W(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x0077AFAF);

    as.RewindBuffer();

    as.AMOADD_W(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x0477AFAF);

    as.RewindBuffer();

    as.AMOADD_W(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x0277AFAF);

    as.RewindBuffer();

    as.AMOADD_W(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x0677AFAF);
}

TEST_CASE("AMOAND.D", "[rv64a]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOAND_D(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x6077BFAF);

    as.RewindBuffer();

    as.AMOAND_D(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x6477BFAF);

    as.RewindBuffer();

    as.AMOAND_D(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x6277BFAF);

    as.RewindBuffer();

    as.AMOAND_D(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x6677BFAF);
}

TEST_CASE("AMOAND.W", "[rv32a]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOAND_W(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x6077AFAF);

    as.RewindBuffer();

    as.AMOAND_W(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x6477AFAF);

    as.RewindBuffer();

    as.AMOAND_W(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x6277AFAF);

    as.RewindBuffer();

    as.AMOAND_W(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x6677AFAF);
}

TEST_CASE("AMOMAX.D", "[rv64a]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOMAX_D(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0xA077BFAF);

    as.RewindBuffer();

    as.AMOMAX_D(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0xA477BFAF);

    as.RewindBuffer();

    as.AMOMAX_D(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0xA277BFAF);

    as.RewindBuffer();

    as.AMOMAX_D(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0xA677BFAF);
}

TEST_CASE("AMOMAX.W", "[rv32a]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOMAX_W(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0xA077AFAF);

    as.RewindBuffer();

    as.AMOMAX_W(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0xA477AFAF);

    as.RewindBuffer();

    as.AMOMAX_W(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0xA277AFAF);

    as.RewindBuffer();

    as.AMOMAX_W(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0xA677AFAF);
}

TEST_CASE("AMOMAXU.D", "[rv64a]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOMAXU_D(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0xE077BFAF);

    as.RewindBuffer();

    as.AMOMAXU_D(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0xE477BFAF);

    as.RewindBuffer();

    as.AMOMAXU_D(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0xE277BFAF);

    as.RewindBuffer();

    as.AMOMAXU_D(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0xE677BFAF);
}

TEST_CASE("AMOMAXU.W", "[rv32a]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOMAXU_W(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0xE077AFAF);

    as.RewindBuffer();

    as.AMOMAXU_W(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0xE477AFAF);

    as.RewindBuffer();

    as.AMOMAXU_W(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0xE277AFAF);

    as.RewindBuffer();

    as.AMOMAXU_W(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0xE677AFAF);
}

TEST_CASE("AMOMIN.D", "[rv64a]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOMIN_D(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x8077BFAF);

    as.RewindBuffer();

    as.AMOMIN_D(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x8477BFAF);

    as.RewindBuffer();

    as.AMOMIN_D(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x8277BFAF);

    as.RewindBuffer();

    as.AMOMIN_D(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x8677BFAF);
}

TEST_CASE("AMOMIN.W", "[rv32a]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOMIN_W(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x8077AFAF);

    as.RewindBuffer();

    as.AMOMIN_W(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x8477AFAF);

    as.RewindBuffer();

    as.AMOMIN_W(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x8277AFAF);

    as.RewindBuffer();

    as.AMOMIN_W(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x8677AFAF);
}

TEST_CASE("AMOMINU.D", "[rv64a]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOMINU_D(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0xC077BFAF);

    as.RewindBuffer();

    as.AMOMINU_D(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0xC477BFAF);

    as.RewindBuffer();

    as.AMOMINU_D(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0xC277BFAF);

    as.RewindBuffer();

    as.AMOMINU_D(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0xC677BFAF);
}

TEST_CASE("AMOMINU.W", "[rv32a]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOMINU_W(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0xC077AFAF);

    as.RewindBuffer();

    as.AMOMINU_W(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0xC477AFAF);

    as.RewindBuffer();

    as.AMOMINU_W(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0xC277AFAF);

    as.RewindBuffer();

    as.AMOMINU_W(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0xC677AFAF);
}

TEST_CASE("AMOOR.D", "[rv64a]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOOR_D(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x4077BFAF);

    as.RewindBuffer();

    as.AMOOR_D(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x4477BFAF);

    as.RewindBuffer();

    as.AMOOR_D(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x4277BFAF);

    as.RewindBuffer();

    as.AMOOR_D(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x4677BFAF);
}

TEST_CASE("AMOOR.W", "[rv32a]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOOR_W(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x4077AFAF);

    as.RewindBuffer();

    as.AMOOR_W(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x4477AFAF);

    as.RewindBuffer();

    as.AMOOR_W(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x4277AFAF);

    as.RewindBuffer();

    as.AMOOR_W(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x4677AFAF);
}

TEST_CASE("AMOSWAP.D", "[rv64a]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOSWAP_D(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x0877BFAF);

    as.RewindBuffer();

    as.AMOSWAP_D(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x0C77BFAF);

    as.RewindBuffer();

    as.AMOSWAP_D(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x0A77BFAF);

    as.RewindBuffer();

    as.AMOSWAP_D(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x0E77BFAF);
}

TEST_CASE("AMOSWAP.W", "[rv32a]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOSWAP_W(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x0877AFAF);

    as.RewindBuffer();

    as.AMOSWAP_W(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x0C77AFAF);

    as.RewindBuffer();

    as.AMOSWAP_W(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x0A77AFAF);

    as.RewindBuffer();

    as.AMOSWAP_W(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x0E77AFAF);
}

TEST_CASE("AMOXOR.D", "[rv64a]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.AMOXOR_D(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x2077BFAF);

    as.RewindBuffer();

    as.AMOXOR_D(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x2477BFAF);

    as.RewindBuffer();

    as.AMOXOR_D(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x2277BFAF);

    as.RewindBuffer();

    as.AMOXOR_D(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x2677BFAF);
}

TEST_CASE("AMOXOR.W", "[rv32a]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.AMOXOR_W(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x2077AFAF);

    as.RewindBuffer();

    as.AMOXOR_W(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x2477AFAF);

    as.RewindBuffer();

    as.AMOXOR_W(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x2277AFAF);

    as.RewindBuffer();

    as.AMOXOR_W(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x2677AFAF);
}

TEST_CASE("LR.D", "[rv64a]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.LR_D(Ordering::None, x31, x15);
    REQUIRE(value == 0x1007BFAF);

    as.RewindBuffer();

    as.LR_D(Ordering::AQ, x31, x15);
    REQUIRE(value == 0x1407BFAF);

    as.RewindBuffer();

    as.LR_D(Ordering::RL, x31, x15);
    REQUIRE(value == 0x1207BFAF);

    as.RewindBuffer();

    as.LR_D(Ordering::AQRL, x31, x15);
    REQUIRE(value == 0x1607BFAF);
}

TEST_CASE("LR.W", "[rv32a]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.LR_W(Ordering::None, x31, x15);
    REQUIRE(value == 0x1007AFAF);

    as.RewindBuffer();

    as.LR_W(Ordering::AQ, x31, x15);
    REQUIRE(value == 0x1407AFAF);

    as.RewindBuffer();

    as.LR_W(Ordering::RL, x31, x15);
    REQUIRE(value == 0x1207AFAF);

    as.RewindBuffer();

    as.LR_W(Ordering::AQRL, x31, x15);
    REQUIRE(value == 0x1607AFAF);
}

TEST_CASE("SC.D", "[rv64a]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.SC_D(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x1877BFAF);

    as.RewindBuffer();

    as.SC_D(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x1C77BFAF);

    as.RewindBuffer();

    as.SC_D(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x1A77BFAF);

    as.RewindBuffer();

    as.SC_D(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x1E77BFAF);
}

TEST_CASE("SC.W", "[rv32a]") {
    uint32_t value = 0;
    auto as = MakeAssembler32(value);

    as.SC_W(Ordering::None, x31, x7, x15);
    REQUIRE(value == 0x1877AFAF);

    as.RewindBuffer();

    as.SC_W(Ordering::AQ, x31, x7, x15);
    REQUIRE(value == 0x1C77AFAF);

    as.RewindBuffer();

    as.SC_W(Ordering::RL, x31, x7, x15);
    REQUIRE(value == 0x1A77AFAF);

    as.RewindBuffer();

    as.SC_W(Ordering::AQRL, x31, x7, x15);
    REQUIRE(value == 0x1E77AFAF);
}
