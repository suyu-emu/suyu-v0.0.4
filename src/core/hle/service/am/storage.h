// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

class LibraryAppletStorage;

class IStorage final : public ServiceFramework<IStorage> {
public:
    explicit IStorage(Core::System& system_, std::shared_ptr<LibraryAppletStorage> impl_);
    explicit IStorage(Core::System& system_, std::vector<u8>&& buffer);
    ~IStorage() override;

    std::shared_ptr<LibraryAppletStorage> GetImpl() const {
        return impl;
    }

    std::vector<u8> GetData() const;

private:
    void Open(HLERequestContext& ctx);
    void OpenTransferStorage(HLERequestContext& ctx);

    const std::shared_ptr<LibraryAppletStorage> impl;
};

} // namespace Service::AM
