// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "audio_core/adsp/adsp.h"
#include "core/core.h"

namespace AudioCore::ADSP {

ADSP::ADSP(Core::System& system, Sink::Sink& sink) {
    audio_renderer =
        std::make_unique<AudioRenderer::AudioRenderer>(system, system.ApplicationMemory(), sink);
}

AudioRenderer::AudioRenderer& ADSP::AudioRenderer() {
    return *audio_renderer.get();
}

} // namespace AudioCore::ADSP
