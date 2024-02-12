// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <set>

#include "common/common_types.h"
#include "core/hle/result.h"
#include "core/hle/service/am/am_types.h"

namespace Kernel {
class KProcess;
}

namespace Service::Nvnflinger {
class Nvnflinger;
}

namespace Service::AM {

class DisplayLayerManager {
public:
    explicit DisplayLayerManager();
    ~DisplayLayerManager();

    void Initialize(Nvnflinger::Nvnflinger* nvnflinger, Kernel::KProcess* process,
                    AppletId applet_id, LibraryAppletMode mode);
    void Finalize();

    Result CreateManagedDisplayLayer(u64* out_layer);
    Result CreateManagedDisplaySeparableLayer(u64* out_layer, u64* out_recording_layer);

    Result IsSystemBufferSharingEnabled();
    Result GetSystemSharedLayerHandle(u64* out_system_shared_buffer_id,
                                      u64* out_system_shared_layer_id);

    void SetWindowVisibility(bool visible);
    bool GetWindowVisibility() const;

    Result WriteAppletCaptureBuffer(bool* out_was_written, s32* out_fbshare_layer_index);

private:
    Kernel::KProcess* m_process{};
    Nvnflinger::Nvnflinger* m_nvnflinger{};
    std::set<u64> m_managed_display_layers{};
    std::set<u64> m_managed_display_recording_layers{};
    u64 m_system_shared_buffer_id{};
    u64 m_system_shared_layer_id{};
    AppletId m_applet_id{};
    bool m_buffer_sharing_enabled{};
    bool m_blending_enabled{};
    bool m_visible{true};
};

} // namespace Service::AM
