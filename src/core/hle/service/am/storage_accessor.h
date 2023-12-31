// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/am/storage.h"
#include "core/hle/service/service.h"

namespace Service::AM {

class IStorageAccessor final : public ServiceFramework<IStorageAccessor> {
public:
    explicit IStorageAccessor(Core::System& system_, IStorage& backing_);
    ~IStorageAccessor() override;

private:
    void GetSize(HLERequestContext& ctx);
    void Write(HLERequestContext& ctx);
    void Read(HLERequestContext& ctx);

    IStorage& backing;
};

} // namespace Service::AM
