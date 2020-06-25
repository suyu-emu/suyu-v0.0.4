// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_set>
#include <utility>

#include "common/alignment.h"
#include "common/common_types.h"
#include "video_core/gpu.h"

namespace VideoCommon {

class BufferBlock {
public:
    bool Overlaps(VAddr start, VAddr end) const {
        return (cpu_addr < end) && (cpu_addr_end > start);
    }

    bool IsInside(VAddr other_start, VAddr other_end) const {
        return cpu_addr <= other_start && other_end <= cpu_addr_end;
    }

    std::size_t Offset(VAddr in_addr) const {
        return static_cast<std::size_t>(in_addr - cpu_addr);
    }

    VAddr CpuAddr() const {
        return cpu_addr;
    }

    VAddr CpuAddrEnd() const {
        return cpu_addr_end;
    }

    void SetCpuAddr(VAddr new_addr) {
        cpu_addr = new_addr;
        cpu_addr_end = new_addr + size;
    }

    std::size_t Size() const {
        return size;
    }

    u64 Epoch() const {
        return epoch;
    }

    void SetEpoch(u64 new_epoch) {
        epoch = new_epoch;
    }

protected:
    explicit BufferBlock(VAddr cpu_addr_, std::size_t size_) : size{size_} {
        SetCpuAddr(cpu_addr_);
    }

private:
    VAddr cpu_addr{};
    VAddr cpu_addr_end{};
    std::size_t size{};
    u64 epoch{};
};

} // namespace VideoCommon
