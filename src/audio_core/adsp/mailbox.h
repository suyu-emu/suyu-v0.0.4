// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/bounded_threadsafe_queue.h"
#include "common/common_types.h"

namespace AudioCore::ADSP {

enum class AppMailboxId : u32 {
    Invalid = 0,
    AudioRenderer = 50,
    AudioRendererMemoryMapUnmap = 51,
};

enum class Direction : u32 {
    Host,
    DSP,
};

struct MailboxMessage {
    u32 msg;
    std::span<u8> data;
};

class Mailbox {
public:
    void Initialize(AppMailboxId id_) {
        Reset();
        id = id_;
    }

    AppMailboxId Id() const noexcept {
        return id;
    }

    void Send(Direction dir, MailboxMessage&& message) {
        auto& queue = dir == Direction::Host ? host_queue : adsp_queue;
        queue.EmplaceWait(std::move(message));
    }

    MailboxMessage Receive(Direction dir, bool block = true) {
        auto& queue = dir == Direction::Host ? host_queue : adsp_queue;
        MailboxMessage t;
        if (block) {
            queue.PopWait(t);
        } else {
            queue.TryPop(t);
        }
        return t;
    }

    void Reset() {
        id = AppMailboxId::Invalid;
        MailboxMessage t;
        while (host_queue.TryPop(t)) {
        }
        while (adsp_queue.TryPop(t)) {
        }
    }

private:
    AppMailboxId id{0};
    Common::SPSCQueue<MailboxMessage> host_queue;
    Common::SPSCQueue<MailboxMessage> adsp_queue;
};

} // namespace AudioCore::ADSP
