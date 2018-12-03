// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "common/common_types.h"
#include "core/hle/kernel/object.h"

namespace CoreTiming {
struct EventType;
}

namespace Kernel {
class ReadableEvent;
class WritableEvent;
} // namespace Kernel

namespace Service::Nvidia {
class Module;
}

namespace Service::NVFlinger {

class BufferQueue;

struct Layer {
    Layer(u64 id, std::shared_ptr<BufferQueue> queue);
    ~Layer();

    u64 id;
    std::shared_ptr<BufferQueue> buffer_queue;
};

struct Display {
    Display(u64 id, std::string name);
    ~Display();

    u64 id;
    std::string name;

    std::vector<Layer> layers;
    Kernel::EventPair vsync_event;
};

class NVFlinger final {
public:
    NVFlinger();
    ~NVFlinger();

    /// Sets the NVDrv module instance to use to send buffers to the GPU.
    void SetNVDrvInstance(std::shared_ptr<Nvidia::Module> instance);

    /// Opens the specified display and returns the id.
    u64 OpenDisplay(std::string_view name);

    /// Creates a layer on the specified display and returns the layer id.
    u64 CreateLayer(u64 display_id);

    /// Gets the buffer queue id of the specified layer in the specified display.
    u32 GetBufferQueueId(u64 display_id, u64 layer_id);

    /// Gets the vsync event for the specified display.
    Kernel::SharedPtr<Kernel::ReadableEvent> GetVsyncEvent(u64 display_id);

    /// Obtains a buffer queue identified by the id.
    std::shared_ptr<BufferQueue> GetBufferQueue(u32 id) const;

    /// Performs a composition request to the emulated nvidia GPU and triggers the vsync events when
    /// finished.
    void Compose();

private:
    /// Returns the display identified by the specified id.
    Display& GetDisplay(u64 display_id);

    /// Returns the layer identified by the specified id in the desired display.
    Layer& GetLayer(u64 display_id, u64 layer_id);

    std::shared_ptr<Nvidia::Module> nvdrv;

    std::vector<Display> displays;
    std::vector<std::shared_ptr<BufferQueue>> buffer_queues;

    /// Id to use for the next layer that is created, this counter is shared among all displays.
    u64 next_layer_id = 1;
    /// Id to use for the next buffer queue that is created, this counter is shared among all
    /// layers.
    u32 next_buffer_queue_id = 1;

    /// CoreTiming event that handles screen composition.
    CoreTiming::EventType* composition_event;
};

} // namespace Service::NVFlinger
