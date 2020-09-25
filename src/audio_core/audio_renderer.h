// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <vector>

#include "audio_core/behavior_info.h"
#include "audio_core/command_generator.h"
#include "audio_core/common.h"
#include "audio_core/effect_context.h"
#include "audio_core/memory_pool.h"
#include "audio_core/mix_context.h"
#include "audio_core/sink_context.h"
#include "audio_core/splitter_context.h"
#include "audio_core/stream.h"
#include "audio_core/voice_context.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/result.h"

namespace Core::Timing {
class CoreTiming;
}

namespace Kernel {
class WritableEvent;
}

namespace Core::Memory {
class Memory;
}

namespace AudioCore {
using DSPStateHolder = std::array<VoiceState*, 6>;

class AudioOut;

struct RendererInfo {
    u64_le elasped_frame_count{};
    INSERT_PADDING_WORDS(2);
};
static_assert(sizeof(RendererInfo) == 0x10, "RendererInfo is an invalid size");

class AudioRenderer {
public:
    AudioRenderer(Core::Timing::CoreTiming& core_timing, Core::Memory::Memory& memory_,
                  AudioCommon::AudioRendererParameter params,
                  std::shared_ptr<Kernel::WritableEvent> buffer_event, std::size_t instance_number);
    ~AudioRenderer();

    ResultCode UpdateAudioRenderer(const std::vector<u8>& input_params,
                                   std::vector<u8>& output_params);
    void QueueMixedBuffer(Buffer::Tag tag);
    void ReleaseAndQueueBuffers();
    u32 GetSampleRate() const;
    u32 GetSampleCount() const;
    u32 GetMixBufferCount() const;
    Stream::State GetStreamState() const;

private:
    BehaviorInfo behavior_info{};

    AudioCommon::AudioRendererParameter worker_params;
    std::shared_ptr<Kernel::WritableEvent> buffer_event;
    std::vector<ServerMemoryPoolInfo> memory_pool_info;
    VoiceContext voice_context;
    EffectContext effect_context;
    MixContext mix_context;
    SinkContext sink_context;
    SplitterContext splitter_context;
    std::vector<VoiceState> voices;
    std::unique_ptr<AudioOut> audio_out;
    StreamPtr stream;
    Core::Memory::Memory& memory;
    CommandGenerator command_generator;
    std::size_t elapsed_frame_count{};
    std::vector<s32> temp_mix_buffer{};
};

} // namespace AudioCore
