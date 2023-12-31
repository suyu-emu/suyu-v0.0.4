// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

class ILibraryAppletCreator final : public ServiceFramework<ILibraryAppletCreator> {
public:
    explicit ILibraryAppletCreator(Core::System& system_);
    ~ILibraryAppletCreator() override;

private:
    void CreateLibraryApplet(HLERequestContext& ctx);
    void CreateStorage(HLERequestContext& ctx);
    void CreateTransferMemoryStorage(HLERequestContext& ctx);
    void CreateHandleStorage(HLERequestContext& ctx);
};

} // namespace Service::AM
