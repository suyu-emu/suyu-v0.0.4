// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/algorithm/interpolate.h"
#include "audio_core/command_generator.h"
#include "audio_core/mix_context.h"
#include "audio_core/voice_context.h"
#include "core/memory.h"

namespace AudioCore {
namespace {
static constexpr std::size_t MIX_BUFFER_SIZE = 0x3f00;
static constexpr std::size_t SCALED_MIX_BUFFER_SIZE = MIX_BUFFER_SIZE << 15ULL;

template <std::size_t N>
void ApplyMix(s32* output, const s32* input, s32 gain, s32 sample_count) {
    for (s32 i = 0; i < sample_count; i += N) {
        for (std::size_t j = 0; j < N; j++) {
            output[i + j] +=
                static_cast<s32>((static_cast<s64>(input[i + j]) * gain + 0x4000) >> 15);
        }
    }
}

s32 ApplyMixRamp(s32* output, const s32* input, float gain, float delta, s32 sample_count) {
    s32 x = 0;
    for (s32 i = 0; i < sample_count; i++) {
        x = static_cast<s32>(static_cast<float>(input[i]) * gain);
        output[i] += x;
        gain += delta;
    }
    return x;
}

void ApplyGain(s32* output, const s32* input, s32 gain, s32 delta, s32 sample_count) {
    for (s32 i = 0; i < sample_count; i++) {
        output[i] = static_cast<s32>((static_cast<s64>(input[i]) * gain + 0x4000) >> 15);
        gain += delta;
    }
}

void ApplyGainWithoutDelta(s32* output, const s32* input, s32 gain, s32 sample_count) {
    for (s32 i = 0; i < sample_count; i++) {
        output[i] = static_cast<s32>((static_cast<s64>(input[i]) * gain + 0x4000) >> 15);
    }
}

} // namespace

CommandGenerator::CommandGenerator(AudioCommon::AudioRendererParameter& worker_params,
                                   VoiceContext& voice_context, MixContext& mix_context,
                                   SplitterContext& splitter_context, Core::Memory::Memory& memory)
    : worker_params(worker_params), voice_context(voice_context), mix_context(mix_context),
      splitter_context(splitter_context), memory(memory),
      mix_buffer((worker_params.mix_buffer_count + AudioCommon::MAX_CHANNEL_COUNT) *
                 worker_params.sample_count),
      sample_buffer(MIX_BUFFER_SIZE) {}
CommandGenerator::~CommandGenerator() = default;

void CommandGenerator::ClearMixBuffers() {
    std::memset(mix_buffer.data(), 0, mix_buffer.size() * sizeof(s32));
    std::memset(sample_buffer.data(), 0, sample_buffer.size() * sizeof(s32));
}

void CommandGenerator::GenerateVoiceCommands() {
    if (dumping_frame) {
        LOG_CRITICAL(Audio, "(DSP_TRACE) GenerateVoiceCommands");
    }
    // Grab all our voices
    const auto voice_count = voice_context.GetVoiceCount();
    for (std::size_t i = 0; i < voice_count; i++) {
        auto& voice_info = voice_context.GetSortedInfo(i);
        // Update voices and check if we should queue them
        if (voice_info.ShouldSkip() || !voice_info.UpdateForCommandGeneration(voice_context)) {
            continue;
        }

        // Queue our voice
        GenerateVoiceCommand(voice_info);
    }
    // Update our splitters
    splitter_context.UpdateInternalState();
}

void CommandGenerator::GenerateVoiceCommand(ServerVoiceInfo& voice_info) {
    auto& in_params = voice_info.GetInParams();
    const auto channel_count = in_params.channel_count;

    for (s32 channel = 0; channel < channel_count; channel++) {
        const auto resource_id = in_params.voice_channel_resource_id[channel];
        auto& dsp_state = voice_context.GetDspSharedState(resource_id);
        auto& channel_resource = voice_context.GetChannelResource(resource_id);

        // Decode our samples for our channel
        GenerateDataSourceCommand(voice_info, dsp_state, channel);

        if (in_params.should_depop) {
            in_params.last_volume = 0.0f;
        } else if (in_params.splitter_info_id != AudioCommon::NO_SPLITTER ||
                   in_params.mix_id != AudioCommon::NO_MIX) {
            // Apply a biquad filter if needed
            GenerateBiquadFilterCommandForVoice(voice_info, dsp_state,
                                                worker_params.mix_buffer_count, channel);
            // Base voice volume ramping
            GenerateVolumeRampCommand(in_params.last_volume, in_params.volume, channel,
                                      in_params.node_id);
            in_params.last_volume = in_params.volume;

            if (in_params.mix_id != AudioCommon::NO_MIX) {
                // If we're using a mix id
                auto& mix_info = mix_context.GetInfo(in_params.mix_id);
                const auto& dest_mix_params = mix_info.GetInParams();

                // Voice Mixing
                GenerateVoiceMixCommand(
                    channel_resource.GetCurrentMixVolume(), channel_resource.GetLastMixVolume(),
                    dsp_state, dest_mix_params.buffer_offset, dest_mix_params.buffer_count,
                    worker_params.mix_buffer_count + channel, in_params.node_id);

                // Update last mix volumes
                channel_resource.UpdateLastMixVolumes();
            } else if (in_params.splitter_info_id != AudioCommon::NO_SPLITTER) {
                s32 base = channel;
                while (auto* destination_data =
                           GetDestinationData(in_params.splitter_info_id, base)) {
                    base += channel_count;

                    if (!destination_data->IsConfigured()) {
                        continue;
                    }
                    if (destination_data->GetMixId() >= mix_context.GetCount()) {
                        continue;
                    }

                    const auto& mix_info = mix_context.GetInfo(destination_data->GetMixId());
                    const auto& dest_mix_params = mix_info.GetInParams();
                    GenerateVoiceMixCommand(
                        destination_data->CurrentMixVolumes(), destination_data->LastMixVolumes(),
                        dsp_state, dest_mix_params.buffer_offset, dest_mix_params.buffer_count,
                        worker_params.mix_buffer_count + channel, in_params.node_id);
                    destination_data->MarkDirty();
                }
            }
        }

        // Update biquad filter enabled states
        for (std::size_t i = 0; i < AudioCommon::MAX_BIQUAD_FILTERS; i++) {
            in_params.was_biquad_filter_enabled[i] = in_params.biquad_filter[i].enabled;
        }
    }
}

void CommandGenerator::GenerateSubMixCommands() {
    const auto mix_count = mix_context.GetCount();
    for (std::size_t i = 0; i < mix_count; i++) {
        auto& mix_info = mix_context.GetSortedInfo(i);
        const auto& in_params = mix_info.GetInParams();
        if (!in_params.in_use || in_params.mix_id == AudioCommon::FINAL_MIX) {
            continue;
        }
        GenerateSubMixCommand(mix_info);
    }
}

void CommandGenerator::GenerateFinalMixCommands() {
    GenerateFinalMixCommand();
}

void CommandGenerator::PreCommand() {
    if (dumping_frame) {
        for (std::size_t i = 0; i < splitter_context.GetInfoCount(); i++) {
            const auto& base = splitter_context.GetInfo(i);
            std::string graph = fmt::format("b[{}]", i);
            auto* head = base.GetHead();
            while (head != nullptr) {
                graph += fmt::format("->{}", head->GetMixId());
                head = head->GetNextDestination();
            }
            LOG_CRITICAL(Audio, "(DSP_TRACE) SplitterGraph splitter_info={}, {}", i, graph);
        }
    }
}

void CommandGenerator::PostCommand() {
    if (dumping_frame) {
        dumping_frame = false;
    }
}

void CommandGenerator::GenerateDataSourceCommand(ServerVoiceInfo& voice_info, VoiceState& dsp_state,
                                                 s32 channel) {
    auto& in_params = voice_info.GetInParams();
    const auto depop = in_params.should_depop;

    if (in_params.mix_id != AudioCommon::NO_MIX) {
        auto& mix_info = mix_context.GetInfo(in_params.mix_id);
        // mix_info.
        // TODO(ogniK): Depop to destination mix
    } else if (in_params.splitter_info_id != AudioCommon::NO_SPLITTER) {
        // TODO(ogniK): Depop to splitter
    }

    if (!depop) {
        switch (in_params.sample_format) {
        case SampleFormat::Pcm16:
            DecodeFromWaveBuffers(voice_info, GetChannelMixBuffer(channel), dsp_state, channel,
                                  worker_params.sample_rate, worker_params.sample_count,
                                  in_params.node_id);
            break;
        case SampleFormat::Adpcm:
            ASSERT(channel == 0 && in_params.channel_count == 1);
            DecodeFromWaveBuffers(voice_info, GetChannelMixBuffer(0), dsp_state, 0,
                                  worker_params.sample_rate, worker_params.sample_count,
                                  in_params.node_id);
            break;
        default:
            UNREACHABLE_MSG("Unimplemented sample format={}", in_params.sample_format);
        }
    }
}

void CommandGenerator::GenerateBiquadFilterCommandForVoice(ServerVoiceInfo& voice_info,
                                                           VoiceState& dsp_state,
                                                           s32 mix_buffer_count, s32 channel) {
    for (std::size_t i = 0; i < AudioCommon::MAX_BIQUAD_FILTERS; i++) {
        const auto& in_params = voice_info.GetInParams();
        auto& biquad_filter = in_params.biquad_filter[i];
        // Check if biquad filter is actually used
        if (!biquad_filter.enabled) {
            continue;
        }

        // Reinitialize our biquad filter state if it was enabled previously
        if (!in_params.was_biquad_filter_enabled[i]) {
            std::memset(dsp_state.biquad_filter_state.data(), 0,
                        dsp_state.biquad_filter_state.size() * sizeof(s64));
        }

        // Generate biquad filter
        GenerateBiquadFilterCommand(mix_buffer_count, biquad_filter, dsp_state.biquad_filter_state,
                                    mix_buffer_count + channel, mix_buffer_count + channel,
                                    worker_params.sample_count, voice_info.GetInParams().node_id);
    }
}

void AudioCore::CommandGenerator::GenerateBiquadFilterCommand(
    s32 mix_buffer, const BiquadFilterParameter& params, std::array<s64, 2>& state,
    std::size_t input_offset, std::size_t output_offset, s32 sample_count, s32 node_id) {
    if (dumping_frame) {
        LOG_CRITICAL(Audio,
                     "(DSP_TRACE) GenerateBiquadFilterCommand node_id={}, "
                     "input_mix_buffer={}, output_mix_buffer={}",
                     node_id, input_offset, output_offset);
    }
    const auto* input = GetMixBuffer(input_offset);
    auto* output = GetMixBuffer(output_offset);

    // Biquad filter parameters
    const auto n0 = params.numerator[0];
    const auto n1 = params.numerator[1];
    const auto n2 = params.numerator[2];
    const auto d0 = params.denominator[0];
    const auto d1 = params.denominator[1];

    // Biquad filter states
    auto s0 = state[0];
    auto s1 = state[1];

    constexpr s64 MIN = std::numeric_limits<int32_t>::min();
    constexpr s64 MAX = std::numeric_limits<int32_t>::max();

    for (int i = 0; i < sample_count; ++i) {
        const auto sample = static_cast<int64_t>(input[i]);
        const auto f = (sample * n0 + s0 + 0x4000) >> 15;
        const auto y = std::clamp(f, MIN, MAX);
        s0 = sample * n1 + y * d0 + s1;
        s1 = sample * n2 + y * d1;
        output[i] = static_cast<s32>(y);
    }

    state[0] = s0;
    state[1] = s1;
}

ServerSplitterDestinationData* CommandGenerator::GetDestinationData(s32 splitter_id, s32 index) {
    if (splitter_id == AudioCommon::NO_SPLITTER) {
        return nullptr;
    }
    return splitter_context.GetDestinationData(splitter_id, index);
}

void CommandGenerator::GenerateVolumeRampCommand(float last_volume, float current_volume,
                                                 s32 channel, s32 node_id) {
    const auto last = static_cast<s32>(last_volume * 32768.0f);
    const auto current = static_cast<s32>(current_volume * 32768.0f);
    const auto delta = static_cast<s32>((static_cast<float>(current) - static_cast<float>(last)) /
                                        static_cast<float>(worker_params.sample_count));

    if (dumping_frame) {
        LOG_CRITICAL(Audio,
                     "(DSP_TRACE) GenerateVolumeRampCommand node_id={}, input={}, output={}, "
                     "last_volume={}, current_volume={}",
                     node_id, GetMixChannelBufferOffset(channel),
                     GetMixChannelBufferOffset(channel), last_volume, current_volume);
    }
    // Apply generic gain on samples
    ApplyGain(GetChannelMixBuffer(channel), GetChannelMixBuffer(channel), last, delta,
              worker_params.sample_count);
}

void CommandGenerator::GenerateVoiceMixCommand(const MixVolumeBuffer& mix_volumes,
                                               const MixVolumeBuffer& last_mix_volumes,
                                               VoiceState& dsp_state, s32 mix_buffer_offset,
                                               s32 mix_buffer_count, s32 voice_index, s32 node_id) {
    // Loop all our mix buffers
    for (s32 i = 0; i < mix_buffer_count; i++) {
        if (last_mix_volumes[i] != 0.0f || mix_volumes[i] != 0.0f) {
            const auto delta = static_cast<float>((mix_volumes[i] - last_mix_volumes[i])) /
                               static_cast<float>(worker_params.sample_count);

            if (dumping_frame) {
                LOG_CRITICAL(Audio,
                             "(DSP_TRACE) GenerateVoiceMixCommand node_id={}, input={}, "
                             "output={}, last_volume={}, current_volume={}",
                             node_id, voice_index, mix_buffer_offset + i, last_mix_volumes[i],
                             mix_volumes[i]);
            }

            dsp_state.previous_samples[i] =
                ApplyMixRamp(GetMixBuffer(mix_buffer_offset + i), GetMixBuffer(voice_index),
                             last_mix_volumes[i], delta, worker_params.sample_count);
        } else {
            dsp_state.previous_samples[i] = 0;
        }
    }
}

void CommandGenerator::GenerateSubMixCommand(ServerMixInfo& mix_info) {
    if (dumping_frame) {
        LOG_CRITICAL(Audio, "(DSP_TRACE) GenerateSubMixCommand");
    }
    // TODO(ogniK): Depop
    // TODO(ogniK): Effects
    GenerateMixCommands(mix_info);
}

void CommandGenerator::GenerateMixCommands(ServerMixInfo& mix_info) {
    if (!mix_info.HasAnyConnection()) {
        return;
    }
    const auto& in_params = mix_info.GetInParams();
    if (in_params.dest_mix_id != AudioCommon::NO_MIX) {
        const auto& dest_mix = mix_context.GetInfo(in_params.dest_mix_id);
        const auto& dest_in_params = dest_mix.GetInParams();

        const auto buffer_count = in_params.buffer_count;

        for (s32 i = 0; i < buffer_count; i++) {
            for (s32 j = 0; j < dest_in_params.buffer_count; j++) {
                const auto mixed_volume = in_params.volume * in_params.mix_volume[i][j];
                if (mixed_volume != 0.0f) {
                    GenerateMixCommand(dest_in_params.buffer_offset + j,
                                       in_params.buffer_offset + i, mixed_volume,
                                       in_params.node_id);
                }
            }
        }
    } else if (in_params.splitter_id != AudioCommon::NO_SPLITTER) {
        s32 base{};
        while (const auto* destination_data = GetDestinationData(in_params.splitter_id, base++)) {
            if (!destination_data->IsConfigured()) {
                continue;
            }

            const auto& dest_mix = mix_context.GetInfo(destination_data->GetMixId());
            const auto& dest_in_params = dest_mix.GetInParams();
            const auto mix_index = (base - 1) % in_params.buffer_count + in_params.buffer_offset;
            for (std::size_t i = 0; i < dest_in_params.buffer_count; i++) {
                const auto mixed_volume = in_params.volume * destination_data->GetMixVolume(i);
                if (mixed_volume != 0.0f) {
                    GenerateMixCommand(dest_in_params.buffer_offset + i, mix_index, mixed_volume,
                                       in_params.node_id);
                }
            }
        }
    }
}

void CommandGenerator::GenerateMixCommand(std::size_t output_offset, std::size_t input_offset,
                                          float volume, s32 node_id) {

    if (dumping_frame) {
        LOG_CRITICAL(Audio,
                     "(DSP_TRACE) GenerateMixCommand node_id={}, input={}, output={}, volume={}",
                     node_id, input_offset, output_offset, volume);
    }

    auto* output = GetMixBuffer(output_offset);
    const auto* input = GetMixBuffer(input_offset);

    const s32 gain = static_cast<s32>(volume * 32768.0f);
    // Mix with loop unrolling
    if (worker_params.sample_count % 4 == 0) {
        ApplyMix<4>(output, input, gain, worker_params.sample_count);
    } else if (worker_params.sample_count % 2 == 0) {
        ApplyMix<2>(output, input, gain, worker_params.sample_count);
    } else {
        ApplyMix<1>(output, input, gain, worker_params.sample_count);
    }
}

void CommandGenerator::GenerateFinalMixCommand() {
    if (dumping_frame) {
        LOG_CRITICAL(Audio, "(DSP_TRACE) GenerateFinalMixCommand");
    }
    // TODO(ogniK): Depop
    // TODO(ogniK): Effects
    auto& mix_info = mix_context.GetFinalMixInfo();
    const auto in_params = mix_info.GetInParams();
    for (s32 i = 0; i < in_params.buffer_count; i++) {
        const s32 gain = static_cast<s32>(in_params.volume * 32768.0f);
        if (dumping_frame) {
            LOG_CRITICAL(
                Audio,
                "(DSP_TRACE) ApplyGainWithoutDelta node_id={}, input={}, output={}, volume={}",
                in_params.node_id, in_params.buffer_offset + i, in_params.buffer_offset + i,
                in_params.volume);
        }
        ApplyGainWithoutDelta(GetMixBuffer(in_params.buffer_offset + i),
                              GetMixBuffer(in_params.buffer_offset + i), gain,
                              worker_params.sample_count);
    }
}

s32 CommandGenerator::DecodePcm16(ServerVoiceInfo& voice_info, VoiceState& dsp_state,
                                  s32 sample_count, s32 channel, std::size_t mix_offset) {
    auto& in_params = voice_info.GetInParams();
    const auto& wave_buffer = in_params.wave_buffer[dsp_state.wave_buffer_index];
    if (wave_buffer.buffer_address == 0) {
        return 0;
    }
    if (wave_buffer.buffer_size == 0) {
        return 0;
    }
    if (wave_buffer.end_sample_offset < wave_buffer.start_sample_offset) {
        return 0;
    }
    const auto samples_remaining =
        (wave_buffer.end_sample_offset - wave_buffer.start_sample_offset) - dsp_state.offset;
    const auto start_offset =
        ((wave_buffer.start_sample_offset + dsp_state.offset) * in_params.channel_count) *
        sizeof(s16);
    const auto buffer_pos = wave_buffer.buffer_address + start_offset;
    const auto samples_processed = std::min(sample_count, samples_remaining);

    if (in_params.channel_count == 1) {
        std::vector<s16> buffer(samples_processed);
        memory.ReadBlock(buffer_pos, buffer.data(), buffer.size() * sizeof(s16));
        for (std::size_t i = 0; i < buffer.size(); i++) {
            sample_buffer[mix_offset + i] = buffer[i];
        }
    } else {
        const auto channel_count = in_params.channel_count;
        std::vector<s16> buffer(samples_processed * channel_count);
        memory.ReadBlock(buffer_pos, buffer.data(), buffer.size() * sizeof(s16));

        for (std::size_t i = 0; i < samples_processed; i++) {
            sample_buffer[mix_offset + i] = buffer[i * channel_count + channel];
        }
    }

    return samples_processed;
}
s32 CommandGenerator::DecodeAdpcm(ServerVoiceInfo& voice_info, VoiceState& dsp_state,
                                  s32 sample_count, s32 channel, std::size_t mix_offset) {
    auto& in_params = voice_info.GetInParams();
    const auto& wave_buffer = in_params.wave_buffer[dsp_state.wave_buffer_index];
    if (wave_buffer.buffer_address == 0) {
        return 0;
    }
    if (wave_buffer.buffer_size == 0) {
        return 0;
    }
    if (wave_buffer.end_sample_offset < wave_buffer.start_sample_offset) {
        return 0;
    }

    const auto samples_remaining =
        (wave_buffer.end_sample_offset - wave_buffer.start_sample_offset) - dsp_state.offset;
    const auto start_offset =
        ((wave_buffer.start_sample_offset + dsp_state.offset) * in_params.channel_count);
    const auto buffer_pos = wave_buffer.buffer_address + start_offset;

    const auto samples_processed = std::min(sample_count, samples_remaining);

    if (start_offset > dsp_state.adpcm_samples.size()) {
        dsp_state.adpcm_samples.clear();
    }

    // TODO(ogniK): Proper ADPCM streaming
    if (dsp_state.adpcm_samples.empty()) {
        Codec::ADPCM_Coeff coeffs;
        memory.ReadBlock(in_params.additional_params_address, coeffs.data(),
                         sizeof(Codec::ADPCM_Coeff));
        std::vector<u8> buffer(wave_buffer.buffer_size);
        memory.ReadBlock(wave_buffer.buffer_address, buffer.data(), buffer.size());
        dsp_state.adpcm_samples =
            std::move(Codec::DecodeADPCM(buffer.data(), buffer.size(), coeffs, dsp_state.context));
    }

    for (std::size_t i = 0; i < samples_processed; i++) {
        const auto sample_offset = i + start_offset;
        sample_buffer[mix_offset + i] =
            dsp_state.adpcm_samples[sample_offset * in_params.channel_count + channel];
    }

    return samples_processed;
}

s32* CommandGenerator::GetMixBuffer(std::size_t index) {
    return mix_buffer.data() + (index * worker_params.sample_count);
}

const s32* CommandGenerator::GetMixBuffer(std::size_t index) const {
    return mix_buffer.data() + (index * worker_params.sample_count);
}

std::size_t CommandGenerator::GetMixChannelBufferOffset(s32 channel) const {
    return worker_params.mix_buffer_count + channel;
}

s32* CommandGenerator::GetChannelMixBuffer(s32 channel) {
    return GetMixBuffer(worker_params.mix_buffer_count + channel);
}

const s32* CommandGenerator::GetChannelMixBuffer(s32 channel) const {
    return GetMixBuffer(worker_params.mix_buffer_count + channel);
}

void CommandGenerator::DecodeFromWaveBuffers(ServerVoiceInfo& voice_info, s32* output,
                                             VoiceState& dsp_state, s32 channel,
                                             s32 target_sample_rate, s32 sample_count,
                                             s32 node_id) {
    auto& in_params = voice_info.GetInParams();
    if (dumping_frame) {
        LOG_CRITICAL(Audio,
                     "(DSP_TRACE) DecodeFromWaveBuffers, node_id={}, channel={}, "
                     "format={}, sample_count={}, sample_rate={}, mix_id={}, splitter_id={}",
                     node_id, channel, in_params.sample_format, sample_count, in_params.sample_rate,
                     in_params.mix_id, in_params.splitter_info_id);
    }
    ASSERT_OR_EXECUTE(output != nullptr, { return; });

    const auto resample_rate = static_cast<s32>(
        static_cast<float>(in_params.sample_rate) / static_cast<float>(target_sample_rate) *
        static_cast<float>(static_cast<s32>(in_params.pitch * 32768.0f)));
    auto* output_base = output;
    if ((dsp_state.fraction + sample_count * resample_rate) > (SCALED_MIX_BUFFER_SIZE - 4ULL)) {
        return;
    }

    auto min_required_samples =
        std::min(static_cast<s32>(SCALED_MIX_BUFFER_SIZE) - dsp_state.fraction, resample_rate);
    if (min_required_samples >= sample_count) {
        min_required_samples = sample_count;
    }

    std::size_t temp_mix_offset{};
    bool is_buffer_completed{false};
    auto samples_remaining = sample_count;
    while (samples_remaining > 0 && !is_buffer_completed) {
        const auto samples_to_output = std::min(samples_remaining, min_required_samples);
        const auto samples_to_read = (samples_to_output * resample_rate + dsp_state.fraction) >> 15;

        if (!in_params.behavior_flags.is_pitch_and_src_skipped) {
            // Append sample histtory for resampler
            for (std::size_t i = 0; i < AudioCommon::MAX_SAMPLE_HISTORY; i++) {
                sample_buffer[temp_mix_offset + i] = dsp_state.sample_history[i];
            }
            temp_mix_offset += 4;
        }

        s32 samples_read{};
        while (samples_read < samples_to_read) {
            const auto& wave_buffer = in_params.wave_buffer[dsp_state.wave_buffer_index];
            // No more data can be read
            if (!dsp_state.is_wave_buffer_valid[dsp_state.wave_buffer_index]) {
                is_buffer_completed = true;
                break;
            }

            if (in_params.sample_format == SampleFormat::Adpcm && dsp_state.offset == 0 &&
                wave_buffer.context_address != 0 && wave_buffer.context_size != 0) {
                // TODO(ogniK): ADPCM loop context
            }

            s32 samples_decoded{0};
            switch (in_params.sample_format) {
            case SampleFormat::Pcm16:
                samples_decoded = DecodePcm16(voice_info, dsp_state, samples_to_read - samples_read,
                                              channel, temp_mix_offset);
                break;
            case SampleFormat::Adpcm:
                samples_decoded = DecodeAdpcm(voice_info, dsp_state, samples_to_read - samples_read,
                                              channel, temp_mix_offset);
                break;
            default:
                UNREACHABLE_MSG("Unimplemented sample format={}", in_params.sample_format);
            }

            temp_mix_offset += samples_decoded;
            samples_read += samples_decoded;
            dsp_state.offset += samples_decoded;
            dsp_state.played_sample_count += samples_decoded;

            if (dsp_state.offset >=
                    (wave_buffer.end_sample_offset - wave_buffer.start_sample_offset) ||
                samples_decoded == 0) {
                // Reset our sample offset
                dsp_state.offset = 0;
                if (wave_buffer.is_looping) {
                    if (samples_decoded == 0) {
                        // End of our buffer
                        is_buffer_completed = true;
                        break;
                    }

                    if (in_params.behavior_flags.is_played_samples_reset_at_loop_point.Value()) {
                        dsp_state.played_sample_count = 0;
                    }
                } else {
                    if (in_params.sample_format == SampleFormat::Adpcm) {
                        // TODO(ogniK): Remove this when ADPCM streaming implemented
                        dsp_state.adpcm_samples.clear();
                    }

                    // Update our wave buffer states
                    dsp_state.is_wave_buffer_valid[dsp_state.wave_buffer_index] = false;
                    dsp_state.wave_buffer_consumed++;
                    dsp_state.wave_buffer_index =
                        (dsp_state.wave_buffer_index + 1) % AudioCommon::MAX_WAVE_BUFFERS;
                    if (wave_buffer.end_of_stream) {
                        dsp_state.played_sample_count = 0;
                    }
                }
            }
        }

        if (in_params.behavior_flags.is_pitch_and_src_skipped.Value()) {
            // No need to resample
            memcpy(output, sample_buffer.data(), samples_read * sizeof(s32));
        } else {
            std::memset(sample_buffer.data() + temp_mix_offset, 0,
                        sizeof(s32) * (samples_to_read - samples_read));
            AudioCore::Resample(output, sample_buffer.data(), resample_rate, dsp_state.fraction,
                                samples_to_output);
            // Resample
            for (std::size_t i = 0; i < AudioCommon::MAX_SAMPLE_HISTORY; i++) {
                dsp_state.sample_history[i] = sample_buffer[samples_to_read + i];
            }
        }
        output += samples_to_output;
        samples_remaining -= samples_to_output;
    }
}

} // namespace AudioCore
