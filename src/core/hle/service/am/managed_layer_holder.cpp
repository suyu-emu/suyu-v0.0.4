// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/managed_layer_holder.h"
#include "core/hle/service/nvnflinger/nvnflinger.h"

namespace Service::AM {

ManagedLayerHolder::ManagedLayerHolder() = default;
ManagedLayerHolder::~ManagedLayerHolder() {
    if (!m_nvnflinger) {
        return;
    }

    for (const auto& layer : m_managed_display_layers) {
        m_nvnflinger->DestroyLayer(layer);
    }

    for (const auto& layer : m_managed_display_recording_layers) {
        m_nvnflinger->DestroyLayer(layer);
    }

    m_nvnflinger = nullptr;
}

void ManagedLayerHolder::Initialize(Nvnflinger::Nvnflinger* nvnflinger) {
    m_nvnflinger = nvnflinger;
}

void ManagedLayerHolder::CreateManagedDisplayLayer(u64* out_layer) {
    // TODO(Subv): Find out how AM determines the display to use, for now just
    // create the layer in the Default display.
    const auto display_id = m_nvnflinger->OpenDisplay("Default");
    const auto layer_id = m_nvnflinger->CreateLayer(*display_id);

    m_managed_display_layers.emplace(*layer_id);

    *out_layer = *layer_id;
}

void ManagedLayerHolder::CreateManagedDisplaySeparableLayer(u64* out_layer,
                                                            u64* out_recording_layer) {
    // TODO(Subv): Find out how AM determines the display to use, for now just
    // create the layer in the Default display.
    // This calls nn::vi::CreateRecordingLayer() which creates another layer.
    // Currently we do not support more than 1 layer per display, output 1 layer id for now.
    // Outputting 1 layer id instead of the expected 2 has not been observed to cause any adverse
    // side effects.
    // TODO: Support multiple layers
    const auto display_id = m_nvnflinger->OpenDisplay("Default");
    const auto layer_id = m_nvnflinger->CreateLayer(*display_id);

    m_managed_display_layers.emplace(*layer_id);

    *out_layer = *layer_id;
    *out_recording_layer = 0;
}

} // namespace Service::AM
