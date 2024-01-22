// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <boost/container/flat_map.hpp>

#include "core/hle/service/nvnflinger/buffer_item.h"

namespace Service::Nvidia::Devices {
class nvdisp_disp0;
}

namespace Service::VI {
class Display;
class Layer;
} // namespace Service::VI

namespace Service::Nvnflinger {

using LayerId = u64;

class HardwareComposer {
public:
    explicit HardwareComposer();
    ~HardwareComposer();

    u32 ComposeLocked(VI::Display& display, Nvidia::Devices::nvdisp_disp0& nvdisp,
                      u32 frame_advance);
    void RemoveLayerLocked(VI::Display& display, LayerId layer_id);

private:
    // TODO: do we want to track frame number in vi instead?
    u64 m_frame_number{0};

private:
    using ReleaseFrameNumber = u64;

    struct Framebuffer {
        android::BufferItem item{};
        ReleaseFrameNumber release_frame_number{};
        bool is_acquired{false};
    };

    enum class CacheStatus : u32 {
        NoBufferAvailable,
        BufferAcquired,
        CachedBufferReused,
    };

    boost::container::flat_map<LayerId, Framebuffer> m_framebuffers{};

private:
    bool TryAcquireFramebufferLocked(VI::Layer& layer, Framebuffer& framebuffer);
    CacheStatus CacheFramebufferLocked(VI::Layer& layer, LayerId layer_id);
};

} // namespace Service::Nvnflinger
