// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/bcat/news/news_database_service.h"
#include "core/hle/service/cmif_serialization.h"

namespace Service::BCAT {

INewsDatabaseService::INewsDatabaseService(Core::System& system_)
    : ServiceFramework{system_, "INewsDatabaseService"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "GetListV1"},
        {1, C<&INewsDatabaseService::Count>, "Count"},
        {2, nullptr, "CountWithKey"},
        {3, nullptr, "UpdateIntegerValue"},
        {4, nullptr, "UpdateIntegerValueWithAddition"},
        {5, nullptr, "UpdateStringValue"},
        {1000, nullptr, "GetList"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

INewsDatabaseService::~INewsDatabaseService() = default;

Result INewsDatabaseService::Count(Out<u32> out_count,
                                   InBuffer<BufferAttr_HipcPointer> buffer_data) {
    LOG_WARNING(Service_BCAT, "(STUBBED) called, buffer_size={}", buffer_data.size());
    *out_count = 0;
    R_SUCCEED();
}

} // namespace Service::BCAT
