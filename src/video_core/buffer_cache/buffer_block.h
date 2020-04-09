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
    bool Overlaps(const VAddr start, const VAddr end) const {
        return (cpu_addr < end) && (cpu_addr_end > start);
    }

    bool IsInside(const VAddr other_start, const VAddr other_end) const {
        return cpu_addr <= other_start && other_end <= cpu_addr_end;
    }

    std::size_t GetOffset(const VAddr in_addr) {
        return static_cast<std::size_t>(in_addr - cpu_addr);
    }

    VAddr GetCpuAddr() const {
        return cpu_addr;
    }

    VAddr GetCpuAddrEnd() const {
        return cpu_addr_end;
    }

    void SetCpuAddr(const VAddr new_addr) {
        cpu_addr = new_addr;
        cpu_addr_end = new_addr + size;
    }

    std::size_t GetSize() const {
        return size;
    }

    void SetEpoch(u64 new_epoch) {
        epoch = new_epoch;
    }

    u64 GetEpoch() {
        return epoch;
    }

protected:
    explicit BufferBlock(VAddr cpu_addr, const std::size_t size) : size{size} {
        SetCpuAddr(cpu_addr);
    }
    ~BufferBlock() = default;

private:
    VAddr cpu_addr{};
    VAddr cpu_addr_end{};
    std::size_t size{};
    u64 epoch{};
};

} // namespace VideoCommon
