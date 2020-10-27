// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Tegra {
class GPU;
class Nvdec;

class Host1x {
public:
    struct Host1xClassRegisters {
        u32 incr_syncpt{};
        u32 incr_syncpt_ctrl{};
        u32 incr_syncpt_error{};
        INSERT_PADDING_WORDS(5);
        u32 wait_syncpt{};
        u32 wait_syncpt_base{};
        u32 wait_syncpt_incr{};
        u32 load_syncpt_base{};
        u32 incr_syncpt_base{};
        u32 clear{};
        u32 wait{};
        u32 wait_with_interrupt{};
        u32 delay_use{};
        u32 tick_count_high{};
        u32 tick_count_low{};
        u32 tick_ctrl{};
        INSERT_PADDING_WORDS(23);
        u32 ind_ctrl{};
        u32 ind_off2{};
        u32 ind_off{};
        std::array<u32, 31> ind_data{};
        INSERT_PADDING_WORDS(1);
        u32 load_syncpoint_payload32{};
        u32 stall_ctrl{};
        u32 wait_syncpt32{};
        u32 wait_syncpt_base32{};
        u32 load_syncpt_base32{};
        u32 incr_syncpt_base32{};
        u32 stall_count_high{};
        u32 stall_count_low{};
        u32 xref_ctrl{};
        u32 channel_xref_high{};
        u32 channel_xref_low{};
    };
    static_assert(sizeof(Host1xClassRegisters) == 0x164, "Host1xClassRegisters is an invalid size");

    enum class Method : u32 {
        WaitSyncpt = offsetof(Host1xClassRegisters, wait_syncpt) / 4,
        LoadSyncptPayload32 = offsetof(Host1xClassRegisters, load_syncpoint_payload32) / 4,
        WaitSyncpt32 = offsetof(Host1xClassRegisters, wait_syncpt32) / 4,
    };

    explicit Host1x(GPU& gpu);
    ~Host1x();

    /// Writes the method into the state, Invoke Execute() if encountered
    void ProcessMethod(Host1x::Method method, const std::vector<u32>& arguments);

private:
    /// For Host1x, execute is waiting on a syncpoint previously written into the state
    void Execute(u32 data);

    /// Write argument into the provided offset
    void StateWrite(u32 offset, u32 arguments);

    u32 syncpoint_value{};
    Host1xClassRegisters state{};
    GPU& gpu;
};

} // namespace Tegra
