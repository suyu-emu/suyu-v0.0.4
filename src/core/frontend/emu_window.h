// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <tuple>
#include <utility>
#include "common/common_types.h"
#include "core/frontend/framebuffer_layout.h"

namespace Core::Frontend {

/**
 * Represents a drawing context that supports graphics operations.
 */
class GraphicsContext {
public:
    virtual ~GraphicsContext();

    /// Inform the driver to swap the front/back buffers and present the current image
    virtual void SwapBuffers() {}

    /// Makes the graphics context current for the caller thread
    virtual void MakeCurrent() {}

    /// Releases (dunno if this is the "right" word) the context from the caller thread
    virtual void DoneCurrent() {}

    class Scoped {
    public:
        explicit Scoped(GraphicsContext& context_) : context(context_) {
            context.MakeCurrent();
        }
        ~Scoped() {
            context.DoneCurrent();
        }

    private:
        GraphicsContext& context;
    };

    /// Calls MakeCurrent on the context and calls DoneCurrent when the scope for the returned value
    /// ends
    Scoped Acquire() {
        return Scoped{*this};
    }
};

/**
 * Abstraction class used to provide an interface between emulation code and the frontend
 * (e.g. SDL, QGLWidget, GLFW, etc...).
 *
 * Design notes on the interaction between EmuWindow and the emulation core:
 * - Generally, decisions on anything visible to the user should be left up to the GUI.
 *   For example, the emulation core should not try to dictate some window title or size.
 *   This stuff is not the core's business and only causes problems with regards to thread-safety
 *   anyway.
 * - Under certain circumstances, it may be desirable for the core to politely request the GUI
 *   to set e.g. a minimum window size. However, the GUI should always be free to ignore any
 *   such hints.
 * - EmuWindow may expose some of its state as read-only to the emulation core, however care
 *   should be taken to make sure the provided information is self-consistent. This requires
 *   some sort of synchronization (most of this is still a TODO).
 * - DO NOT TREAT THIS CLASS AS A GUI TOOLKIT ABSTRACTION LAYER. That's not what it is. Please
 *   re-read the upper points again and think about it if you don't see this.
 */
class EmuWindow {
public:
    /// Data structure to store emuwindow configuration
    struct WindowConfig {
        bool fullscreen = false;
        int res_width = 0;
        int res_height = 0;
        std::pair<unsigned, unsigned> min_client_area_size;
    };

    /// Polls window events
    virtual void PollEvents() = 0;

    /**
     * Returns a GraphicsContext that the frontend provides to be used for rendering.
     */
    virtual std::unique_ptr<GraphicsContext> CreateSharedContext() const = 0;

    /// Returns if window is shown (not minimized)
    virtual bool IsShown() const = 0;

    /// Retrieves Vulkan specific handlers from the window
    virtual void RetrieveVulkanHandlers(void* get_instance_proc_addr, void* instance,
                                        void* surface) const = 0;

    /**
     * Signal that a touch pressed event has occurred (e.g. mouse click pressed)
     * @param framebuffer_x Framebuffer x-coordinate that was pressed
     * @param framebuffer_y Framebuffer y-coordinate that was pressed
     */
    void TouchPressed(unsigned framebuffer_x, unsigned framebuffer_y);

    /// Signal that a touch released event has occurred (e.g. mouse click released)
    void TouchReleased();

    /**
     * Signal that a touch movement event has occurred (e.g. mouse was moved over the emu window)
     * @param framebuffer_x Framebuffer x-coordinate
     * @param framebuffer_y Framebuffer y-coordinate
     */
    void TouchMoved(unsigned framebuffer_x, unsigned framebuffer_y);

    /**
     * Returns currently active configuration.
     * @note Accesses to the returned object need not be consistent because it may be modified in
     * another thread
     */
    const WindowConfig& GetActiveConfig() const {
        return active_config;
    }

    /**
     * Requests the internal configuration to be replaced by the specified argument at some point in
     * the future.
     * @note This method is thread-safe, because it delays configuration changes to the GUI event
     * loop. Hence there is no guarantee on when the requested configuration will be active.
     */
    void SetConfig(const WindowConfig& val) {
        config = val;
    }

    /**
     * Gets the framebuffer layout (width, height, and screen regions)
     * @note This method is thread-safe
     */
    const Layout::FramebufferLayout& GetFramebufferLayout() const {
        return framebuffer_layout;
    }

    /**
     * Convenience method to update the current frame layout
     * Read from the current settings to determine which layout to use.
     */
    void UpdateCurrentFramebufferLayout(unsigned width, unsigned height);

protected:
    EmuWindow();
    virtual ~EmuWindow();

    /**
     * Processes any pending configuration changes from the last SetConfig call.
     * This method invokes OnMinimalClientAreaChangeRequest if the corresponding configuration
     * field changed.
     * @note Implementations will usually want to call this from the GUI thread.
     * @todo Actually call this in existing implementations.
     */
    void ProcessConfigurationChanges() {
        // TODO: For proper thread safety, we should eventually implement a proper
        // multiple-writer/single-reader queue...

        if (config.min_client_area_size != active_config.min_client_area_size) {
            OnMinimalClientAreaChangeRequest(config.min_client_area_size);
            config.min_client_area_size = active_config.min_client_area_size;
        }
    }

    /**
     * Update framebuffer layout with the given parameter.
     * @note EmuWindow implementations will usually use this in window resize event handlers.
     */
    void NotifyFramebufferLayoutChanged(const Layout::FramebufferLayout& layout) {
        framebuffer_layout = layout;
    }

    /**
     * Update internal client area size with the given parameter.
     * @note EmuWindow implementations will usually use this in window resize event handlers.
     */
    void NotifyClientAreaSizeChanged(const std::pair<unsigned, unsigned>& size) {
        client_area_width = size.first;
        client_area_height = size.second;
    }

private:
    /**
     * Handler called when the minimal client area was requested to be changed via SetConfig.
     * For the request to be honored, EmuWindow implementations will usually reimplement this
     * function.
     */
    virtual void OnMinimalClientAreaChangeRequest(std::pair<unsigned, unsigned>) {
        // By default, ignore this request and do nothing.
    }

    Layout::FramebufferLayout framebuffer_layout; ///< Current framebuffer layout

    unsigned client_area_width;  ///< Current client width, should be set by window impl.
    unsigned client_area_height; ///< Current client height, should be set by window impl.

    WindowConfig config;        ///< Internal configuration (changes pending for being applied in
                                /// ProcessConfigurationChanges)
    WindowConfig active_config; ///< Internal active configuration

    class TouchState;
    std::shared_ptr<TouchState> touch_state;

    /**
     * Clip the provided coordinates to be inside the touchscreen area.
     */
    std::tuple<unsigned, unsigned> ClipToTouchScreen(unsigned new_x, unsigned new_y) const;
};

} // namespace Core::Frontend
