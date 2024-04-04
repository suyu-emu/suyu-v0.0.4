#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("CSRRC", "[Zicsr]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.CSRRC(x31, CSR::Cycle, x15);
    REQUIRE(value == 0xC007BFF3);

    as.RewindBuffer();

    as.CSRRC(x31, CSR::FFlags, x15);
    REQUIRE(value == 0x0017BFF3);

    as.RewindBuffer();

    as.CSRRC(x31, CSR::FRM, x15);
    REQUIRE(value == 0x0027BFF3);

    as.RewindBuffer();

    as.CSRRC(x31, CSR::FCSR, x15);
    REQUIRE(value == 0x0037BFF3);
}

TEST_CASE("CSRRCI", "[Zicsr]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.CSRRCI(x31, CSR::Cycle, 0);
    REQUIRE(value == 0xC0007FF3);

    as.RewindBuffer();

    as.CSRRCI(x31, CSR::FFlags, 0x1F);
    REQUIRE(value == 0x001FFFF3);

    as.RewindBuffer();

    as.CSRRCI(x31, CSR::FRM, 0x7);
    REQUIRE(value == 0x0023FFF3);
}

TEST_CASE("CSRRS", "[Zicsr]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.CSRRS(x31, CSR::Cycle, x15);
    REQUIRE(value == 0xC007AFF3);

    as.RewindBuffer();

    as.CSRRS(x31, CSR::FFlags, x15);
    REQUIRE(value == 0x0017AFF3);

    as.RewindBuffer();

    as.CSRRS(x31, CSR::FRM, x15);
    REQUIRE(value == 0x0027AFF3);

    as.RewindBuffer();

    as.CSRRS(x31, CSR::FCSR, x15);
    REQUIRE(value == 0x0037AFF3);
}

TEST_CASE("CSRRSI", "[Zicsr]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.CSRRSI(x31, CSR::Cycle, 0);
    REQUIRE(value == 0xC0006FF3);

    as.RewindBuffer();

    as.CSRRSI(x31, CSR::FFlags, 0x1F);
    REQUIRE(value == 0x001FEFF3);

    as.RewindBuffer();

    as.CSRRSI(x31, CSR::FRM, 0x7);
    REQUIRE(value == 0x0023EFF3);
}

TEST_CASE("CSRRW", "[Zicsr]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.CSRRW(x31, CSR::Cycle, x15);
    REQUIRE(value == 0xC0079FF3);

    as.RewindBuffer();

    as.CSRRW(x31, CSR::FFlags, x15);
    REQUIRE(value == 0x00179FF3);

    as.RewindBuffer();

    as.CSRRW(x31, CSR::FRM, x15);
    REQUIRE(value == 0x00279FF3);

    as.RewindBuffer();

    as.CSRRW(x31, CSR::FCSR, x15);
    REQUIRE(value == 0x00379FF3);
}

TEST_CASE("CSRRWI", "[Zicsr]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.CSRRWI(x31, CSR::Cycle, 0);
    REQUIRE(value == 0xC0005FF3);

    as.RewindBuffer();

    as.CSRRWI(x31, CSR::FFlags, 0x1F);
    REQUIRE(value == 0x001FDFF3);

    as.RewindBuffer();

    as.CSRRWI(x31, CSR::FRM, 0x7);
    REQUIRE(value == 0x0023DFF3);
}
