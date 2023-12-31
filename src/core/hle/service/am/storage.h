// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

class IStorageImpl {
public:
    virtual ~IStorageImpl();
    virtual std::vector<u8>& GetData() = 0;
    virtual const std::vector<u8>& GetData() const = 0;
    virtual std::size_t GetSize() const = 0;
};

class IStorage final : public ServiceFramework<IStorage> {
public:
    explicit IStorage(Core::System& system_, std::vector<u8>&& buffer);
    ~IStorage() override;

    std::vector<u8>& GetData() {
        return impl->GetData();
    }

    const std::vector<u8>& GetData() const {
        return impl->GetData();
    }

    std::size_t GetSize() const {
        return impl->GetSize();
    }

private:
    void Register();
    void Open(HLERequestContext& ctx);

    std::shared_ptr<IStorageImpl> impl;
};

} // namespace Service::AM
