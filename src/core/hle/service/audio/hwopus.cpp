// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <vector>

#include "audio_core/opus/decoder.h"
#include "audio_core/opus/parameters.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/scratch_buffer.h"
#include "core/core.h"
#include "core/hle/service/audio/hwopus.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::Audio {
using namespace AudioCore::OpusDecoder;

class IHardwareOpusDecoder final : public ServiceFramework<IHardwareOpusDecoder> {
public:
    explicit IHardwareOpusDecoder(Core::System& system_, HardwareOpus& hardware_opus)
        : ServiceFramework{system_, "IHardwareOpusDecoder"},
          impl{std::make_unique<AudioCore::OpusDecoder::OpusDecoder>(system_, hardware_opus)} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IHardwareOpusDecoder::DecodeInterleavedOld, "DecodeInterleavedOld"},
            {1, &IHardwareOpusDecoder::SetContext, "SetContext"},
            {2, &IHardwareOpusDecoder::DecodeInterleavedForMultiStreamOld, "DecodeInterleavedForMultiStreamOld"},
            {3, &IHardwareOpusDecoder::SetContextForMultiStream, "SetContextForMultiStream"},
            {4, &IHardwareOpusDecoder::DecodeInterleavedWithPerfOld, "DecodeInterleavedWithPerfOld"},
            {5, &IHardwareOpusDecoder::DecodeInterleavedForMultiStreamWithPerfOld, "DecodeInterleavedForMultiStreamWithPerfOld"},
            {6, &IHardwareOpusDecoder::DecodeInterleavedWithPerfAndResetOld, "DecodeInterleavedWithPerfAndResetOld"},
            {7, &IHardwareOpusDecoder::DecodeInterleavedForMultiStreamWithPerfAndResetOld, "DecodeInterleavedForMultiStreamWithPerfAndResetOld"},
            {8, &IHardwareOpusDecoder::DecodeInterleaved, "DecodeInterleaved"},
            {9, &IHardwareOpusDecoder::DecodeInterleavedForMultiStream, "DecodeInterleavedForMultiStream"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

    Result Initialize(OpusParametersEx& params, Kernel::KTransferMemory* transfer_memory,
                      u64 transfer_memory_size) {
        return impl->Initialize(params, transfer_memory, transfer_memory_size);
    }

    Result Initialize(OpusMultiStreamParametersEx& params, Kernel::KTransferMemory* transfer_memory,
                      u64 transfer_memory_size) {
        return impl->Initialize(params, transfer_memory, transfer_memory_size);
    }

private:
    void DecodeInterleavedOld(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};

        auto input_data{ctx.ReadBuffer(0)};
        output_data.resize_destructive(ctx.GetWriteBufferSize());

        u32 size{};
        u32 sample_count{};
        auto result =
            impl->DecodeInterleaved(&size, nullptr, &sample_count, input_data, output_data, false);

        LOG_DEBUG(Service_Audio, "bytes read 0x{:X} samples generated {}", size, sample_count);

        ctx.WriteBuffer(output_data);

        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(result);
        rb.Push(size);
        rb.Push(sample_count);
    }

    void SetContext(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};

        LOG_DEBUG(Service_Audio, "called");

        auto input_data{ctx.ReadBuffer(0)};
        auto result = impl->SetContext(input_data);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
    }

    void DecodeInterleavedForMultiStreamOld(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};

        auto input_data{ctx.ReadBuffer(0)};
        output_data.resize_destructive(ctx.GetWriteBufferSize());

        u32 size{};
        u32 sample_count{};
        auto result = impl->DecodeInterleavedForMultiStream(&size, nullptr, &sample_count,
                                                            input_data, output_data, false);

        LOG_DEBUG(Service_Audio, "bytes read 0x{:X} samples generated {}", size, sample_count);

        ctx.WriteBuffer(output_data);

        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(result);
        rb.Push(size);
        rb.Push(sample_count);
    }

    void SetContextForMultiStream(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};

        LOG_DEBUG(Service_Audio, "called");

        auto input_data{ctx.ReadBuffer(0)};
        auto result = impl->SetContext(input_data);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
    }

    void DecodeInterleavedWithPerfOld(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};

        auto input_data{ctx.ReadBuffer(0)};
        output_data.resize_destructive(ctx.GetWriteBufferSize());

        u32 size{};
        u32 sample_count{};
        u64 time_taken{};
        auto result = impl->DecodeInterleaved(&size, &time_taken, &sample_count, input_data,
                                              output_data, false);

        LOG_DEBUG(Service_Audio, "bytes read 0x{:X} samples generated {} time taken {}", size,
                  sample_count, time_taken);

        ctx.WriteBuffer(output_data);

        IPC::ResponseBuilder rb{ctx, 6};
        rb.Push(result);
        rb.Push(size);
        rb.Push(sample_count);
        rb.Push(time_taken);
    }

    void DecodeInterleavedForMultiStreamWithPerfOld(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};

        auto input_data{ctx.ReadBuffer(0)};
        output_data.resize_destructive(ctx.GetWriteBufferSize());

        u32 size{};
        u32 sample_count{};
        u64 time_taken{};
        auto result = impl->DecodeInterleavedForMultiStream(&size, &time_taken, &sample_count,
                                                            input_data, output_data, false);

        LOG_DEBUG(Service_Audio, "bytes read 0x{:X} samples generated {} time taken {}", size,
                  sample_count, time_taken);

        ctx.WriteBuffer(output_data);

        IPC::ResponseBuilder rb{ctx, 6};
        rb.Push(result);
        rb.Push(size);
        rb.Push(sample_count);
        rb.Push(time_taken);
    }

    void DecodeInterleavedWithPerfAndResetOld(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};

        auto reset{rp.Pop<bool>()};

        auto input_data{ctx.ReadBuffer(0)};
        output_data.resize_destructive(ctx.GetWriteBufferSize());

        u32 size{};
        u32 sample_count{};
        u64 time_taken{};
        auto result = impl->DecodeInterleaved(&size, &time_taken, &sample_count, input_data,
                                              output_data, reset);

        LOG_DEBUG(Service_Audio, "reset {} bytes read 0x{:X} samples generated {} time taken {}",
                  reset, size, sample_count, time_taken);

        ctx.WriteBuffer(output_data);

        IPC::ResponseBuilder rb{ctx, 6};
        rb.Push(result);
        rb.Push(size);
        rb.Push(sample_count);
        rb.Push(time_taken);
    }

    void DecodeInterleavedForMultiStreamWithPerfAndResetOld(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};

        auto reset{rp.Pop<bool>()};

        auto input_data{ctx.ReadBuffer(0)};
        output_data.resize_destructive(ctx.GetWriteBufferSize());

        u32 size{};
        u32 sample_count{};
        u64 time_taken{};
        auto result = impl->DecodeInterleavedForMultiStream(&size, &time_taken, &sample_count,
                                                            input_data, output_data, reset);

        LOG_DEBUG(Service_Audio, "reset {} bytes read 0x{:X} samples generated {} time taken {}",
                  reset, size, sample_count, time_taken);

        ctx.WriteBuffer(output_data);

        IPC::ResponseBuilder rb{ctx, 6};
        rb.Push(result);
        rb.Push(size);
        rb.Push(sample_count);
        rb.Push(time_taken);
    }

    void DecodeInterleaved(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};

        auto reset{rp.Pop<bool>()};

        auto input_data{ctx.ReadBuffer(0)};
        output_data.resize_destructive(ctx.GetWriteBufferSize());

        u32 size{};
        u32 sample_count{};
        u64 time_taken{};
        auto result = impl->DecodeInterleaved(&size, &time_taken, &sample_count, input_data,
                                              output_data, reset);

        LOG_DEBUG(Service_Audio, "reset {} bytes read 0x{:X} samples generated {} time taken {}",
                  reset, size, sample_count, time_taken);

        ctx.WriteBuffer(output_data);

        IPC::ResponseBuilder rb{ctx, 6};
        rb.Push(result);
        rb.Push(size);
        rb.Push(sample_count);
        rb.Push(time_taken);
    }

    void DecodeInterleavedForMultiStream(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};

        auto reset{rp.Pop<bool>()};

        auto input_data{ctx.ReadBuffer(0)};
        output_data.resize_destructive(ctx.GetWriteBufferSize());

        u32 size{};
        u32 sample_count{};
        u64 time_taken{};
        auto result = impl->DecodeInterleavedForMultiStream(&size, &time_taken, &sample_count,
                                                            input_data, output_data, reset);

        LOG_DEBUG(Service_Audio, "reset {} bytes read 0x{:X} samples generated {} time taken {}",
                  reset, size, sample_count, time_taken);

        ctx.WriteBuffer(output_data);

        IPC::ResponseBuilder rb{ctx, 6};
        rb.Push(result);
        rb.Push(size);
        rb.Push(sample_count);
        rb.Push(time_taken);
    }

    std::unique_ptr<AudioCore::OpusDecoder::OpusDecoder> impl;
    Common::ScratchBuffer<u8> output_data;
};

void HwOpus::OpenHardwareOpusDecoder(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    auto params = rp.PopRaw<OpusParameters>();
    auto transfer_memory_size{rp.Pop<u32>()};
    auto transfer_memory_handle{ctx.GetCopyHandle(0)};
    auto transfer_memory{ctx.GetObjectFromHandle<Kernel::KTransferMemory>(transfer_memory_handle)};

    LOG_DEBUG(Service_Audio, "sample_rate {} channel_count {} transfer_memory_size 0x{:X}",
              params.sample_rate, params.channel_count, transfer_memory_size);

    auto decoder{std::make_shared<IHardwareOpusDecoder>(system, impl.GetHardwareOpus())};

    OpusParametersEx ex{
        .sample_rate = params.sample_rate,
        .channel_count = params.channel_count,
        .use_large_frame_size = false,
    };
    auto result = decoder->Initialize(ex, transfer_memory.GetPointerUnsafe(), transfer_memory_size);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(result);
    rb.PushIpcInterface(decoder);
}

void HwOpus::GetWorkBufferSize(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    auto params = rp.PopRaw<OpusParameters>();

    u64 size{};
    auto result = impl.GetWorkBufferSize(params, size);

    LOG_DEBUG(Service_Audio, "sample_rate {} channel_count {} -- returned size 0x{:X}",
              params.sample_rate, params.channel_count, size);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push(size);
}

void HwOpus::OpenHardwareOpusDecoderForMultiStream(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    auto input{ctx.ReadBuffer()};
    OpusMultiStreamParameters params;
    std::memcpy(&params, input.data(), sizeof(OpusMultiStreamParameters));

    auto transfer_memory_size{rp.Pop<u32>()};
    auto transfer_memory_handle{ctx.GetCopyHandle(0)};
    auto transfer_memory{ctx.GetObjectFromHandle<Kernel::KTransferMemory>(transfer_memory_handle)};

    LOG_DEBUG(Service_Audio,
              "sample_rate {} channel_count {} total_stream_count {} stereo_stream_count {} "
              "transfer_memory_size 0x{:X}",
              params.sample_rate, params.channel_count, params.total_stream_count,
              params.stereo_stream_count, transfer_memory_size);

    auto decoder{std::make_shared<IHardwareOpusDecoder>(system, impl.GetHardwareOpus())};

    OpusMultiStreamParametersEx ex{
        .sample_rate = params.sample_rate,
        .channel_count = params.channel_count,
        .total_stream_count = params.total_stream_count,
        .stereo_stream_count = params.stereo_stream_count,
        .use_large_frame_size = false,
        .mappings{},
    };
    std::memcpy(ex.mappings.data(), params.mappings.data(), sizeof(params.mappings));
    auto result = decoder->Initialize(ex, transfer_memory.GetPointerUnsafe(), transfer_memory_size);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(result);
    rb.PushIpcInterface(decoder);
}

void HwOpus::GetWorkBufferSizeForMultiStream(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    auto input{ctx.ReadBuffer()};
    OpusMultiStreamParameters params;
    std::memcpy(&params, input.data(), sizeof(OpusMultiStreamParameters));

    u64 size{};
    auto result = impl.GetWorkBufferSizeForMultiStream(params, size);

    LOG_DEBUG(Service_Audio, "size 0x{:X}", size);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push(size);
}

void HwOpus::OpenHardwareOpusDecoderEx(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    auto params = rp.PopRaw<OpusParametersEx>();
    auto transfer_memory_size{rp.Pop<u32>()};
    auto transfer_memory_handle{ctx.GetCopyHandle(0)};
    auto transfer_memory{ctx.GetObjectFromHandle<Kernel::KTransferMemory>(transfer_memory_handle)};

    LOG_DEBUG(Service_Audio, "sample_rate {} channel_count {} transfer_memory_size 0x{:X}",
              params.sample_rate, params.channel_count, transfer_memory_size);

    auto decoder{std::make_shared<IHardwareOpusDecoder>(system, impl.GetHardwareOpus())};

    auto result =
        decoder->Initialize(params, transfer_memory.GetPointerUnsafe(), transfer_memory_size);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(result);
    rb.PushIpcInterface(decoder);
}

void HwOpus::GetWorkBufferSizeEx(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    auto params = rp.PopRaw<OpusParametersEx>();

    u64 size{};
    auto result = impl.GetWorkBufferSizeEx(params, size);

    LOG_DEBUG(Service_Audio, "size 0x{:X}", size);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push(size);
}

void HwOpus::OpenHardwareOpusDecoderForMultiStreamEx(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    auto input{ctx.ReadBuffer()};
    OpusMultiStreamParametersEx params;
    std::memcpy(&params, input.data(), sizeof(OpusMultiStreamParametersEx));

    auto transfer_memory_size{rp.Pop<u32>()};
    auto transfer_memory_handle{ctx.GetCopyHandle(0)};
    auto transfer_memory{ctx.GetObjectFromHandle<Kernel::KTransferMemory>(transfer_memory_handle)};

    LOG_DEBUG(Service_Audio,
              "sample_rate {} channel_count {} total_stream_count {} stereo_stream_count {} "
              "use_large_frame_size {}"
              "transfer_memory_size 0x{:X}",
              params.sample_rate, params.channel_count, params.total_stream_count,
              params.stereo_stream_count, params.use_large_frame_size, transfer_memory_size);

    auto decoder{std::make_shared<IHardwareOpusDecoder>(system, impl.GetHardwareOpus())};

    auto result =
        decoder->Initialize(params, transfer_memory.GetPointerUnsafe(), transfer_memory_size);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(result);
    rb.PushIpcInterface(decoder);
}

void HwOpus::GetWorkBufferSizeForMultiStreamEx(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    auto input{ctx.ReadBuffer()};
    OpusMultiStreamParametersEx params;
    std::memcpy(&params, input.data(), sizeof(OpusMultiStreamParametersEx));

    u64 size{};
    auto result = impl.GetWorkBufferSizeForMultiStreamEx(params, size);

    LOG_DEBUG(Service_Audio,
              "sample_rate {} channel_count {} total_stream_count {} stereo_stream_count {} "
              "use_large_frame_size {} -- returned size 0x{:X}",
              params.sample_rate, params.channel_count, params.total_stream_count,
              params.stereo_stream_count, params.use_large_frame_size, size);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push(size);
}

void HwOpus::GetWorkBufferSizeExEx(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    auto params = rp.PopRaw<OpusParametersEx>();

    u64 size{};
    auto result = impl.GetWorkBufferSizeExEx(params, size);

    LOG_DEBUG(Service_Audio, "size 0x{:X}", size);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push(size);
}

void HwOpus::GetWorkBufferSizeForMultiStreamExEx(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    auto input{ctx.ReadBuffer()};
    OpusMultiStreamParametersEx params;
    std::memcpy(&params, input.data(), sizeof(OpusMultiStreamParametersEx));

    u64 size{};
    auto result = impl.GetWorkBufferSizeForMultiStreamExEx(params, size);

    LOG_DEBUG(Service_Audio, "size 0x{:X}", size);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push(size);
}

HwOpus::HwOpus(Core::System& system_)
    : ServiceFramework{system_, "hwopus"}, system{system_}, impl{system} {
    static const FunctionInfo functions[] = {
        {0, &HwOpus::OpenHardwareOpusDecoder, "OpenHardwareOpusDecoder"},
        {1, &HwOpus::GetWorkBufferSize, "GetWorkBufferSize"},
        {2, &HwOpus::OpenHardwareOpusDecoderForMultiStream, "OpenOpusDecoderForMultiStream"},
        {3, &HwOpus::GetWorkBufferSizeForMultiStream, "GetWorkBufferSizeForMultiStream"},
        {4, &HwOpus::OpenHardwareOpusDecoderEx, "OpenHardwareOpusDecoderEx"},
        {5, &HwOpus::GetWorkBufferSizeEx, "GetWorkBufferSizeEx"},
        {6, &HwOpus::OpenHardwareOpusDecoderForMultiStreamEx,
         "OpenHardwareOpusDecoderForMultiStreamEx"},
        {7, &HwOpus::GetWorkBufferSizeForMultiStreamEx, "GetWorkBufferSizeForMultiStreamEx"},
        {8, &HwOpus::GetWorkBufferSizeExEx, "GetWorkBufferSizeExEx"},
        {9, &HwOpus::GetWorkBufferSizeForMultiStreamExEx, "GetWorkBufferSizeForMultiStreamExEx"},
    };
    RegisterHandlers(functions);
}

HwOpus::~HwOpus() = default;

} // namespace Service::Audio
