// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <atomic>
#include <memory>
#include <span>
#include <vector>

#include "audio_core/audio_core.h"
#include "audio_core/common/common.h"
#include "audio_core/sink/sink_stream.h"
#include "common/common_types.h"
#include "common/fixed_point.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/core_timing.h"

namespace AudioCore::Sink {

void SinkStream::AppendBuffer(SinkBuffer& buffer, std::span<s16> samples) {
    if (type == StreamType::In)
        return;

    constexpr s32 min = (std::numeric_limits<s16>::min)();
    constexpr s32 max = (std::numeric_limits<s16>::max)();

    auto yuzu_volume = Settings::Volume();
    if (yuzu_volume > 1.0f)
        yuzu_volume = 0.6f + 20.0f * std::log10(yuzu_volume);
    yuzu_volume = std::max(yuzu_volume, 0.001f);
    auto const volume = system_volume * device_volume * yuzu_volume;

    if (system_channels > device_channels) {
        static constexpr std::array<f32, 4> tcoeff{1.0f, 0.596f, 0.354f, 0.707f};
        for (u32 r_offs = 0, w_offs = 0; r_offs < samples.size(); r_offs += system_channels, w_offs += device_channels) {
            std::array<f32, 6> ccoeff{0.f};
            for (u32 i = 0; i < system_channels; ++i)
                ccoeff[i] = f32(samples[r_offs + i]);

            std::array<f32, 6> rcoeff{
                ccoeff[u32(Channels::FrontLeft)],
                ccoeff[u32(Channels::BackLeft)],
                ccoeff[u32(Channels::Center)],
                ccoeff[u32(Channels::LFE)],
                ccoeff[u32(Channels::BackRight)],
                ccoeff[u32(Channels::FrontRight)],
            };

            const f32 left  = rcoeff[0] * tcoeff[0] + rcoeff[2] * tcoeff[1] + rcoeff[3] * tcoeff[2] + rcoeff[1] * tcoeff[3];
            const f32 right = rcoeff[5] * tcoeff[0] + rcoeff[2] * tcoeff[1] + rcoeff[3] * tcoeff[2] + rcoeff[4] * tcoeff[3];

            samples[w_offs + 0] = s16(std::clamp(s32(left * volume),  min, max));
            samples[w_offs + 1] = s16(std::clamp(s32(right * volume), min, max));
        }

        queue.EmplaceWait(buffer);
        samples_buffer.Push(samples.subspan(0, samples.size() / system_channels * device_channels));
    } else if (system_channels < device_channels) {
        std::vector<s16> new_samples(samples.size() / system_channels * device_channels);
        for (u32 r_offs = 0, w_offs = 0; r_offs < samples.size(); r_offs += system_channels, w_offs += device_channels)
            for (u32 channel = 0; channel < system_channels; ++channel)
                new_samples[w_offs + channel] = s16(std::clamp(s32(f32(samples[r_offs + channel]) * volume), min, max));

        queue.EmplaceWait(buffer);
        samples_buffer.Push(new_samples);
    } else {
        if (volume != 1.0f) {
            for (u32 i = 0; i < samples.size(); ++i)
                samples[i] = s16(std::clamp(s32(f32(samples[i]) * volume), min, max));
        }

        queue.EmplaceWait(buffer);
        samples_buffer.Push(samples);
    }

    ++queued_buffers;
}

std::vector<s16> SinkStream::ReleaseBuffer(u64 num_samples) {
    auto samples{samples_buffer.Pop(num_samples)};

    // TODO: Up-mix to 6 channels if the game expects it.
    // For audio input this is unlikely to ever be the case though.

    // Incoming mic volume seems to always be very quiet, so multiply by an additional 8 here.
    // TODO: Play with this and find something that works better.
    constexpr s32 min = (std::numeric_limits<s16>::min)();
    constexpr s32 max = (std::numeric_limits<s16>::max)();
    auto volume{system_volume * device_volume * 8};
    for (u32 i = 0; i < samples.size(); i++)
        samples[i] = s16(std::clamp(s32(f32(samples[i]) * volume), min, max));

    if (samples.size() < num_samples)
        samples.resize(num_samples, 0);
    return samples;
}

void SinkStream::ClearQueue() {
    std::scoped_lock lk{release_mutex};

    samples_buffer.Pop();
    SinkBuffer tmp;
    while (queue.TryPop(tmp));

    queued_buffers = 0;
    playing_buffer = {};
    playing_buffer.consumed = true;
}

void SinkStream::ProcessAudioIn(std::span<const s16> input_buffer, std::size_t num_frames) {
    const std::size_t num_channels = GetDeviceChannels();
    const std::size_t frame_size = num_channels;
    const std::size_t frame_size_bytes = frame_size * sizeof(s16);
    size_t frames_written{0};

    // If we're paused or going to shut down, we don't want to consume buffers as coretiming is
    // paused and we'll desync, so just return.
    if (system.IsPaused() || system.IsShuttingDown())
        return;

    while (frames_written < num_frames) {
        // If the playing buffer has been consumed or has no frames, we need a new one
        if (playing_buffer.consumed || playing_buffer.frames == 0) {
            if (!queue.TryPop(playing_buffer)) {
                // If no buffer was available we've underrun, just push the samples and
                // continue.
                samples_buffer.Push(&input_buffer[frames_written * frame_size], (num_frames - frames_written) * frame_size);
                frames_written = num_frames;
                continue;
            }
            // Successfully dequeued a new buffer.
            queued_buffers--;
        }

        // Get the minimum frames available between the currently playing buffer, and the
        // amount we have left to fill
        size_t frames_available{std::min<u64>(playing_buffer.frames - playing_buffer.frames_played, num_frames - frames_written)};

        samples_buffer.Push(&input_buffer[frames_written * frame_size], frames_available * frame_size);

        frames_written += frames_available;
        playing_buffer.frames_played += frames_available;

        // If that's all the frames in the current buffer, add its samples and mark it as
        // consumed
        if (playing_buffer.frames_played >= playing_buffer.frames)
            playing_buffer.consumed = true;
    }

    std::memcpy(&last_frame[0], &input_buffer[(frames_written - 1) * frame_size], frame_size_bytes);
}

void SinkStream::ProcessAudioOutAndRender(std::span<s16> output_buffer, std::size_t num_frames) {
    const std::size_t num_channels = GetDeviceChannels();
    const std::size_t frame_size = num_channels;
    const std::size_t frame_size_bytes = frame_size * sizeof(s16);
    size_t frames_written{0};
    size_t actual_frames_written{0};

    if (system.IsPaused() || system.IsShuttingDown()) {
        if (system.IsShuttingDown()) {
            std::scoped_lock lk{release_mutex};
            queued_buffers.store(0);
            release_cv.notify_one();
        }

        static constexpr std::array<s16, 6> silence{};
        for (size_t i = 0; i < num_frames; i++)
            std::memcpy(&output_buffer[i * frame_size], silence.data(), frame_size_bytes);
        return;
    }

    while (frames_written < num_frames) {
        if (playing_buffer.consumed || playing_buffer.frames == 0) {
            std::unique_lock lk{release_mutex};

            if (!queue.TryPop(playing_buffer)) {
                lk.unlock();
                for (size_t i = frames_written; i < num_frames; i++)
                    std::memcpy(&output_buffer[i * frame_size], last_frame.data(), frame_size_bytes);
                frames_written = num_frames;
                continue;
            }

            --queued_buffers;
            lk.unlock();
            release_cv.notify_one();
        }

        const size_t frames_available = std::min<u64>(playing_buffer.frames - playing_buffer.frames_played, num_frames - frames_written);

        samples_buffer.Pop(&output_buffer[frames_written * frame_size], frames_available * frame_size);

        frames_written += frames_available;
        actual_frames_written += frames_available;
        playing_buffer.frames_played += frames_available;

        if (playing_buffer.frames_played >= playing_buffer.frames)
            playing_buffer.consumed = true;
    }

    std::memcpy(last_frame.data(), &output_buffer[(frames_written - 1) * frame_size], frame_size_bytes);

    {
        std::scoped_lock lk{sample_count_lock};
        last_sample_count_update_time = system.CoreTiming().GetGlobalTimeNs();
        min_played_sample_count = max_played_sample_count;
        max_played_sample_count += actual_frames_written;
    }
}

u64 SinkStream::GetExpectedPlayedSampleCount() {
    std::scoped_lock lk{sample_count_lock};
    auto cur_time{system.CoreTiming().GetGlobalTimeNs()};
    auto time_delta{cur_time - last_sample_count_update_time};
    auto exp_played_sample_count{min_played_sample_count + (TargetSampleRate * time_delta) / std::chrono::seconds{1}};

    // Add 25ms of latency in sample reporting to allow for some leeway in scheduler timings
    return std::min<u64>(exp_played_sample_count, max_played_sample_count) + TargetSampleCount * 5;
}

void SinkStream::WaitFreeSpace(std::stop_token stop_token) {
    std::unique_lock lk{release_mutex};

    auto can_continue = [this]() {
        return paused || queued_buffers < max_queue_size;
    };

    release_cv.wait_for(lk, std::chrono::milliseconds(7), can_continue);

    if (queued_buffers > max_queue_size + 3) {
        release_cv.wait(lk, stop_token, can_continue);
    }
}

void SinkStream::SignalPause() {
    {
        std::scoped_lock lk{release_mutex};
        paused = true;
    }
    release_cv.notify_one();
}

} // namespace AudioCore::Sink
