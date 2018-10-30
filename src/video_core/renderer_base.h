// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <optional>

#include "common/common_types.h"
#include "video_core/gpu.h"
#include "video_core/rasterizer_interface.h"

namespace Core::Frontend {
class EmuWindow;
}

namespace VideoCore {

struct RendererSettings {
    std::atomic_bool use_framelimiter{false};
    std::atomic_bool set_background_color{false};
};

class RendererBase : NonCopyable {
public:
    explicit RendererBase(Core::Frontend::EmuWindow& window);
    virtual ~RendererBase();

    /// Swap buffers (render frame)
    virtual void SwapBuffers(
        std::optional<std::reference_wrapper<const Tegra::FramebufferConfig>> framebuffer) = 0;

    /// Initialize the renderer
    virtual bool Init() = 0;

    /// Shutdown the renderer
    virtual void ShutDown() = 0;

    // Getter/setter functions:
    // ------------------------

    f32 GetCurrentFPS() const {
        return m_current_fps;
    }

    int GetCurrentFrame() const {
        return m_current_frame;
    }

    RasterizerInterface& Rasterizer() {
        return *rasterizer;
    }

    const RasterizerInterface& Rasterizer() const {
        return *rasterizer;
    }

    /// Refreshes the settings common to all renderers
    void RefreshBaseSettings();

protected:
    Core::Frontend::EmuWindow& render_window; ///< Reference to the render window handle.
    std::unique_ptr<RasterizerInterface> rasterizer;
    f32 m_current_fps = 0.0f; ///< Current framerate, should be set by the renderer
    int m_current_frame = 0;  ///< Current frame, should be set by the renderer

    RendererSettings renderer_settings;

private:
    /// Updates the framebuffer layout of the contained render window handle.
    void UpdateCurrentFramebufferLayout();
};

} // namespace VideoCore
