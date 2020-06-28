// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Common {

bool AtomicCompareAndSwap(u8 volatile* pointer, u8 value, u8 expected);
bool AtomicCompareAndSwap(u16 volatile* pointer, u16 value, u16 expected);
bool AtomicCompareAndSwap(u32 volatile* pointer, u32 value, u32 expected);
bool AtomicCompareAndSwap(u64 volatile* pointer, u64 value, u64 expected);
bool AtomicCompareAndSwap(u64 volatile* pointer, u128 value, u128 expected);

} // namespace Common
