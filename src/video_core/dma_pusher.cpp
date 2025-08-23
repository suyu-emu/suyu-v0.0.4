// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/settings.h"
#include "core/core.h"
#include "video_core/dma_pusher.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/gpu.h"
#include "video_core/guest_memory.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"

namespace Tegra {

constexpr u32 MacroRegistersStart = 0xE00;
[[maybe_unused]] constexpr u32 ComputeInline = 0x6D;

DmaPusher::DmaPusher(Core::System& system_, GPU& gpu_, MemoryManager& memory_manager_,
                     Control::ChannelState& channel_state_)
    : gpu{gpu_}, system{system_}, memory_manager{memory_manager_}, puller{gpu_, memory_manager_,
                                                                          *this, channel_state_}, signal_sync{false}, synced{false} {}

DmaPusher::~DmaPusher() = default;

void DmaPusher::DispatchCalls() {

    dma_pushbuffer_subindex = 0;

    dma_state.is_last_call = true;

    while (system.IsPoweredOn()) {
        if (!Step()) {
            break;
        }
    }
    gpu.FlushCommands();
    gpu.OnCommandListEnd();
}

bool DmaPusher::Step() {
    if (!ib_enable || dma_pushbuffer.empty()) {
        // pushbuffer empty and IB empty or nonexistent - nothing to do
        return false;
    }

    CommandList& command_list{dma_pushbuffer.front()};

    ASSERT_OR_EXECUTE(
        command_list.command_lists.size() || command_list.prefetch_command_list.size(), {
            // Somehow the command_list is empty, in order to avoid a crash
            // We ignore it and assume its size is 0.
            dma_pushbuffer.pop();
            dma_pushbuffer_subindex = 0;
            return true;
        });

    if (command_list.prefetch_command_list.size()) {
        // Prefetched command list from nvdrv, used for things like synchronization
        ProcessCommands(command_list.prefetch_command_list);
        dma_pushbuffer.pop();
    } else {
        const CommandListHeader command_list_header{
            command_list.command_lists[dma_pushbuffer_subindex++]};

        if (signal_sync) {
            std::unique_lock lk(sync_mutex);
            sync_cv.wait(lk, [this]() { return synced; });
            signal_sync = false;
            synced = false;
        }

        dma_state.dma_get = command_list_header.addr;

        if (command_list_header.size == 0) {
            return true;
        }

        // Push buffer non-empty, read a word
        if (dma_state.method >= MacroRegistersStart) {
            if (subchannels[dma_state.subchannel]) {
                subchannels[dma_state.subchannel]->current_dirty = memory_manager.IsMemoryDirty(
                    dma_state.dma_get, command_list_header.size * sizeof(u32));
            }
        }

        const auto safe_process = [&] {
            Tegra::Memory::GpuGuestMemory<Tegra::CommandHeader,
                                          Tegra::Memory::GuestMemoryFlags::SafeRead>
                headers(memory_manager, dma_state.dma_get, command_list_header.size,
                        &command_headers);
            ProcessCommands(headers);
        };

        const auto unsafe_process = [&] {
            Tegra::Memory::GpuGuestMemory<Tegra::CommandHeader,
                                          Tegra::Memory::GuestMemoryFlags::UnsafeRead>
                headers(memory_manager, dma_state.dma_get, command_list_header.size,
                        &command_headers);
            ProcessCommands(headers);
        };

        const Settings::DmaAccuracy accuracy = Settings::values.dma_accuracy.GetValue();
        const bool use_gpu_accuracy = accuracy == Settings::DmaAccuracy::Default;

        // reduces eye bleeding but also macros are dumb so idk
#define CHECK_LEVEL(level) use_gpu_accuracy ? Settings::IsGPULevel##level() : accuracy == Settings::DmaAccuracy::level;
            const bool force_safe = CHECK_LEVEL(Extreme)
            const bool unsafe_compute = CHECK_LEVEL(High)
#undef CHECK_LEVEL

        if (force_safe) {
            safe_process();
        } else if (unsafe_compute) {
            if (dma_state.method >= MacroRegistersStart) {
                unsafe_process();
            } else {
                safe_process();
            }
        } else {
            unsafe_process();
        }

        if (dma_pushbuffer_subindex >= command_list.command_lists.size()) {
            // We've gone through the current list, remove it from the queue
            dma_pushbuffer.pop();
            dma_pushbuffer_subindex = 0;
        } else if (command_list.command_lists[dma_pushbuffer_subindex].sync && Settings::values.sync_memory_operations.GetValue()) {
            signal_sync = true;
        }

        if (signal_sync) {
            rasterizer->SignalFence([this]() {
            std::scoped_lock lk(sync_mutex);
            synced = true;
            sync_cv.notify_all();
            });
        }
    }
    return true;
}

void DmaPusher::ProcessCommands(std::span<const CommandHeader> commands) {
    for (std::size_t index = 0; index < commands.size();) {
        const CommandHeader& command_header = commands[index];

        if (dma_state.method_count) {
            // Data word of methods command
            dma_state.dma_word_offset = static_cast<u32>(index * sizeof(u32));
            if (dma_state.non_incrementing) {
                const u32 max_write = static_cast<u32>(
                    std::min<std::size_t>(index + dma_state.method_count, commands.size()) - index);
                CallMultiMethod(&command_header.argument, max_write);
                dma_state.method_count -= max_write;
                dma_state.is_last_call = true;
                index += max_write;
                continue;
            } else {
                dma_state.is_last_call = dma_state.method_count <= 1;
                CallMethod(command_header.argument);
            }

            if (!dma_state.non_incrementing) {
                dma_state.method++;
            }

            if (dma_increment_once) {
                dma_state.non_incrementing = true;
            }

            dma_state.method_count--;
        } else {
            // No command active - this is the first word of a new one
            switch (command_header.mode) {
            case SubmissionMode::Increasing:
                SetState(command_header);
                dma_state.non_incrementing = false;
                dma_increment_once = false;
                break;
            case SubmissionMode::NonIncreasing:
                SetState(command_header);
                dma_state.non_incrementing = true;
                dma_increment_once = false;
                break;
            case SubmissionMode::Inline:
                dma_state.method = command_header.method;
                dma_state.subchannel = command_header.subchannel;
                dma_state.dma_word_offset = static_cast<u64>(
                    -static_cast<s64>(dma_state.dma_get)); // negate to set address as 0
                CallMethod(command_header.arg_count);
                dma_state.non_incrementing = true;
                dma_increment_once = false;
                break;
            case SubmissionMode::IncreaseOnce:
                SetState(command_header);
                dma_state.non_incrementing = false;
                dma_increment_once = true;
                break;
            default:
                break;
            }
        }
        index++;
    }
}

void DmaPusher::SetState(const CommandHeader& command_header) {
    dma_state.method = command_header.method;
    dma_state.subchannel = command_header.subchannel;
    dma_state.method_count = command_header.method_count;
}

void DmaPusher::CallMethod(u32 argument) const {
    if (dma_state.method < non_puller_methods) {
        puller.CallPullerMethod(Engines::Puller::MethodCall{
            dma_state.method,
            argument,
            dma_state.subchannel,
            dma_state.method_count,
        });
    } else {
        auto subchannel = subchannels[dma_state.subchannel];
        if (!subchannel->execution_mask[dma_state.method]) [[likely]] {
            subchannel->method_sink.emplace_back(dma_state.method, argument);
            return;
        }
        subchannel->ConsumeSink();
        subchannel->current_dma_segment = dma_state.dma_get + dma_state.dma_word_offset;
        subchannel->CallMethod(dma_state.method, argument, dma_state.is_last_call);
    }
}

void DmaPusher::CallMultiMethod(const u32* base_start, u32 num_methods) const {
    if (dma_state.method < non_puller_methods) {
        puller.CallMultiMethod(dma_state.method, dma_state.subchannel, base_start, num_methods,
                               dma_state.method_count);
    } else {
        auto subchannel = subchannels[dma_state.subchannel];
        subchannel->ConsumeSink();
        subchannel->current_dma_segment = dma_state.dma_get + dma_state.dma_word_offset;
        subchannel->CallMultiMethod(dma_state.method, base_start, num_methods,
                                    dma_state.method_count);
    }
}

void DmaPusher::BindRasterizer(VideoCore::RasterizerInterface* rasterizer_) {
    rasterizer = rasterizer_;
    puller.BindRasterizer(rasterizer);
}

} // namespace Tegra
