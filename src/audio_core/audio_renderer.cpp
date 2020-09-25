// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>

#include "audio_core/audio_out.h"
#include "audio_core/audio_renderer.h"
#include "audio_core/common.h"
#include "audio_core/info_updater.h"
#include "audio_core/voice_context.h"
#include "common/logging/log.h"
#include "core/hle/kernel/writable_event.h"
#include "core/memory.h"
#include "core/settings.h"

namespace AudioCore {
AudioRenderer::AudioRenderer(Core::Timing::CoreTiming& core_timing, Core::Memory::Memory& memory_,
                             AudioCommon::AudioRendererParameter params,
                             std::shared_ptr<Kernel::WritableEvent> buffer_event,
                             std::size_t instance_number)
    : worker_params{params}, buffer_event{buffer_event},
      memory_pool_info(params.effect_count + params.voice_count * 4),
      voice_context(params.voice_count), effect_context(params.effect_count), mix_context(),
      sink_context(params.sink_count), splitter_context(),
      voices(params.voice_count), memory{memory_},
      command_generator(worker_params, voice_context, mix_context, splitter_context, effect_context,
                        memory),
      temp_mix_buffer(AudioCommon::TOTAL_TEMP_MIX_SIZE) {
    behavior_info.SetUserRevision(params.revision);
    splitter_context.Initialize(behavior_info, params.splitter_count,
                                params.num_splitter_send_channels);
    mix_context.Initialize(behavior_info, params.submix_count + 1, params.effect_count);
    audio_out = std::make_unique<AudioCore::AudioOut>();
    stream =
        audio_out->OpenStream(core_timing, params.sample_rate, AudioCommon::STREAM_NUM_CHANNELS,
                              fmt::format("AudioRenderer-Instance{}", instance_number),
                              [=]() { buffer_event->Signal(); });
    audio_out->StartStream(stream);

    QueueMixedBuffer(0);
    QueueMixedBuffer(1);
    QueueMixedBuffer(2);
    QueueMixedBuffer(3);
}

AudioRenderer::~AudioRenderer() = default;

u32 AudioRenderer::GetSampleRate() const {
    return worker_params.sample_rate;
}

u32 AudioRenderer::GetSampleCount() const {
    return worker_params.sample_count;
}

u32 AudioRenderer::GetMixBufferCount() const {
    return worker_params.mix_buffer_count;
}

Stream::State AudioRenderer::GetStreamState() const {
    return stream->GetState();
}

static constexpr s16 ClampToS16(s32 value) {
    return static_cast<s16>(std::clamp(value, -32768, 32767));
}

ResultCode AudioRenderer::UpdateAudioRenderer(const std::vector<u8>& input_params,
                                              std::vector<u8>& output_params) {

    InfoUpdater info_updater{input_params, output_params, behavior_info};

    if (!info_updater.UpdateBehaviorInfo(behavior_info)) {
        LOG_ERROR(Audio, "Failed to update behavior info input parameters");
        return AudioCommon::Audren::ERR_INVALID_PARAMETERS;
    }

    if (!info_updater.UpdateMemoryPools(memory_pool_info)) {
        LOG_ERROR(Audio, "Failed to update memory pool parameters");
        return AudioCommon::Audren::ERR_INVALID_PARAMETERS;
    }

    if (!info_updater.UpdateVoiceChannelResources(voice_context)) {
        LOG_ERROR(Audio, "Failed to update voice channel resource parameters");
        return AudioCommon::Audren::ERR_INVALID_PARAMETERS;
    }

    if (!info_updater.UpdateVoices(voice_context, memory_pool_info, 0)) {
        LOG_ERROR(Audio, "Failed to update voice parameters");
        return AudioCommon::Audren::ERR_INVALID_PARAMETERS;
    }

    // TODO(ogniK): Deal with stopped audio renderer but updates still taking place
    if (!info_updater.UpdateEffects(effect_context, true)) {
        LOG_ERROR(Audio, "Failed to update effect parameters");
        return AudioCommon::Audren::ERR_INVALID_PARAMETERS;
    }

    if (behavior_info.IsSplitterSupported()) {
        if (!info_updater.UpdateSplitterInfo(splitter_context)) {
            LOG_ERROR(Audio, "Failed to update splitter parameters");
            return AudioCommon::Audren::ERR_INVALID_PARAMETERS;
        }
    }

    auto mix_result = info_updater.UpdateMixes(mix_context, worker_params.mix_buffer_count,
                                               splitter_context, effect_context);

    if (mix_result.IsError()) {
        LOG_ERROR(Audio, "Failed to update mix parameters");
        return mix_result;
    }

    // TODO(ogniK): Sinks
    if (!info_updater.UpdateSinks(sink_context)) {
        LOG_ERROR(Audio, "Failed to update sink parameters");
        return AudioCommon::Audren::ERR_INVALID_PARAMETERS;
    }

    // TODO(ogniK): Performance buffer
    if (!info_updater.UpdatePerformanceBuffer()) {
        LOG_ERROR(Audio, "Failed to update performance buffer parameters");
        return AudioCommon::Audren::ERR_INVALID_PARAMETERS;
    }

    if (!info_updater.UpdateErrorInfo(behavior_info)) {
        LOG_ERROR(Audio, "Failed to update error info");
        return AudioCommon::Audren::ERR_INVALID_PARAMETERS;
    }

    if (behavior_info.IsElapsedFrameCountSupported()) {
        if (!info_updater.UpdateRendererInfo(elapsed_frame_count)) {
            LOG_ERROR(Audio, "Failed to update renderer info");
            return AudioCommon::Audren::ERR_INVALID_PARAMETERS;
        }
    }
    // TODO(ogniK): Statistics

    if (!info_updater.WriteOutputHeader()) {
        LOG_ERROR(Audio, "Failed to write output header");
        return AudioCommon::Audren::ERR_INVALID_PARAMETERS;
    }

    // TODO(ogniK): Check when all sections are implemented

    if (!info_updater.CheckConsumedSize()) {
        LOG_ERROR(Audio, "Audio buffers were not consumed!");
        return AudioCommon::Audren::ERR_INVALID_PARAMETERS;
    }

    ReleaseAndQueueBuffers();

    return RESULT_SUCCESS;
}

void AudioRenderer::QueueMixedBuffer(Buffer::Tag tag) {
    command_generator.PreCommand();
    // Clear mix buffers before our next operation
    command_generator.ClearMixBuffers();

    // If the splitter is not in use, sort our mixes
    if (!splitter_context.UsingSplitter()) {
        mix_context.SortInfo();
    }
    // Sort our voices
    voice_context.SortInfo();

    // Handle samples
    command_generator.GenerateVoiceCommands();
    command_generator.GenerateSubMixCommands();
    command_generator.GenerateFinalMixCommands();

    command_generator.PostCommand();
    // Base sample size
    std::size_t BUFFER_SIZE{worker_params.sample_count};
    // Samples
    std::vector<s16> buffer(BUFFER_SIZE * stream->GetNumChannels());
    // Make sure to clear our samples
    std::memset(buffer.data(), 0, buffer.size() * sizeof(s16));

    if (sink_context.InUse()) {
        const auto stream_channel_count = stream->GetNumChannels();
        const auto buffer_offsets = sink_context.OutputBuffers();
        const auto channel_count = buffer_offsets.size();
        const auto& final_mix = mix_context.GetFinalMixInfo();
        const auto& in_params = final_mix.GetInParams();
        std::vector<s32*> mix_buffers(channel_count);
        for (std::size_t i = 0; i < channel_count; i++) {
            mix_buffers[i] =
                command_generator.GetMixBuffer(in_params.buffer_offset + buffer_offsets[i]);
        }

        for (std::size_t i = 0; i < BUFFER_SIZE; i++) {
            if (channel_count == 1) {
                const auto sample = ClampToS16(mix_buffers[0][i]);
                buffer[i * stream_channel_count + 0] = sample;
                if (stream_channel_count > 1) {
                    buffer[i * stream_channel_count + 1] = sample;
                }
                if (stream_channel_count == 6) {
                    buffer[i * stream_channel_count + 2] = sample;
                    buffer[i * stream_channel_count + 4] = sample;
                    buffer[i * stream_channel_count + 5] = sample;
                }
            } else if (channel_count == 2) {
                const auto l_sample = ClampToS16(mix_buffers[0][i]);
                const auto r_sample = ClampToS16(mix_buffers[1][i]);
                if (stream_channel_count == 1) {
                    buffer[i * stream_channel_count + 0] = l_sample;
                } else if (stream_channel_count == 2) {
                    buffer[i * stream_channel_count + 0] = l_sample;
                    buffer[i * stream_channel_count + 1] = r_sample;
                } else if (stream_channel_count == 6) {
                    buffer[i * stream_channel_count + 0] = l_sample;
                    buffer[i * stream_channel_count + 1] = r_sample;

                    buffer[i * stream_channel_count + 2] =
                        ClampToS16((static_cast<s32>(l_sample) + static_cast<s32>(r_sample)) / 2);

                    buffer[i * stream_channel_count + 4] = l_sample;
                    buffer[i * stream_channel_count + 5] = r_sample;
                }

            } else if (channel_count == 6) {
                const auto fl_sample = ClampToS16(mix_buffers[0][i]);
                const auto fr_sample = ClampToS16(mix_buffers[1][i]);
                const auto fc_sample = ClampToS16(mix_buffers[2][i]);
                const auto lf_sample = ClampToS16(mix_buffers[3][i]);
                const auto bl_sample = ClampToS16(mix_buffers[4][i]);
                const auto br_sample = ClampToS16(mix_buffers[5][i]);

                if (stream_channel_count == 1) {
                    buffer[i * stream_channel_count + 0] = fc_sample;
                } else if (stream_channel_count == 2) {
                    buffer[i * stream_channel_count + 0] =
                        static_cast<s16>(0.3694f * static_cast<float>(fl_sample) +
                                         0.2612f * static_cast<float>(fc_sample) +
                                         0.3694f * static_cast<float>(bl_sample));
                    buffer[i * stream_channel_count + 1] =
                        static_cast<s16>(0.3694f * static_cast<float>(fr_sample) +
                                         0.2612f * static_cast<float>(fc_sample) +
                                         0.3694f * static_cast<float>(br_sample));
                } else if (stream_channel_count == 6) {
                    buffer[i * stream_channel_count + 0] = fl_sample;
                    buffer[i * stream_channel_count + 1] = fr_sample;
                    buffer[i * stream_channel_count + 2] = fc_sample;
                    buffer[i * stream_channel_count + 3] = lf_sample;
                    buffer[i * stream_channel_count + 4] = bl_sample;
                    buffer[i * stream_channel_count + 5] = br_sample;
                }
            }
        }
    }

    audio_out->QueueBuffer(stream, tag, std::move(buffer));
    elapsed_frame_count++;
    voice_context.UpdateStateByDspShared();
}

void AudioRenderer::ReleaseAndQueueBuffers() {
    const auto released_buffers{audio_out->GetTagsAndReleaseBuffers(stream, 2)};
    for (const auto& tag : released_buffers) {
        QueueMixedBuffer(tag);
    }
}

} // namespace AudioCore
