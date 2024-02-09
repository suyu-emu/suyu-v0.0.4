// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/bcat/news/newly_arrived_event_holder.h"
#include "core/hle/service/bcat/news/news_data_service.h"
#include "core/hle/service/bcat/news/news_database_service.h"
#include "core/hle/service/bcat/news/news_interface.h"
#include "core/hle/service/bcat/news/news_service.h"
#include "core/hle/service/bcat/news/overwrite_event_holder.h"
#include "core/hle/service/cmif_serialization.h"

namespace Service::BCAT {

NewsInterface::NewsInterface(Core::System& system_, u32 permissions_, const char* name_)
    : ServiceFramework{system_, name_}, permissions{permissions_} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, C<&NewsInterface::CreateNewsService>, "CreateNewsService"},
        {1, C<&NewsInterface::CreateNewlyArrivedEventHolder>, "CreateNewlyArrivedEventHolder"},
        {2, C<&NewsInterface::CreateNewsDataService>, "CreateNewsDataService"},
        {3, C<&NewsInterface::CreateNewsDatabaseService>, "CreateNewsDatabaseService"},
        {4, C<&NewsInterface::CreateOverwriteEventHolder>, "CreateOverwriteEventHolder"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

NewsInterface::~NewsInterface() = default;

Result NewsInterface::CreateNewsService(OutInterface<INewsService> out_interface) {
    LOG_INFO(Service_BCAT, "called");
    *out_interface = std::make_shared<INewsService>(system);
    R_SUCCEED();
}

Result NewsInterface::CreateNewlyArrivedEventHolder(
    OutInterface<INewlyArrivedEventHolder> out_interface) {
    LOG_INFO(Service_BCAT, "called");
    *out_interface = std::make_shared<INewlyArrivedEventHolder>(system);
    R_SUCCEED();
}

Result NewsInterface::CreateNewsDataService(OutInterface<INewsDataService> out_interface) {
    LOG_INFO(Service_BCAT, "called");
    *out_interface = std::make_shared<INewsDataService>(system);
    R_SUCCEED();
}

Result NewsInterface::CreateNewsDatabaseService(OutInterface<INewsDatabaseService> out_interface) {
    LOG_INFO(Service_BCAT, "called");
    *out_interface = std::make_shared<INewsDatabaseService>(system);
    R_SUCCEED();
}

Result NewsInterface::CreateOverwriteEventHolder(
    OutInterface<IOverwriteEventHolder> out_interface) {
    LOG_INFO(Service_BCAT, "called");
    *out_interface = std::make_shared<IOverwriteEventHolder>(system);
    R_SUCCEED();
}

} // namespace Service::BCAT
