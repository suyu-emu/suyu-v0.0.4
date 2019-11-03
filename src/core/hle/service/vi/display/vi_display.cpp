// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <utility>

#include <fmt/format.h>

#include "common/assert.h"
#include "core/core.h"
#include "core/hle/kernel/readable_event.h"
#include "core/hle/service/vi/display/vi_display.h"
#include "core/hle/service/vi/layer/vi_layer.h"

namespace Service::VI {

Display::Display(u64 id, std::string name, Core::System& system) : id{id}, name{std::move(name)} {
    auto& kernel = system.Kernel();
    vsync_event =
        Kernel::WritableEvent::CreateEventPair(kernel, fmt::format("Display VSync Event {}", id));
}

Display::~Display() = default;

Layer& Display::GetLayer(std::size_t index) {
    return layers.at(index);
}

const Layer& Display::GetLayer(std::size_t index) const {
    return layers.at(index);
}

Kernel::SharedPtr<Kernel::ReadableEvent> Display::GetVSyncEvent() const {
    return vsync_event.readable;
}

void Display::SignalVSyncEvent() {
    vsync_event.writable->Signal();
}

void Display::CreateLayer(u64 id, NVFlinger::BufferQueue& buffer_queue) {
    // TODO(Subv): Support more than 1 layer.
    ASSERT_MSG(layers.empty(), "Only one layer is supported per display at the moment");

    layers.emplace_back(id, buffer_queue);
}

Layer* Display::FindLayer(u64 id) {
    const auto itr = std::find_if(layers.begin(), layers.end(),
                                  [id](const VI::Layer& layer) { return layer.GetID() == id; });

    if (itr == layers.end()) {
        return nullptr;
    }

    return &*itr;
}

const Layer* Display::FindLayer(u64 id) const {
    const auto itr = std::find_if(layers.begin(), layers.end(),
                                  [id](const VI::Layer& layer) { return layer.GetID() == id; });

    if (itr == layers.end()) {
        return nullptr;
    }

    return &*itr;
}

} // namespace Service::VI
