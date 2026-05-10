// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "audio_core/renderer/splitter/splitter_destinations_data.h"

namespace AudioCore::Renderer {

namespace {
f32 FixedBiquadCoefficientToFloat(const s16 value) {
    return static_cast<f32>(value) / 16384.0f;
}
} // namespace

SplitterDestinationData::SplitterDestinationData(const s32 id_) : id{id_} {}

void SplitterDestinationData::ClearMixVolume() {
    mix_volumes.fill(0.0f);
    prev_mix_volumes.fill(0.0f);
}

s32 SplitterDestinationData::GetId() const {
    return id;
}

bool SplitterDestinationData::IsConfigured() const {
    return in_use && destination_id != UnusedMixId;
}

s32 SplitterDestinationData::GetMixId() const {
    return destination_id;
}

f32 SplitterDestinationData::GetMixVolume(const u32 index) const {
    if (index >= mix_volumes.size()) {
        LOG_ERROR(Service_Audio, "SplitterDestinationData::GetMixVolume Invalid index {}", index);
        return 0.0f;
    }
    return mix_volumes[index];
}

std::span<f32> SplitterDestinationData::GetMixVolume() {
    return mix_volumes;
}

f32 SplitterDestinationData::GetMixVolumePrev(const u32 index) const {
    if (index >= prev_mix_volumes.size()) {
        LOG_ERROR(Service_Audio, "SplitterDestinationData::GetMixVolumePrev Invalid index {}",
                  index);
        return 0.0f;
    }
    return prev_mix_volumes[index];
}

std::span<f32> SplitterDestinationData::GetMixVolumePrev() {
    return prev_mix_volumes;
}

void SplitterDestinationData::Update(const InParameter& params,
                                     const bool reset_prev_volume_supported) {
    if (params.id != id || params.magic != GetSplitterSendDataMagic()) {
        return;
    }

    destination_id = params.mix_id;
    mix_volumes = params.mix_volumes;
    biquad_filters = {};

    const bool reset_prev_volume{
        reset_prev_volume_supported ? params.reset_prev_volume : (!in_use && params.in_use)};
    if (reset_prev_volume) {
        prev_mix_volumes = mix_volumes;
        need_update = false;
    }

    in_use = params.in_use;
}

void SplitterDestinationData::Update(const InParameterVersion2& params,
                                     const bool reset_prev_volume_supported) {
    if (params.id != id || params.magic != GetSplitterSendDataMagic()) {
        return;
    }

    destination_id = params.mix_id;
    mix_volumes = params.mix_volumes;

    for (size_t i = 0; i < biquad_filters.size(); i++) {
        biquad_filters[i].enabled = params.biquad_filters[i].enabled;
        for (size_t j = 0; j < biquad_filters[i].b.size(); j++) {
            biquad_filters[i].b[j] = FixedBiquadCoefficientToFloat(params.biquad_filters[i].b[j]);
        }
        for (size_t j = 0; j < biquad_filters[i].a.size(); j++) {
            biquad_filters[i].a[j] = FixedBiquadCoefficientToFloat(params.biquad_filters[i].a[j]);
        }
    }

    const bool reset_prev_volume{
        reset_prev_volume_supported ? params.reset_prev_volume : (!in_use && params.in_use)};
    if (reset_prev_volume) {
        prev_mix_volumes = mix_volumes;
        need_update = false;
    }

    in_use = params.in_use;
}

void SplitterDestinationData::Update(const InParameterVersion3& params,
                                     const bool reset_prev_volume_supported) {
    if (params.id != id || params.magic != GetSplitterSendDataMagic()) {
        return;
    }

    destination_id = params.mix_id;
    mix_volumes = params.mix_volumes;
    biquad_filters = params.biquad_filters;

    const bool reset_prev_volume{
        reset_prev_volume_supported ? params.reset_prev_volume : (!in_use && params.in_use)};
    if (reset_prev_volume) {
        prev_mix_volumes = mix_volumes;
        need_update = false;
    }

    in_use = params.in_use;
}

void SplitterDestinationData::MarkAsNeedToUpdateInternalState() {
    need_update = true;
}

void SplitterDestinationData::UpdateInternalState() {
    if (in_use && need_update) {
        prev_mix_volumes = mix_volumes;
    }
    need_update = false;
}

SplitterDestinationData* SplitterDestinationData::GetNext() const {
    return next;
}

void SplitterDestinationData::SetNext(SplitterDestinationData* next_) {
    next = next_;
}

} // namespace AudioCore::Renderer
