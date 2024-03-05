// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Service::AM {

class IApplicationAccessor;
struct Applet;

class IApplicationCreator final : public ServiceFramework<IApplicationCreator> {
public:
    explicit IApplicationCreator(Core::System& system_);
    ~IApplicationCreator() override;

private:
    Result CreateApplication(Out<SharedPointer<IApplicationAccessor>>, u64 application_id);
};

} // namespace Service::AM
