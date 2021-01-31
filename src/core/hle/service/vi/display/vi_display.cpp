// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <utility>

#include <fmt/format.h>

#include "common/assert.h"
#include "core/core.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/k_writable_event.h"
#include "core/hle/service/vi/display/vi_display.h"
#include "core/hle/service/vi/layer/vi_layer.h"

namespace Service::VI {

Display::Display(u64 id, std::string name, Core::System& system) : id{id}, name{std::move(name)} {
    auto& kernel = system.Kernel();
    vsync_event = Kernel::KEvent::Create(kernel, fmt::format("Display VSync Event {}", id));
    vsync_event->Initialize();
}

Display::~Display() = default;

Layer& Display::GetLayer(std::size_t index) {
    return *layers.at(index);
}

const Layer& Display::GetLayer(std::size_t index) const {
    return *layers.at(index);
}

std::shared_ptr<Kernel::KReadableEvent> Display::GetVSyncEvent() const {
    return vsync_event->GetReadableEvent();
}

void Display::SignalVSyncEvent() {
    vsync_event->GetWritableEvent()->Signal();
}

void Display::CreateLayer(u64 id, NVFlinger::BufferQueue& buffer_queue) {
    // TODO(Subv): Support more than 1 layer.
    ASSERT_MSG(layers.empty(), "Only one layer is supported per display at the moment");

    layers.emplace_back(std::make_shared<Layer>(id, buffer_queue));
}

void Display::CloseLayer(u64 id) {
    layers.erase(
        std::remove_if(layers.begin(), layers.end(),
                       [id](const std::shared_ptr<Layer>& layer) { return layer->GetID() == id; }),
        layers.end());
}

Layer* Display::FindLayer(u64 id) {
    const auto itr =
        std::find_if(layers.begin(), layers.end(),
                     [id](const std::shared_ptr<Layer>& layer) { return layer->GetID() == id; });

    if (itr == layers.end()) {
        return nullptr;
    }

    return itr->get();
}

const Layer* Display::FindLayer(u64 id) const {
    const auto itr =
        std::find_if(layers.begin(), layers.end(),
                     [id](const std::shared_ptr<Layer>& layer) { return layer->GetID() == id; });

    if (itr == layers.end()) {
        return nullptr;
    }

    return itr->get();
}

} // namespace Service::VI
