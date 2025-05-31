// This file is part of the mcl project.
// Copyright (c) 2022 merryhime
// SPDX-License-Identifier: MIT

#pragma once

#include "mcl/stdint.hpp"

namespace mcl::memory::literals {

constexpr u64 operator""_KiB(unsigned long long int x)
{
    return 1024ULL * x;
}

constexpr u64 operator""_MiB(unsigned long long int x)
{
    return 1024_KiB * x;
}

constexpr u64 operator""_GiB(unsigned long long int x)
{
    return 1024_MiB * x;
}

constexpr u64 operator""_TiB(unsigned long long int x)
{
    return 1024_GiB * x;
}

constexpr u64 operator""_PiB(unsigned long long int x)
{
    return 1024_TiB * x;
}

}  // namespace mcl::memory::literals