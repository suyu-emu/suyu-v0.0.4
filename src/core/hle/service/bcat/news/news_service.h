// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::News {

class INewsService final : public ServiceFramework<INewsService> {
public:
    explicit INewsService(Core::System& system_);
    ~INewsService() override;

private:
    Result PostLocalNews(InBuffer<BufferAttr_HipcAutoSelect> buffer_data);

    Result SetPassphrase(InBuffer<BufferAttr_HipcPointer> buffer_data);

    Result GetSubscriptionStatus(Out<u32> out_status, InBuffer<BufferAttr_HipcPointer> buffer_data);

    Result GetTopicList(Out<s32> out_count, OutBuffer<BufferAttr_HipcMapAlias> out_topics, s32 filter);

    Result IsSystemUpdateRequired(Out<bool> out_is_system_update_required);

    Result RequestAutoSubscription(u64 value);

    Result ClearStorage();

    Result ClearSubscriptionStatusAll();

    Result GetNewsDatabaseDump();
};

} // namespace Service::News
