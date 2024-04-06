// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <string>

#include "objc_bridge.h"
#include "video_core/host1x/gpu_device_memory_manager.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_metal/mtl_command_recorder.h"
#include "video_core/renderer_metal/mtl_device.h"
#include "video_core/renderer_metal/mtl_rasterizer.h"
#include "video_core/renderer_metal/mtl_swap_chain.h"

namespace Core {
class TelemetrySession;
}

namespace Core::Memory {
class Memory;
}

namespace Tegra {
class GPU;
}

namespace Metal {

class RendererMetal final : public VideoCore::RendererBase {
public:
    explicit RendererMetal(Core::Frontend::EmuWindow& emu_window,
                           Tegra::MaxwellDeviceMemoryManager& device_memory_, Tegra::GPU& gpu_,
                           std::unique_ptr<Core::Frontend::GraphicsContext> context);
    ~RendererMetal() override;

    void Composite(std::span<const Tegra::FramebufferConfig> framebuffer) override;

    std::vector<u8> GetAppletCaptureBuffer() override;

    VideoCore::RasterizerInterface* ReadRasterizer() override {
        return &rasterizer;
    }

    [[nodiscard]] std::string GetDeviceVendor() const override {
        return "Apple";
    }

private:
    Tegra::MaxwellDeviceMemoryManager& device_memory;
    Tegra::GPU& gpu;

    Device device;
    CommandRecorder command_recorder;
    SwapChain swap_chain;

    RasterizerMetal rasterizer;
};

} // namespace Metal
