// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <cstring>
#include <memory>
#include <vector>

#include <opus.h>

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/service/audio/hwopus.h"

namespace Service::Audio {
namespace {
struct OpusDeleter {
    void operator()(void* ptr) const {
        operator delete(ptr);
    }
};

using OpusDecoderPtr = std::unique_ptr<OpusDecoder, OpusDeleter>;

struct OpusPacketHeader {
    // Packet size in bytes.
    u32_be size;
    // Indicates the final range of the codec's entropy coder.
    u32_be final_range;
};
static_assert(sizeof(OpusPacketHeader) == 0x8, "OpusHeader is an invalid size");

class OpusDecoderStateBase {
public:
    /// Describes extra behavior that may be asked of the decoding context.
    enum class ExtraBehavior {
        /// No extra behavior.
        None,

        /// Resets the decoder context back to a freshly initialized state.
        ResetContext,
    };

    enum class PerfTime {
        Disabled,
        Enabled,
    };

    virtual ~OpusDecoderStateBase() = default;

    // Decodes interleaved Opus packets. Optionally allows reporting time taken to
    // perform the decoding, as well as any relevant extra behavior.
    virtual void DecodeInterleaved(Kernel::HLERequestContext& ctx, PerfTime perf_time,
                                   ExtraBehavior extra_behavior) = 0;
};

// Represents the decoder state for a non-multistream decoder.
class OpusDecoderState final : public OpusDecoderStateBase {
public:
    explicit OpusDecoderState(OpusDecoderPtr decoder, u32 sample_rate, u32 channel_count)
        : decoder{std::move(decoder)}, sample_rate{sample_rate}, channel_count{channel_count} {}

    void DecodeInterleaved(Kernel::HLERequestContext& ctx, PerfTime perf_time,
                           ExtraBehavior extra_behavior) override {
        if (perf_time == PerfTime::Disabled) {
            DecodeInterleavedHelper(ctx, nullptr, extra_behavior);
        } else {
            u64 performance = 0;
            DecodeInterleavedHelper(ctx, &performance, extra_behavior);
        }
    }

private:
    void DecodeInterleavedHelper(Kernel::HLERequestContext& ctx, u64* performance,
                                 ExtraBehavior extra_behavior) {
        u32 consumed = 0;
        u32 sample_count = 0;
        std::vector<opus_int16> samples(ctx.GetWriteBufferSize() / sizeof(opus_int16));

        if (extra_behavior == ExtraBehavior::ResetContext) {
            ResetDecoderContext();
        }

        if (!DecodeOpusData(consumed, sample_count, ctx.ReadBuffer(), samples, performance)) {
            LOG_ERROR(Audio, "Failed to decode opus data");
            IPC::ResponseBuilder rb{ctx, 2};
            // TODO(ogniK): Use correct error code
            rb.Push(ResultCode(-1));
            return;
        }

        const u32 param_size = performance != nullptr ? 6 : 4;
        IPC::ResponseBuilder rb{ctx, param_size};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(consumed);
        rb.Push<u32>(sample_count);
        if (performance) {
            rb.Push<u64>(*performance);
        }
        ctx.WriteBuffer(samples.data(), samples.size() * sizeof(s16));
    }

    bool DecodeOpusData(u32& consumed, u32& sample_count, const std::vector<u8>& input,
                        std::vector<opus_int16>& output, u64* out_performance_time) const {
        const auto start_time = std::chrono::high_resolution_clock::now();
        const std::size_t raw_output_sz = output.size() * sizeof(opus_int16);
        if (sizeof(OpusPacketHeader) > input.size()) {
            LOG_ERROR(Audio, "Input is smaller than the header size, header_sz={}, input_sz={}",
                      sizeof(OpusPacketHeader), input.size());
            return false;
        }

        OpusPacketHeader hdr{};
        std::memcpy(&hdr, input.data(), sizeof(OpusPacketHeader));
        if (sizeof(OpusPacketHeader) + static_cast<u32>(hdr.size) > input.size()) {
            LOG_ERROR(Audio, "Input does not fit in the opus header size. data_sz={}, input_sz={}",
                      sizeof(OpusPacketHeader) + static_cast<u32>(hdr.size), input.size());
            return false;
        }

        const auto frame = input.data() + sizeof(OpusPacketHeader);
        const auto decoded_sample_count = opus_packet_get_nb_samples(
            frame, static_cast<opus_int32>(input.size() - sizeof(OpusPacketHeader)),
            static_cast<opus_int32>(sample_rate));
        if (decoded_sample_count * channel_count * sizeof(u16) > raw_output_sz) {
            LOG_ERROR(
                Audio,
                "Decoded data does not fit into the output data, decoded_sz={}, raw_output_sz={}",
                decoded_sample_count * channel_count * sizeof(u16), raw_output_sz);
            return false;
        }

        const int frame_size = (static_cast<int>(raw_output_sz / sizeof(s16) / channel_count));
        const auto out_sample_count =
            opus_decode(decoder.get(), frame, hdr.size, output.data(), frame_size, 0);
        if (out_sample_count < 0) {
            LOG_ERROR(Audio,
                      "Incorrect sample count received from opus_decode, "
                      "output_sample_count={}, frame_size={}, data_sz_from_hdr={}",
                      out_sample_count, frame_size, static_cast<u32>(hdr.size));
            return false;
        }

        const auto end_time = std::chrono::high_resolution_clock::now() - start_time;
        sample_count = out_sample_count;
        consumed = static_cast<u32>(sizeof(OpusPacketHeader) + hdr.size);
        if (out_performance_time != nullptr) {
            *out_performance_time =
                std::chrono::duration_cast<std::chrono::milliseconds>(end_time).count();
        }

        return true;
    }

    void ResetDecoderContext() {
        ASSERT(decoder != nullptr);

        opus_decoder_ctl(decoder.get(), OPUS_RESET_STATE);
    }

    OpusDecoderPtr decoder;
    u32 sample_rate;
    u32 channel_count;
};

class IHardwareOpusDecoderManager final : public ServiceFramework<IHardwareOpusDecoderManager> {
public:
    explicit IHardwareOpusDecoderManager(std::unique_ptr<OpusDecoderStateBase> decoder_state)
        : ServiceFramework("IHardwareOpusDecoderManager"), decoder_state{std::move(decoder_state)} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IHardwareOpusDecoderManager::DecodeInterleavedOld, "DecodeInterleavedOld"},
            {1, nullptr, "SetContext"},
            {2, nullptr, "DecodeInterleavedForMultiStreamOld"},
            {3, nullptr, "SetContextForMultiStream"},
            {4, &IHardwareOpusDecoderManager::DecodeInterleavedWithPerfOld, "DecodeInterleavedWithPerfOld"},
            {5, nullptr, "DecodeInterleavedForMultiStreamWithPerfOld"},
            {6, &IHardwareOpusDecoderManager::DecodeInterleaved, "DecodeInterleaved"},
            {7, nullptr, "DecodeInterleavedForMultiStream"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void DecodeInterleavedOld(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Audio, "called");

        decoder_state->DecodeInterleaved(ctx, OpusDecoderStateBase::PerfTime::Disabled,
                                         OpusDecoderStateBase::ExtraBehavior::None);
    }

    void DecodeInterleavedWithPerfOld(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Audio, "called");

        decoder_state->DecodeInterleaved(ctx, OpusDecoderStateBase::PerfTime::Enabled,
                                         OpusDecoderStateBase::ExtraBehavior::None);
    }

    void DecodeInterleaved(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Audio, "called");

        IPC::RequestParser rp{ctx};
        const auto extra_behavior = rp.Pop<bool>()
                                        ? OpusDecoderStateBase::ExtraBehavior::ResetContext
                                        : OpusDecoderStateBase::ExtraBehavior::None;

        decoder_state->DecodeInterleaved(ctx, OpusDecoderStateBase::PerfTime::Enabled,
                                         extra_behavior);
    }

    std::unique_ptr<OpusDecoderStateBase> decoder_state;
};

std::size_t WorkerBufferSize(u32 channel_count) {
    ASSERT_MSG(channel_count == 1 || channel_count == 2, "Invalid channel count");
    return opus_decoder_get_size(static_cast<int>(channel_count));
}
} // Anonymous namespace

void HwOpus::GetWorkBufferSize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto sample_rate = rp.Pop<u32>();
    const auto channel_count = rp.Pop<u32>();

    LOG_DEBUG(Audio, "called with sample_rate={}, channel_count={}", sample_rate, channel_count);

    ASSERT_MSG(sample_rate == 48000 || sample_rate == 24000 || sample_rate == 16000 ||
                   sample_rate == 12000 || sample_rate == 8000,
               "Invalid sample rate");
    ASSERT_MSG(channel_count == 1 || channel_count == 2, "Invalid channel count");

    const u32 worker_buffer_sz = static_cast<u32>(WorkerBufferSize(channel_count));
    LOG_DEBUG(Audio, "worker_buffer_sz={}", worker_buffer_sz);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(worker_buffer_sz);
}

void HwOpus::OpenOpusDecoder(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto sample_rate = rp.Pop<u32>();
    const auto channel_count = rp.Pop<u32>();
    const auto buffer_sz = rp.Pop<u32>();

    LOG_DEBUG(Audio, "called sample_rate={}, channel_count={}, buffer_size={}", sample_rate,
              channel_count, buffer_sz);

    ASSERT_MSG(sample_rate == 48000 || sample_rate == 24000 || sample_rate == 16000 ||
                   sample_rate == 12000 || sample_rate == 8000,
               "Invalid sample rate");
    ASSERT_MSG(channel_count == 1 || channel_count == 2, "Invalid channel count");

    const std::size_t worker_sz = WorkerBufferSize(channel_count);
    ASSERT_MSG(buffer_sz >= worker_sz, "Worker buffer too large");

    OpusDecoderPtr decoder{static_cast<OpusDecoder*>(operator new(worker_sz))};
    if (const int err = opus_decoder_init(decoder.get(), sample_rate, channel_count)) {
        LOG_ERROR(Audio, "Failed to init opus decoder with error={}", err);
        IPC::ResponseBuilder rb{ctx, 2};
        // TODO(ogniK): Use correct error code
        rb.Push(ResultCode(-1));
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<IHardwareOpusDecoderManager>(
        std::make_unique<OpusDecoderState>(std::move(decoder), sample_rate, channel_count));
}

HwOpus::HwOpus() : ServiceFramework("hwopus") {
    static const FunctionInfo functions[] = {
        {0, &HwOpus::OpenOpusDecoder, "OpenOpusDecoder"},
        {1, &HwOpus::GetWorkBufferSize, "GetWorkBufferSize"},
        {2, nullptr, "OpenOpusDecoderForMultiStream"},
        {3, nullptr, "GetWorkBufferSizeForMultiStream"},
    };
    RegisterHandlers(functions);
}

HwOpus::~HwOpus() = default;

} // namespace Service::Audio
