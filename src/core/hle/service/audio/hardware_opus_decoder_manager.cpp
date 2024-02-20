// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/audio/hardware_opus_decoder.h"
#include "core/hle/service/audio/hardware_opus_decoder_manager.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::Audio {

using namespace AudioCore::OpusDecoder;

void IHardwareOpusDecoderManager::OpenHardwareOpusDecoder(HLERequestContext& ctx) {
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

void IHardwareOpusDecoderManager::GetWorkBufferSize(HLERequestContext& ctx) {
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

void IHardwareOpusDecoderManager::OpenHardwareOpusDecoderForMultiStream(HLERequestContext& ctx) {
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

void IHardwareOpusDecoderManager::GetWorkBufferSizeForMultiStream(HLERequestContext& ctx) {
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

void IHardwareOpusDecoderManager::OpenHardwareOpusDecoderEx(HLERequestContext& ctx) {
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

void IHardwareOpusDecoderManager::GetWorkBufferSizeEx(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    auto params = rp.PopRaw<OpusParametersEx>();

    u64 size{};
    auto result = impl.GetWorkBufferSizeEx(params, size);

    LOG_DEBUG(Service_Audio, "size 0x{:X}", size);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push(size);
}

void IHardwareOpusDecoderManager::OpenHardwareOpusDecoderForMultiStreamEx(HLERequestContext& ctx) {
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

void IHardwareOpusDecoderManager::GetWorkBufferSizeForMultiStreamEx(HLERequestContext& ctx) {
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

void IHardwareOpusDecoderManager::GetWorkBufferSizeExEx(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    auto params = rp.PopRaw<OpusParametersEx>();

    u64 size{};
    auto result = impl.GetWorkBufferSizeExEx(params, size);

    LOG_DEBUG(Service_Audio, "size 0x{:X}", size);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push(size);
}

void IHardwareOpusDecoderManager::GetWorkBufferSizeForMultiStreamExEx(HLERequestContext& ctx) {
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

IHardwareOpusDecoderManager::IHardwareOpusDecoderManager(Core::System& system_)
    : ServiceFramework{system_, "hwopus"}, system{system_}, impl{system} {
    static const FunctionInfo functions[] = {
        {0, &IHardwareOpusDecoderManager::OpenHardwareOpusDecoder, "OpenHardwareOpusDecoder"},
        {1, &IHardwareOpusDecoderManager::GetWorkBufferSize, "GetWorkBufferSize"},
        {2, &IHardwareOpusDecoderManager::OpenHardwareOpusDecoderForMultiStream,
         "OpenOpusDecoderForMultiStream"},
        {3, &IHardwareOpusDecoderManager::GetWorkBufferSizeForMultiStream,
         "GetWorkBufferSizeForMultiStream"},
        {4, &IHardwareOpusDecoderManager::OpenHardwareOpusDecoderEx, "OpenHardwareOpusDecoderEx"},
        {5, &IHardwareOpusDecoderManager::GetWorkBufferSizeEx, "GetWorkBufferSizeEx"},
        {6, &IHardwareOpusDecoderManager::OpenHardwareOpusDecoderForMultiStreamEx,
         "OpenHardwareOpusDecoderForMultiStreamEx"},
        {7, &IHardwareOpusDecoderManager::GetWorkBufferSizeForMultiStreamEx,
         "GetWorkBufferSizeForMultiStreamEx"},
        {8, &IHardwareOpusDecoderManager::GetWorkBufferSizeExEx, "GetWorkBufferSizeExEx"},
        {9, &IHardwareOpusDecoderManager::GetWorkBufferSizeForMultiStreamExEx,
         "GetWorkBufferSizeForMultiStreamExEx"},
    };
    RegisterHandlers(functions);
}

IHardwareOpusDecoderManager::~IHardwareOpusDecoderManager() = default;

} // namespace Service::Audio
