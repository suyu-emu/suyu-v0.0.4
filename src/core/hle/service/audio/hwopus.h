// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "audio_core/opus/decoder_manager.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Audio {

class HwOpus final : public ServiceFramework<HwOpus> {
public:
    explicit HwOpus(Core::System& system_);
    ~HwOpus() override;

private:
    void OpenHardwareOpusDecoder(HLERequestContext& ctx);
    void GetWorkBufferSize(HLERequestContext& ctx);
    void OpenHardwareOpusDecoderForMultiStream(HLERequestContext& ctx);
    void GetWorkBufferSizeForMultiStream(HLERequestContext& ctx);
    void OpenHardwareOpusDecoderEx(HLERequestContext& ctx);
    void GetWorkBufferSizeEx(HLERequestContext& ctx);
    void OpenHardwareOpusDecoderForMultiStreamEx(HLERequestContext& ctx);
    void GetWorkBufferSizeForMultiStreamEx(HLERequestContext& ctx);
    void GetWorkBufferSizeExEx(HLERequestContext& ctx);
    void GetWorkBufferSizeForMultiStreamExEx(HLERequestContext& ctx);

    Core::System& system;
    AudioCore::OpusDecoder::OpusDecoderManager impl;
};

} // namespace Service::Audio
