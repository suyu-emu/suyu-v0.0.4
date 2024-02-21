// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "audio_core/opus/decoder.h"
#include "core/hle/service/service.h"

namespace Service::Audio {

class IHardwareOpusDecoder final : public ServiceFramework<IHardwareOpusDecoder> {
public:
    explicit IHardwareOpusDecoder(Core::System& system_,
                                  AudioCore::OpusDecoder::HardwareOpus& hardware_opus);
    ~IHardwareOpusDecoder() override;

    Result Initialize(const AudioCore::OpusDecoder::OpusParametersEx& params,
                      Kernel::KTransferMemory* transfer_memory, u64 transfer_memory_size);
    Result Initialize(const AudioCore::OpusDecoder::OpusMultiStreamParametersEx& params,
                      Kernel::KTransferMemory* transfer_memory, u64 transfer_memory_size);

private:
    void DecodeInterleavedOld(HLERequestContext& ctx);
    void SetContext(HLERequestContext& ctx);
    void DecodeInterleavedForMultiStreamOld(HLERequestContext& ctx);
    void SetContextForMultiStream(HLERequestContext& ctx);
    void DecodeInterleavedWithPerfOld(HLERequestContext& ctx);
    void DecodeInterleavedForMultiStreamWithPerfOld(HLERequestContext& ctx);
    void DecodeInterleavedWithPerfAndResetOld(HLERequestContext& ctx);
    void DecodeInterleavedForMultiStreamWithPerfAndResetOld(HLERequestContext& ctx);
    void DecodeInterleaved(HLERequestContext& ctx);
    void DecodeInterleavedForMultiStream(HLERequestContext& ctx);

    std::unique_ptr<AudioCore::OpusDecoder::OpusDecoder> impl;
    Common::ScratchBuffer<u8> output_data;
};

} // namespace Service::Audio
