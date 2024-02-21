// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/audio/hardware_opus_decoder.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::Audio {

using namespace AudioCore::OpusDecoder;

IHardwareOpusDecoder::IHardwareOpusDecoder(Core::System& system_, HardwareOpus& hardware_opus)
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

IHardwareOpusDecoder::~IHardwareOpusDecoder() = default;

Result IHardwareOpusDecoder::Initialize(const OpusParametersEx& params,
                                        Kernel::KTransferMemory* transfer_memory,
                                        u64 transfer_memory_size) {
    return impl->Initialize(params, transfer_memory, transfer_memory_size);
}

Result IHardwareOpusDecoder::Initialize(const OpusMultiStreamParametersEx& params,
                                        Kernel::KTransferMemory* transfer_memory,
                                        u64 transfer_memory_size) {
    return impl->Initialize(params, transfer_memory, transfer_memory_size);
}

void IHardwareOpusDecoder::DecodeInterleavedOld(HLERequestContext& ctx) {
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

void IHardwareOpusDecoder::SetContext(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    LOG_DEBUG(Service_Audio, "called");

    auto input_data{ctx.ReadBuffer(0)};
    auto result = impl->SetContext(input_data);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHardwareOpusDecoder::DecodeInterleavedForMultiStreamOld(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    auto input_data{ctx.ReadBuffer(0)};
    output_data.resize_destructive(ctx.GetWriteBufferSize());

    u32 size{};
    u32 sample_count{};
    auto result = impl->DecodeInterleavedForMultiStream(&size, nullptr, &sample_count, input_data,
                                                        output_data, false);

    LOG_DEBUG(Service_Audio, "bytes read 0x{:X} samples generated {}", size, sample_count);

    ctx.WriteBuffer(output_data);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push(size);
    rb.Push(sample_count);
}

void IHardwareOpusDecoder::SetContextForMultiStream(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    LOG_DEBUG(Service_Audio, "called");

    auto input_data{ctx.ReadBuffer(0)};
    auto result = impl->SetContext(input_data);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHardwareOpusDecoder::DecodeInterleavedWithPerfOld(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    auto input_data{ctx.ReadBuffer(0)};
    output_data.resize_destructive(ctx.GetWriteBufferSize());

    u32 size{};
    u32 sample_count{};
    u64 time_taken{};
    auto result =
        impl->DecodeInterleaved(&size, &time_taken, &sample_count, input_data, output_data, false);

    LOG_DEBUG(Service_Audio, "bytes read 0x{:X} samples generated {} time taken {}", size,
              sample_count, time_taken);

    ctx.WriteBuffer(output_data);

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(result);
    rb.Push(size);
    rb.Push(sample_count);
    rb.Push(time_taken);
}

void IHardwareOpusDecoder::DecodeInterleavedForMultiStreamWithPerfOld(HLERequestContext& ctx) {
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

void IHardwareOpusDecoder::DecodeInterleavedWithPerfAndResetOld(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    auto reset{rp.Pop<bool>()};

    auto input_data{ctx.ReadBuffer(0)};
    output_data.resize_destructive(ctx.GetWriteBufferSize());

    u32 size{};
    u32 sample_count{};
    u64 time_taken{};
    auto result =
        impl->DecodeInterleaved(&size, &time_taken, &sample_count, input_data, output_data, reset);

    LOG_DEBUG(Service_Audio, "reset {} bytes read 0x{:X} samples generated {} time taken {}", reset,
              size, sample_count, time_taken);

    ctx.WriteBuffer(output_data);

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(result);
    rb.Push(size);
    rb.Push(sample_count);
    rb.Push(time_taken);
}

void IHardwareOpusDecoder::DecodeInterleavedForMultiStreamWithPerfAndResetOld(
    HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    auto reset{rp.Pop<bool>()};

    auto input_data{ctx.ReadBuffer(0)};
    output_data.resize_destructive(ctx.GetWriteBufferSize());

    u32 size{};
    u32 sample_count{};
    u64 time_taken{};
    auto result = impl->DecodeInterleavedForMultiStream(&size, &time_taken, &sample_count,
                                                        input_data, output_data, reset);

    LOG_DEBUG(Service_Audio, "reset {} bytes read 0x{:X} samples generated {} time taken {}", reset,
              size, sample_count, time_taken);

    ctx.WriteBuffer(output_data);

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(result);
    rb.Push(size);
    rb.Push(sample_count);
    rb.Push(time_taken);
}

void IHardwareOpusDecoder::DecodeInterleaved(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    auto reset{rp.Pop<bool>()};

    auto input_data{ctx.ReadBuffer(0)};
    output_data.resize_destructive(ctx.GetWriteBufferSize());

    u32 size{};
    u32 sample_count{};
    u64 time_taken{};
    auto result =
        impl->DecodeInterleaved(&size, &time_taken, &sample_count, input_data, output_data, reset);

    LOG_DEBUG(Service_Audio, "reset {} bytes read 0x{:X} samples generated {} time taken {}", reset,
              size, sample_count, time_taken);

    ctx.WriteBuffer(output_data);

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(result);
    rb.Push(size);
    rb.Push(sample_count);
    rb.Push(time_taken);
}

void IHardwareOpusDecoder::DecodeInterleavedForMultiStream(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    auto reset{rp.Pop<bool>()};

    auto input_data{ctx.ReadBuffer(0)};
    output_data.resize_destructive(ctx.GetWriteBufferSize());

    u32 size{};
    u32 sample_count{};
    u64 time_taken{};
    auto result = impl->DecodeInterleavedForMultiStream(&size, &time_taken, &sample_count,
                                                        input_data, output_data, reset);

    LOG_DEBUG(Service_Audio, "reset {} bytes read 0x{:X} samples generated {} time taken {}", reset,
              size, sample_count, time_taken);

    ctx.WriteBuffer(output_data);

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(result);
    rb.Push(size);
    rb.Push(sample_count);
    rb.Push(time_taken);
}

} // namespace Service::Audio
