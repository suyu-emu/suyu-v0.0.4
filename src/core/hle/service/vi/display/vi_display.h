// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/hle/result.h"

namespace Core {
class System;
}

namespace Kernel {
class KEvent;
class KReadableEvent;
} // namespace Kernel

namespace Service::android {
class BufferQueueProducer;
}

namespace Service::KernelHelpers {
class ServiceContext;
}

namespace Service::Nvnflinger {
class HardwareComposer;
class HosBinderDriverServer;
} // namespace Service::Nvnflinger

namespace Service::Nvidia::NvCore {
class Container;
class NvMap;
} // namespace Service::Nvidia::NvCore

namespace Service::VI {

class Layer;

/// Represents a single display type
class Display {
public:
    YUZU_NON_COPYABLE(Display);
    YUZU_NON_MOVEABLE(Display);

    /// Constructs a display with a given unique ID and name.
    ///
    /// @param id The unique ID for this display.
    /// @param hos_binder_driver_server_ Nvnflinger HOSBinderDriver server instance.
    /// @param service_context_ The ServiceContext for the owning service.
    /// @param name_ The name for this display.
    /// @param system_ The global system instance.
    ///
    Display(u64 id, std::string name_, Nvnflinger::HosBinderDriverServer& hos_binder_driver_server_,
            KernelHelpers::ServiceContext& service_context_, Core::System& system_);
    ~Display();

    /// Gets the unique ID assigned to this display.
    u64 GetID() const {
        return display_id;
    }

    /// Gets the name of this display
    const std::string& GetName() const {
        return name;
    }

    /// Whether or not this display has any layers added to it.
    bool HasLayers() const {
        return GetNumLayers() > 0;
    }

    /// Gets a layer for this display based off an index.
    Layer& GetLayer(std::size_t index);

    std::size_t GetNumLayers() const;

    /// Gets the internal vsync event.
    Kernel::KReadableEvent* GetVSyncEvent();

    /// Signals the internal vsync event.
    void SignalVSyncEvent();

    /// Creates and adds a layer to this display with the given ID.
    ///
    /// @param layer_id The ID to assign to the created layer.
    /// @param binder_id The ID assigned to the buffer queue.
    ///
    void CreateLayer(u64 layer_id, u32 binder_id, Service::Nvidia::NvCore::Container& core);

    /// Removes a layer from this display with the given ID.
    ///
    /// @param layer_id The ID assigned to the layer to destroy.
    ///
    void DestroyLayer(u64 layer_id);

    /// Resets the display for a new connection.
    void Reset() {
        layers.clear();
    }

    void Abandon();

    /// Attempts to find a layer with the given ID.
    ///
    /// @param layer_id The layer ID.
    ///
    /// @returns If found, the Layer instance with the given ID.
    ///          If not found, then nullptr is returned.
    ///
    Layer* FindLayer(u64 layer_id);

    /// Attempts to find a layer with the given ID.
    ///
    /// @param layer_id The layer ID.
    ///
    /// @returns If found, the Layer instance with the given ID.
    ///          If not found, then nullptr is returned.
    ///
    const Layer* FindLayer(u64 layer_id) const;

    Nvnflinger::HardwareComposer& GetComposer() const {
        return *hardware_composer;
    }

private:
    u64 display_id;
    std::string name;
    Nvnflinger::HosBinderDriverServer& hos_binder_driver_server;
    KernelHelpers::ServiceContext& service_context;

    std::vector<std::unique_ptr<Layer>> layers;
    std::unique_ptr<Nvnflinger::HardwareComposer> hardware_composer;
    Kernel::KEvent* vsync_event{};
    bool is_abandoned{};
};

} // namespace Service::VI
