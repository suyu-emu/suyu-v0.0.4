// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fmt/format.h>

#include "core/core.h"
#include "core/hle/kernel/readable_event.h"
#include "core/hle/service/vi/display/vi_display.h"
#include "core/hle/service/vi/layer/vi_layer.h"

namespace Service::VI {

Display::Display(u64 id, std::string name) : id{id}, name{std::move(name)} {
    auto& kernel = Core::System::GetInstance().Kernel();
    vsync_event = Kernel::WritableEvent::CreateEventPair(kernel, Kernel::ResetType::Sticky,
                                                         fmt::format("Display VSync Event {}", id));
}

Display::~Display() = default;

} // namespace Service::VI
