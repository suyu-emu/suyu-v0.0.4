// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

#include "core/hle/service/bcat/news/news_storage.h"

namespace Core {
class System;
}

namespace Service::News {

class INewsDataService final : public ServiceFramework<INewsDataService> {
public:
    explicit INewsDataService(Core::System& system_);
    ~INewsDataService() override;

private:
    bool TryOpen(std::string_view key, std::string_view user);

    Result Open(InBuffer<BufferAttr_HipcMapAlias> name);
    Result OpenWithNewsRecordV1(NewsRecordV1 record_buffer);
    Result OpenWithNewsRecord(NewsRecord record_buffer);
    Result Read(Out<u64> out_size, s64 offset, OutBuffer<BufferAttr_HipcMapAlias> out_buffer);
    Result GetSize(Out<s64> out_size);

    std::vector<u8> opened_payload;
};

} // namespace Service::News
