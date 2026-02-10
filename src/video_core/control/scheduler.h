// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <mutex>
#include <ankerl/unordered_dense.h>

#include "video_core/dma_pusher.h"

namespace Tegra {

class GPU;

namespace Control {

struct ChannelState;

class Scheduler {
public:
    explicit Scheduler(GPU& gpu_);
    ~Scheduler();

    void Push(s32 channel, CommandList&& entries);

    void DeclareChannel(std::shared_ptr<ChannelState> new_channel);

private:
    ankerl::unordered_dense::map<s32, std::shared_ptr<ChannelState>> channels;
    std::mutex scheduling_guard;
    GPU& gpu;
};

} // namespace Control

} // namespace Tegra
