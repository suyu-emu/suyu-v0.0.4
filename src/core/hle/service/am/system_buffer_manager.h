// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <set>

#include "common/common_funcs.h"
#include "common/common_types.h"

#include "core/hle/service/am/am_types.h"

namespace Kernel {
class KProcess;
}

namespace Service::Nvnflinger {
class Nvnflinger;
}

union Result;

namespace Service::AM {

class SystemBufferManager {
public:
    SystemBufferManager();
    ~SystemBufferManager();

    bool Initialize(Nvnflinger::Nvnflinger* flinger, Kernel::KProcess* process, AppletId applet_id,
                    LibraryAppletMode mode);

    void GetSystemSharedLayerHandle(u64* out_system_shared_buffer_id,
                                    u64* out_system_shared_layer_id) {
        *out_system_shared_buffer_id = m_system_shared_buffer_id;
        *out_system_shared_layer_id = m_system_shared_layer_id;
    }

    void SetWindowVisibility(bool visible);

    Result WriteAppletCaptureBuffer(bool* out_was_written, s32* out_fbshare_layer_index);

private:
    Kernel::KProcess* m_process{};
    Nvnflinger::Nvnflinger* m_nvnflinger{};
    bool m_buffer_sharing_enabled{};
    bool m_visible{true};
    u64 m_system_shared_buffer_id{};
    u64 m_system_shared_layer_id{};
};

} // namespace Service::AM
