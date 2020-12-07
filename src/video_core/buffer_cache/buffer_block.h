// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace VideoCommon {

class BufferBlock {
public:
    [[nodiscard]] bool Overlaps(VAddr start, VAddr end) const {
        return (cpu_addr < end) && (cpu_addr_end > start);
    }

    [[nodiscard]] bool IsInside(VAddr other_start, VAddr other_end) const {
        return cpu_addr <= other_start && other_end <= cpu_addr_end;
    }

    [[nodiscard]] std::size_t Offset(VAddr in_addr) const {
        return static_cast<std::size_t>(in_addr - cpu_addr);
    }

    [[nodiscard]] VAddr CpuAddr() const {
        return cpu_addr;
    }

    [[nodiscard]] VAddr CpuAddrEnd() const {
        return cpu_addr_end;
    }

    void SetCpuAddr(VAddr new_addr) {
        cpu_addr = new_addr;
        cpu_addr_end = new_addr + size;
    }

    [[nodiscard]] std::size_t Size() const {
        return size;
    }

    [[nodiscard]] u64 Epoch() const {
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
