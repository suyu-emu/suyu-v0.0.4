// Copyright 2022 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/frontend/applets/mii_edit.h"

namespace Core::Frontend {

MiiEditApplet::~MiiEditApplet() = default;

void DefaultMiiEditApplet::ShowMii(
    const MiiParameters& parameters,
    const std::function<void(const Core::Frontend::MiiParameters& parameters)> callback) const {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    callback(parameters);
}

} // namespace Core::Frontend
