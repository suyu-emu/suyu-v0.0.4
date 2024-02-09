// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::BCAT {
class INewsService;
class INewlyArrivedEventHolder;
class INewsDataService;
class INewsDatabaseService;
class IOverwriteEventHolder;

class NewsInterface final : public ServiceFramework<NewsInterface> {
public:
    explicit NewsInterface(Core::System& system_, u32 permissions_, const char* name_);
    ~NewsInterface() override;

private:
    Result CreateNewsService(OutInterface<INewsService> out_interface);
    Result CreateNewlyArrivedEventHolder(OutInterface<INewlyArrivedEventHolder> out_interface);
    Result CreateNewsDataService(OutInterface<INewsDataService> out_interface);
    Result CreateNewsDatabaseService(OutInterface<INewsDatabaseService> out_interface);
    Result CreateOverwriteEventHolder(OutInterface<IOverwriteEventHolder> out_interface);

    u32 permissions;
};

} // namespace Service::BCAT
