// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/system_buffer_manager.h"
#include "core/hle/service/nvnflinger/fb_share_buffer_manager.h"
#include "core/hle/service/nvnflinger/nvnflinger.h"
#include "core/hle/service/vi/vi_results.h"

namespace Service::AM {

SystemBufferManager::SystemBufferManager() = default;

SystemBufferManager::~SystemBufferManager() {
    if (!m_nvnflinger) {
        return;
    }

    // Clean up shared layers.
    if (m_buffer_sharing_enabled) {
        m_nvnflinger->GetSystemBufferManager().Finalize(m_process);
    }
}

bool SystemBufferManager::Initialize(Nvnflinger::Nvnflinger* nvnflinger, Kernel::KProcess* process,
                                     AppletId applet_id, LibraryAppletMode mode) {
    if (m_nvnflinger) {
        return m_buffer_sharing_enabled;
    }

    m_process = process;
    m_nvnflinger = nvnflinger;
    m_buffer_sharing_enabled = false;
    m_system_shared_buffer_id = 0;
    m_system_shared_layer_id = 0;

    if (applet_id <= AppletId::Application) {
        return false;
    }

    Nvnflinger::LayerBlending blending = Nvnflinger::LayerBlending::None;
    if (mode == LibraryAppletMode::PartialForeground ||
        mode == LibraryAppletMode::PartialForegroundIndirectDisplay) {
        blending = Nvnflinger::LayerBlending::Coverage;
    }

    const auto display_id = m_nvnflinger->OpenDisplay("Default").value();
    const auto res = m_nvnflinger->GetSystemBufferManager().Initialize(
        m_process, &m_system_shared_buffer_id, &m_system_shared_layer_id, display_id, blending);

    if (res.IsSuccess()) {
        m_buffer_sharing_enabled = true;
        m_nvnflinger->SetLayerVisibility(m_system_shared_layer_id, m_visible);
    }

    return m_buffer_sharing_enabled;
}

void SystemBufferManager::SetWindowVisibility(bool visible) {
    if (m_visible == visible) {
        return;
    }

    m_visible = visible;

    if (m_nvnflinger) {
        m_nvnflinger->SetLayerVisibility(m_system_shared_layer_id, m_visible);
    }
}

Result SystemBufferManager::WriteAppletCaptureBuffer(bool* out_was_written,
                                                     s32* out_fbshare_layer_index) {
    if (!m_buffer_sharing_enabled) {
        return VI::ResultPermissionDenied;
    }

    return m_nvnflinger->GetSystemBufferManager().WriteAppletCaptureBuffer(out_was_written,
                                                                           out_fbshare_layer_index);
}

} // namespace Service::AM
