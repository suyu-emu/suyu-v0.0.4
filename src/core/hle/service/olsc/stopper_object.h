// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Service::OLSC {

class IStopperObject final : public ServiceFramework<IStopperObject> {
public:
    explicit IStopperObject(Core::System& system_);
    ~IStopperObject() override;
};

} // namespace Service::OLSC
