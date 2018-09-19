// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Core {

class ExclusiveMonitor {
public:
    virtual ~ExclusiveMonitor();

    virtual void SetExclusive(std::size_t core_index, VAddr addr) = 0;
    virtual void ClearExclusive() = 0;

    virtual bool ExclusiveWrite8(std::size_t core_index, VAddr vaddr, u8 value) = 0;
    virtual bool ExclusiveWrite16(std::size_t core_index, VAddr vaddr, u16 value) = 0;
    virtual bool ExclusiveWrite32(std::size_t core_index, VAddr vaddr, u32 value) = 0;
    virtual bool ExclusiveWrite64(std::size_t core_index, VAddr vaddr, u64 value) = 0;
    virtual bool ExclusiveWrite128(std::size_t core_index, VAddr vaddr, u128 value) = 0;
};

} // namespace Core
