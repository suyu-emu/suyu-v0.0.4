// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace AudioCore {
class AudioOut;
}

namespace Kernel {
class HLERequestContext;
}

namespace Service::Audio {

class IAudioOut;

class AudOutU final : public ServiceFramework<AudOutU> {
public:
    AudOutU();
    ~AudOutU() override;

private:
    std::shared_ptr<IAudioOut> audio_out_interface;
    std::unique_ptr<AudioCore::AudioOut> audio_core;

    void ListAudioOutsImpl(Kernel::HLERequestContext& ctx);
    void OpenAudioOutImpl(Kernel::HLERequestContext& ctx);
};

} // namespace Service::Audio
