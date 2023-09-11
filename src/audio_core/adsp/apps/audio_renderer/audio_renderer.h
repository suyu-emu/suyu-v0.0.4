// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <thread>

#include "audio_core/adsp/apps/audio_renderer/command_buffer.h"
#include "audio_core/adsp/apps/audio_renderer/command_list_processor.h"
#include "audio_core/adsp/mailbox.h"
#include "common/common_types.h"
#include "common/polyfill_thread.h"
#include "common/reader_writer_queue.h"
#include "common/thread.h"

namespace Core {
class System;
namespace Timing {
struct EventType;
}
namespace Memory {
class Memory;
}
class System;
} // namespace Core

namespace AudioCore {
namespace Sink {
class Sink;
}

namespace ADSP::AudioRenderer {

enum Message : u32 {
    Invalid = 0x00,
    MapUnmap_Map = 0x01,
    MapUnmap_MapResponse = 0x02,
    MapUnmap_Unmap = 0x03,
    MapUnmap_UnmapResponse = 0x04,
    MapUnmap_InvalidateCache = 0x05,
    MapUnmap_InvalidateCacheResponse = 0x06,
    MapUnmap_Shutdown = 0x07,
    MapUnmap_ShutdownResponse = 0x08,
    InitializeOK = 0x16,
    RenderResponse = 0x20,
    Render = 0x2A,
    Shutdown = 0x34,
};

/**
 * The AudioRenderer application running on the ADSP.
 */
class AudioRenderer {
public:
    explicit AudioRenderer(Core::System& system, Core::Memory::Memory& memory, Sink::Sink& sink);
    ~AudioRenderer();

    /**
     * Start the AudioRenderer.
     *
     * @param mailbox The mailbox to use for this session.
     */
    void Start();

    /**
     * Stop the AudioRenderer.
     */
    void Stop();

    void Signal();
    void Wait();

    void Send(Direction dir, MailboxMessage message);
    MailboxMessage Receive(Direction dir, bool block = true);

    void SetCommandBuffer(s32 session_id, CpuAddr buffer, u64 size, u64 time_limit,
                          u64 applet_resource_user_id, bool reset) noexcept;
    u32 GetRemainCommandCount(s32 session_id) const noexcept;
    void ClearRemainCommandCount(s32 session_id) noexcept;
    u64 GetRenderingStartTick(s32 session_id) const noexcept;

private:
    /**
     * Main AudioRenderer thread, responsible for processing the command lists.
     */
    void Main(std::stop_token stop_token);

    /**
     * Creates the streams which will receive the processed samples.
     */
    void CreateSinkStreams();

    /// Core system
    Core::System& system;
    /// Memory
    Core::Memory::Memory& memory;
    /// The output sink the AudioRenderer will use
    Sink::Sink& sink;
    /// The active mailbox
    Mailbox mailbox;
    /// Main thread
    std::jthread main_thread{};
    /// The current state
    std::atomic<bool> running{};
    std::array<CommandBuffer, MaxRendererSessions> command_buffers{};
    /// The command lists to process
    std::array<CommandListProcessor, MaxRendererSessions> command_list_processors{};
    /// The streams which will receive the processed samples
    std::array<Sink::SinkStream*, MaxRendererSessions> streams{};
    u64 signalled_tick{0};
};

} // namespace ADSP::AudioRenderer
} // namespace AudioCore
