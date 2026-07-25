// SPDX-FileCopyrightText: Copyright 2026 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

#include "audio_core/sink/sink.h"
#include "audio_core/sink/sink_stream.h"

namespace Core {
class System;
} // namespace Core

namespace AudioCore::Sink {

/**
 * Sample queue shared between the emulated audio renderer (producer, running
 * on suyu's audio thread) and the libretro frontend (consumer, draining once
 * per retro_run on RetroArch's thread).
 *
 * A plain locked deque rather than a lock-free ring: the producer appends in
 * large infrequent chunks and the consumer drains everything at 60Hz, so
 * contention is negligible and the simplicity is worth more than the
 * micro-optimisation.
 */
class LibretroSampleQueue {
public:
    static LibretroSampleQueue& Instance() {
        static LibretroSampleQueue instance;
        return instance;
    }

    void Push(std::span<const s16> samples) {
        std::scoped_lock lock{mutex};
        // Hard cap the backlog. If the frontend stops draining (paused, or a
        // frontend that never calls retro_run), an uncapped queue would grow
        // without bound; dropping the oldest audio is the right failure here
        // since stale samples are useless anyway.
        constexpr size_t kMaxQueuedSamples = 48000 * 2 * 2; // ~2s stereo
        if (queue.size() + samples.size() > kMaxQueuedSamples) {
            const size_t overflow = queue.size() + samples.size() - kMaxQueuedSamples;
            const size_t to_drop = std::min(overflow, queue.size());
            queue.erase(queue.begin(), queue.begin() + static_cast<ptrdiff_t>(to_drop));
        }
        queue.insert(queue.end(), samples.begin(), samples.end());
    }

    /// Moves every queued sample into @p out, leaving the queue empty.
    void Drain(std::vector<s16>& out) {
        std::scoped_lock lock{mutex};
        out.assign(queue.begin(), queue.end());
        queue.clear();
    }

    void Clear() {
        std::scoped_lock lock{mutex};
        queue.clear();
    }

private:
    std::mutex mutex;
    std::vector<s16> queue;
};

class LibretroSinkStreamImpl final : public SinkStream {
public:
    explicit LibretroSinkStreamImpl(Core::System& system_, StreamType type_)
        : SinkStream{system_, type_} {}
    ~LibretroSinkStreamImpl() override = default;

    void AppendBuffer(SinkBuffer&, std::span<s16> samples) override {
        // Only the final mixed output belongs in the frontend's audio stream;
        // render/capture streams are intermediate and would double up.
        if (type == StreamType::Out) {
            LibretroSampleQueue::Instance().Push(samples);
        }
    }

    std::vector<s16> ReleaseBuffer(u64) override {
        return {};
    }
};

/**
 * Audio sink that hands the emulated console's mixed output to the libretro
 * frontend instead of to a host audio device.
 */
class LibretroSink final : public Sink {
public:
    explicit LibretroSink(std::string_view) {}
    ~LibretroSink() override = default;

    SinkStream* AcquireSinkStream(Core::System& system, u32, const std::string&,
                                  StreamType type) override {
        auto stream = std::make_unique<LibretroSinkStreamImpl>(system, type);
        auto* raw = stream.get();
        streams.push_back(std::move(stream));
        return raw;
    }

    void CloseStream(SinkStream* stream) override {
        std::erase_if(streams, [stream](const SinkStreamPtr& s) { return s.get() == stream; });
    }

    void CloseStreams() override {
        streams.clear();
    }

    f32 GetDeviceVolume() const override {
        return volume;
    }
    void SetDeviceVolume(f32 volume_) override {
        volume = volume_;
    }
    void SetSystemVolume(f32 volume_) override {
        system_volume = volume_;
    }

private:
    std::vector<SinkStreamPtr> streams;
    f32 volume{1.0f};
    f32 system_volume{1.0f};
};

} // namespace AudioCore::Sink
