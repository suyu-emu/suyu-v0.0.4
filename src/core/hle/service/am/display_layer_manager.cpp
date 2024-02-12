// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/display_layer_manager.h"
#include "core/hle/service/nvnflinger/fb_share_buffer_manager.h"
#include "core/hle/service/nvnflinger/nvnflinger.h"
#include "core/hle/service/vi/vi_results.h"

namespace Service::AM {

DisplayLayerManager::DisplayLayerManager() = default;
DisplayLayerManager::~DisplayLayerManager() {
    this->Finalize();
}

void DisplayLayerManager::Initialize(Nvnflinger::Nvnflinger* nvnflinger, Kernel::KProcess* process,
                                     AppletId applet_id, LibraryAppletMode mode) {
    m_process = process;
    m_nvnflinger = nvnflinger;
    m_system_shared_buffer_id = 0;
    m_system_shared_layer_id = 0;
    m_applet_id = applet_id;
    m_buffer_sharing_enabled = false;
    m_blending_enabled = mode == LibraryAppletMode::PartialForeground ||
                         mode == LibraryAppletMode::PartialForegroundIndirectDisplay;
}

void DisplayLayerManager::Finalize() {
    if (!m_nvnflinger) {
        return;
    }

    // Clean up managed layers.
    for (const auto& layer : m_managed_display_layers) {
        m_nvnflinger->DestroyLayer(layer);
    }

    for (const auto& layer : m_managed_display_recording_layers) {
        m_nvnflinger->DestroyLayer(layer);
    }

    // Clean up shared layers.
    if (m_buffer_sharing_enabled) {
        m_nvnflinger->GetSystemBufferManager().Finalize(m_process);
    }

    m_nvnflinger = nullptr;
}

Result DisplayLayerManager::CreateManagedDisplayLayer(u64* out_layer) {
    R_UNLESS(m_nvnflinger != nullptr, VI::ResultOperationFailed);

    // TODO(Subv): Find out how AM determines the display to use, for now just
    // create the layer in the Default display.
    const auto display_id = m_nvnflinger->OpenDisplay("Default");
    const auto layer_id = m_nvnflinger->CreateLayer(*display_id);

    m_nvnflinger->SetLayerVisibility(*layer_id, m_visible);
    m_managed_display_layers.emplace(*layer_id);

    *out_layer = *layer_id;

    R_SUCCEED();
}

Result DisplayLayerManager::CreateManagedDisplaySeparableLayer(u64* out_layer,
                                                               u64* out_recording_layer) {
    R_UNLESS(m_nvnflinger != nullptr, VI::ResultOperationFailed);

    // TODO(Subv): Find out how AM determines the display to use, for now just
    // create the layer in the Default display.
    // This calls nn::vi::CreateRecordingLayer() which creates another layer.
    // Currently we do not support more than 1 layer per display, output 1 layer id for now.
    // Outputting 1 layer id instead of the expected 2 has not been observed to cause any adverse
    // side effects.
    // TODO: Support multiple layers
    const auto display_id = m_nvnflinger->OpenDisplay("Default");
    const auto layer_id = m_nvnflinger->CreateLayer(*display_id);

    m_nvnflinger->SetLayerVisibility(*layer_id, m_visible);
    m_managed_display_layers.emplace(*layer_id);

    *out_layer = *layer_id;
    *out_recording_layer = 0;

    R_SUCCEED();
}

Result DisplayLayerManager::IsSystemBufferSharingEnabled() {
    // Succeed if already enabled.
    R_SUCCEED_IF(m_buffer_sharing_enabled);

    // Ensure we can access shared layers.
    R_UNLESS(m_nvnflinger != nullptr, VI::ResultOperationFailed);
    R_UNLESS(m_applet_id != AppletId::Application, VI::ResultPermissionDenied);

    // Create the shared layer.
    const auto blend =
        m_blending_enabled ? Nvnflinger::LayerBlending::Coverage : Nvnflinger::LayerBlending::None;
    const auto display_id = m_nvnflinger->OpenDisplay("Default").value();
    R_TRY(m_nvnflinger->GetSystemBufferManager().Initialize(
        m_process, &m_system_shared_buffer_id, &m_system_shared_layer_id, display_id, blend));

    // We succeeded, so set up remaining state.
    m_buffer_sharing_enabled = true;
    m_nvnflinger->SetLayerVisibility(m_system_shared_layer_id, m_visible);
    R_SUCCEED();
}

Result DisplayLayerManager::GetSystemSharedLayerHandle(u64* out_system_shared_buffer_id,
                                                       u64* out_system_shared_layer_id) {
    R_TRY(this->IsSystemBufferSharingEnabled());

    *out_system_shared_buffer_id = m_system_shared_buffer_id;
    *out_system_shared_layer_id = m_system_shared_layer_id;

    R_SUCCEED();
}

void DisplayLayerManager::SetWindowVisibility(bool visible) {
    if (m_visible == visible) {
        return;
    }

    m_visible = visible;

    if (m_nvnflinger) {
        if (m_system_shared_layer_id) {
            m_nvnflinger->SetLayerVisibility(m_system_shared_layer_id, m_visible);
        }

        for (const auto layer_id : m_managed_display_layers) {
            m_nvnflinger->SetLayerVisibility(layer_id, m_visible);
        }
    }
}

bool DisplayLayerManager::GetWindowVisibility() const {
    return m_visible;
}

Result DisplayLayerManager::WriteAppletCaptureBuffer(bool* out_was_written,
                                                     s32* out_fbshare_layer_index) {
    R_UNLESS(m_buffer_sharing_enabled, VI::ResultPermissionDenied);
    R_RETURN(m_nvnflinger->GetSystemBufferManager().WriteAppletCaptureBuffer(
        out_was_written, out_fbshare_layer_index));
}

} // namespace Service::AM
