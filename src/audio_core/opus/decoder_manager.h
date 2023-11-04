// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "audio_core/opus/hardware_opus.h"
#include "audio_core/opus/parameters.h"
#include "common/common_types.h"
#include "core/hle/service/audio/errors.h"

namespace Core {
class System;
}

namespace AudioCore::OpusDecoder {

class OpusDecoderManager {
public:
    OpusDecoderManager(Core::System& system);

    HardwareOpus& GetHardwareOpus() {
        return hardware_opus;
    }

    Result GetWorkBufferSize(OpusParameters& params, u64& out_size);
    Result GetWorkBufferSizeEx(OpusParametersEx& params, u64& out_size);
    Result GetWorkBufferSizeExEx(OpusParametersEx& params, u64& out_size);
    Result GetWorkBufferSizeForMultiStream(OpusMultiStreamParameters& params, u64& out_size);
    Result GetWorkBufferSizeForMultiStreamEx(OpusMultiStreamParametersEx& params, u64& out_size);
    Result GetWorkBufferSizeForMultiStreamExEx(OpusMultiStreamParametersEx& params, u64& out_size);

private:
    Core::System& system;
    HardwareOpus hardware_opus;
    std::array<u64, MaxChannels> required_workbuffer_sizes{};
};

} // namespace AudioCore::OpusDecoder
