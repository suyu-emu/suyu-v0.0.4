// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Common {

bool AtomicCompareAndSwap(volatile u8* pointer, u8 value, u8 expected);
bool AtomicCompareAndSwap(volatile u16* pointer, u16 value, u16 expected);
bool AtomicCompareAndSwap(volatile u32* pointer, u32 value, u32 expected);
bool AtomicCompareAndSwap(volatile u64* pointer, u64 value, u64 expected);
bool AtomicCompareAndSwap(volatile u64* pointer, u128 value, u128 expected);

} // namespace Common
