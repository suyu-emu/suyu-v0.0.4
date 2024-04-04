// SPDX-FileCopyrightText: Copyright (c) 2023 merryhime <https://mary.rs>
// SPDX-License-Identifier: MIT

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "architecture.hpp"

#ifdef ON_ARM64

#    include "oaknut/code_block.hpp"
#    include "oaknut/oaknut.hpp"
#    include "rand_int.hpp"

using namespace oaknut;
using namespace oaknut::util;

TEST_CASE("Basic Test (VectorCodeGenerator)")
{
    std::vector<std::uint32_t> vec;
    VectorCodeGenerator code{vec};

    code.MOV(W0, 42);
    code.RET();

    CodeBlock mem{4096};

    mem.unprotect();
    std::memcpy(mem.ptr(), vec.data(), vec.size() * sizeof(std::uint32_t));
    mem.protect();
    mem.invalidate_all();

    int result = ((int (*)())mem.ptr())();
    REQUIRE(result == 42);
}

TEST_CASE("Fibonacci (VectorCodeGenerator)")
{
    std::vector<std::uint32_t> vec;
    VectorCodeGenerator code{vec};

    Label start, end, zero, recurse;

    code.l(start);
    code.STP(X29, X30, SP, PRE_INDEXED, -32);
    code.STP(X20, X19, SP, 16);
    code.MOV(X29, SP);
    code.MOV(W19, W0);
    code.SUBS(W0, W0, 1);
    code.B(LT, zero);
    code.B(NE, recurse);
    code.MOV(W0, 1);
    code.B(end);

    code.l(zero);
    code.MOV(W0, WZR);
    code.B(end);

    code.l(recurse);
    code.BL(start);
    code.MOV(W20, W0);
    code.SUB(W0, W19, 2);
    code.BL(start);
    code.ADD(W0, W0, W20);

    code.l(end);
    code.LDP(X20, X19, SP, 16);
    code.LDP(X29, X30, SP, POST_INDEXED, 32);
    code.RET();

    CodeBlock mem{4096};

    mem.unprotect();
    std::memcpy(mem.ptr(), vec.data(), vec.size() * sizeof(std::uint32_t));
    mem.protect();
    mem.invalidate_all();

    auto fib = (int (*)(int))mem.ptr();

    REQUIRE(fib(0) == 0);
    REQUIRE(fib(1) == 1);
    REQUIRE(fib(5) == 5);
    REQUIRE(fib(9) == 34);
}

#endif
