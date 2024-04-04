#include <catch/catch.hpp>

#include <biscuit/assembler.hpp>

#include "assembler_test_utils.hpp"

using namespace biscuit;

TEST_CASE("VANDN.VV", "[Zvbb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VANDN(v20, v12, v10, VecMask::Yes);
    REQUIRE(value == 0x04C50A57);

    as.RewindBuffer();

    as.VANDN(v20, v12, v10, VecMask::No);
    REQUIRE(value == 0x06C50A57);
}

TEST_CASE("VANDN.VX", "[Zvbb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VANDN(v20, v12, x10, VecMask::Yes);
    REQUIRE(value == 0x04C54A57);

    as.RewindBuffer();

    as.VANDN(v20, v12, x10, VecMask::No);
    REQUIRE(value == 0x06C54A57);
}

TEST_CASE("VBREV.V", "[Zvbb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VBREV(v20, v12, VecMask::Yes);
    REQUIRE(value == 0x48C52A57);

    as.RewindBuffer();

    as.VBREV(v20, v12, VecMask::No);
    REQUIRE(value == 0x4AC52A57);
}

TEST_CASE("VBREV8.V", "[Zvbb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VBREV8(v20, v12, VecMask::Yes);
    REQUIRE(value == 0x48C42A57);

    as.RewindBuffer();

    as.VBREV8(v20, v12, VecMask::No);
    REQUIRE(value == 0x4AC42A57);
}

TEST_CASE("VREV8.V", "[Zvbb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VREV8(v20, v12, VecMask::Yes);
    REQUIRE(value == 0x48C4AA57);

    as.RewindBuffer();

    as.VREV8(v20, v12, VecMask::No);
    REQUIRE(value == 0x4AC4AA57);
}

TEST_CASE("VCLZ.V", "[Zvbb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VCLZ(v20, v12, VecMask::Yes);
    REQUIRE(value == 0x48C62A57);

    as.RewindBuffer();

    as.VCLZ(v20, v12, VecMask::No);
    REQUIRE(value == 0x4AC62A57);
}

TEST_CASE("VCTZ.V", "[Zvbb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VCTZ(v20, v12, VecMask::Yes);
    REQUIRE(value == 0x48C6AA57);

    as.RewindBuffer();

    as.VCTZ(v20, v12, VecMask::No);
    REQUIRE(value == 0x4AC6AA57);
}

TEST_CASE("VCPOP.V", "[Zvbb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VCPOP(v20, v12, VecMask::Yes);
    REQUIRE(value == 0x48C72A57);

    as.RewindBuffer();

    as.VCPOP(v20, v12, VecMask::No);
    REQUIRE(value == 0x4AC72A57);
}

TEST_CASE("VROL.VV", "[Zvbb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VROL(v20, v12, v10, VecMask::Yes);
    REQUIRE(value == 0x54C50A57);

    as.RewindBuffer();

    as.VROL(v20, v12, v10, VecMask::No);
    REQUIRE(value == 0x56C50A57);
}

TEST_CASE("VROL.VX", "[Zvbb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VROL(v20, v12, x10, VecMask::Yes);
    REQUIRE(value == 0x54C54A57);

    as.RewindBuffer();

    as.VROL(v20, v12, x10, VecMask::No);
    REQUIRE(value == 0x56C54A57);
}

TEST_CASE("VROR.VV", "[Zvbb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VROR(v20, v12, v10, VecMask::Yes);
    REQUIRE(value == 0x50C50A57);

    as.RewindBuffer();

    as.VROR(v20, v12, v10, VecMask::No);
    REQUIRE(value == 0x52C50A57);
}

TEST_CASE("VROR.VX", "[Zvbb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VROR(v20, v12, x10, VecMask::Yes);
    REQUIRE(value == 0x50C54A57);

    as.RewindBuffer();

    as.VROR(v20, v12, x10, VecMask::No);
    REQUIRE(value == 0x52C54A57);
}

TEST_CASE("VROR.VI", "[Zvbb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VROR(v20, v12, 63, VecMask::Yes);
    REQUIRE(value == 0x54CFBA57);

    as.RewindBuffer();

    as.VROR(v20, v12, 31, VecMask::Yes);
    REQUIRE(value == 0x50CFBA57);

    as.RewindBuffer();

    as.VROR(v20, v12, 63, VecMask::No);
    REQUIRE(value == 0x56CFBA57);

    as.RewindBuffer();

    as.VROR(v20, v12, 31, VecMask::No);
    REQUIRE(value == 0x52CFBA57);
}

TEST_CASE("VWSLL.VV", "[Zvbb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWSLL(v20, v12, v10, VecMask::Yes);
    REQUIRE(value == 0xD4C50A57);

    as.RewindBuffer();

    as.VWSLL(v20, v12, v10, VecMask::No);
    REQUIRE(value == 0xD6C50A57);
}

TEST_CASE("VWSLL.VX", "[Zvbb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWSLL(v20, v12, x10, VecMask::Yes);
    REQUIRE(value == 0xD4C54A57);

    as.RewindBuffer();

    as.VWSLL(v20, v12, x10, VecMask::No);
    REQUIRE(value == 0xD6C54A57);
}

TEST_CASE("VWSLL.VI", "[Zvbb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VWSLL(v20, v12, 31, VecMask::Yes);
    REQUIRE(value == 0xD4CFBA57);

    as.RewindBuffer();

    as.VWSLL(v20, v12, 15, VecMask::Yes);
    REQUIRE(value == 0xD4C7BA57);

    as.RewindBuffer();

    as.VWSLL(v20, v12, 31, VecMask::No);
    REQUIRE(value == 0xD6CFBA57);

    as.RewindBuffer();

    as.VWSLL(v20, v12, 15, VecMask::No);
    REQUIRE(value == 0xD6C7BA57);
}

TEST_CASE("VCLMUL.VV", "[Zvbc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VCLMUL(v20, v12, v10, VecMask::Yes);
    REQUIRE(value == 0x30C52A57);

    as.RewindBuffer();

    as.VCLMUL(v20, v12, v10, VecMask::No);
    REQUIRE(value == 0x32C52A57);
}

TEST_CASE("VCLMUL.VX", "[Zvbc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VCLMUL(v20, v12, x10, VecMask::Yes);
    REQUIRE(value == 0x30C56A57);

    as.RewindBuffer();

    as.VCLMUL(v20, v12, x10, VecMask::No);
    REQUIRE(value == 0x32C56A57);
}

TEST_CASE("VCLMULH.VV", "[Zvbc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VCLMULH(v20, v12, v10, VecMask::Yes);
    REQUIRE(value == 0x34C52A57);

    as.RewindBuffer();

    as.VCLMULH(v20, v12, v10, VecMask::No);
    REQUIRE(value == 0x36C52A57);
}

TEST_CASE("VCLMULH.VX", "[Zvbc]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VCLMULH(v20, v12, x10, VecMask::Yes);
    REQUIRE(value == 0x34C56A57);

    as.RewindBuffer();

    as.VCLMULH(v20, v12, x10, VecMask::No);
    REQUIRE(value == 0x36C56A57);
}

TEST_CASE("VGHSH.VV", "[Zvkg]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VGHSH(v20, v12, v10);
    REQUIRE(value == 0xB2C52A77);
}

TEST_CASE("VGMUL.VV", "[Zvkg]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VGMUL(v20, v12);
    REQUIRE(value == 0xA2C8AA77);
}

TEST_CASE("VAESDF.VV", "[Zvkned]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VAESDF_VV(v20, v12);
    REQUIRE(value == 0xA2C0AA77);
}

TEST_CASE("VAESDF.VS", "[Zvkned]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VAESDF_VS(v20, v12);
    REQUIRE(value == 0xA6C0AA77);
}

TEST_CASE("VAESDM.VV", "[Zvkned]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VAESDM_VV(v20, v12);
    REQUIRE(value == 0xA2C02A77);
}

TEST_CASE("VAESDM.VS", "[Zvkned]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VAESDM_VS(v20, v12);
    REQUIRE(value == 0xA6C02A77);
}

TEST_CASE("VAESEF.VV", "[Zvkned]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VAESEF_VV(v20, v12);
    REQUIRE(value == 0xA2C1AA77);
}

TEST_CASE("VAESEF.VS", "[Zvkned]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VAESEF_VS(v20, v12);
    REQUIRE(value == 0xA6C1AA77);
}

TEST_CASE("VAESEM.VV", "[Zvkned]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VAESEM_VV(v20, v12);
    REQUIRE(value == 0xA2C12A77);
}

TEST_CASE("VAESEM.VS", "[Zvkned]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VAESEM_VS(v20, v12);
    REQUIRE(value == 0xA6C12A77);
}

TEST_CASE("VAESKF1.VI", "[Zvkned]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    // Test mapping of out of range indices
    for (const uint32_t idx : {0U, 11U, 12U, 13U, 14U, 15U}) {
        as.VAESKF1(v20, v12, idx);

        const auto op_base = 0x8AC02A77U;
        const auto inverted_b3 = idx ^ 0b1000;
        const auto verify = op_base | (inverted_b3 << 15);

        REQUIRE(value == verify);

        as.RewindBuffer();
    }

   as.VAESKF1(v20, v12, 8);
   REQUIRE(value == 0x8AC42A77);
}

TEST_CASE("VAESKF2.VI", "[Zvkned]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    // Test mapping of out of range indices
    for (const uint32_t idx : {0U, 1U, 15U}) {
        as.VAESKF2(v20, v12, idx);

        const auto op_base = 0xAAC02A77;
        const auto inverted_b3 = idx ^ 0b1000;
        const auto verify = op_base | (inverted_b3 << 15);

        REQUIRE(value == verify);

        as.RewindBuffer();
    }

    as.VAESKF2(v20, v12, 8);
    REQUIRE(value == 0xAAC42A77);
}

TEST_CASE("VAESZ.VS", "[Zvkned]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VAESZ(v20, v12);
    REQUIRE(value == 0xA6C3AA77);
}

TEST_CASE("VSHA2MS.VV", "[Zvknhb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSHA2MS(v20, v12, v10);
    REQUIRE(value == 0xB6C52A77);
}

TEST_CASE("VSHA2CH.VV", "[Zvknhb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSHA2CH(v20, v12, v10);
    REQUIRE(value == 0xBAC52A77);
}

TEST_CASE("VSHA2CL.VV", "[Zvknhb]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSHA2CL(v20, v12, v10);
    REQUIRE(value == 0xBEC52A77);
}

TEST_CASE("VSM4K.VI", "[Zvksed]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 0; i <= 7; i++) {
        as.VSM4K(v20, v12, i);

        const auto op_base = 0x86C02A77U;
        const auto verify = op_base | (i << 15);
        REQUIRE(value == verify);

        as.RewindBuffer();
    }
}

TEST_CASE("VSM4R.VV", "[Zvksed]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSM4R_VV(v20, v12);
    REQUIRE(value == 0xA2C82A77);
}

TEST_CASE("VSM4R.VS", "[Zvksed]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSM4R_VS(v20, v12);
    REQUIRE(value == 0xA6C82A77);
}

TEST_CASE("VSM3C.VI", "[Zvksh]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    for (uint32_t i = 0; i <= 31; i++) {
        as.VSM3C(v20, v12, i);

        const auto op_base = 0xAEC02A77U;
        const auto verify = op_base | (i << 15);
        REQUIRE(value == verify);

        as.RewindBuffer();
    }
}

TEST_CASE("VSM3ME.VV", "[Zvksh]") {
    uint32_t value = 0;
    auto as = MakeAssembler64(value);

    as.VSM3ME(v20, v12, v10);
    REQUIRE(value == 0x82C52A77);
}
