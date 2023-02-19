// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

namespace Core::Frontend {

/**
 * Represents a drawing context that supports graphics operations.
 */
class GraphicsContext {
public:
    virtual ~GraphicsContext() = default;

    /// Inform the driver to swap the front/back buffers and present the current image
    virtual void SwapBuffers() {}

    /// Makes the graphics context current for the caller thread
    virtual void MakeCurrent() {}

    /// Releases (dunno if this is the "right" word) the context from the caller thread
    virtual void DoneCurrent() {}

    /// Parameters used to configure custom drivers (used by Android only)
    struct CustomDriverParameters {
        std::string hook_lib_dir;
        std::string custom_driver_dir;
        std::string custom_driver_name;
        std::string file_redirect_dir;
    };

    /// Gets custom driver parameters configured by the frontend (used by Android only)
    virtual std::optional<CustomDriverParameters> GetCustomDriverParameters() {
        return {};
    }

    class Scoped {
    public:
        [[nodiscard]] explicit Scoped(GraphicsContext& context_) : context(context_) {
            context.MakeCurrent();
        }
        ~Scoped() {
            if (active) {
                context.DoneCurrent();
            }
        }

        /// In the event that context was destroyed before the Scoped is destroyed, this provides a
        /// mechanism to prevent calling a destroyed object's method during the deconstructor
        void Cancel() {
            active = false;
        }

    private:
        GraphicsContext& context;
        bool active{true};
    };

    /// Calls MakeCurrent on the context and calls DoneCurrent when the scope for the returned value
    /// ends
    [[nodiscard]] Scoped Acquire() {
        return Scoped{*this};
    }
};

} // namespace Core::Frontend
