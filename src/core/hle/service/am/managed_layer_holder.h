// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <set>

#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Service::Nvnflinger {
class Nvnflinger;
}

namespace Service::AM {

class ManagedLayerHolder {
public:
    ManagedLayerHolder();
    ~ManagedLayerHolder();

    void Initialize(Nvnflinger::Nvnflinger* nvnflinger);
    void CreateManagedDisplayLayer(u64* out_layer);
    void CreateManagedDisplaySeparableLayer(u64* out_layer, u64* out_recording_layer);

private:
    Nvnflinger::Nvnflinger* m_nvnflinger{};
    std::set<u64> m_managed_display_layers{};
    std::set<u64> m_managed_display_recording_layers{};
};

} // namespace Service::AM
