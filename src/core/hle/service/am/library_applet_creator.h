// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

struct Applet;

class ILibraryAppletCreator final : public ServiceFramework<ILibraryAppletCreator> {
public:
    explicit ILibraryAppletCreator(Core::System& system_, std::shared_ptr<Applet> applet_);
    ~ILibraryAppletCreator() override;

private:
    void CreateLibraryApplet(HLERequestContext& ctx);
    void CreateStorage(HLERequestContext& ctx);
    void CreateTransferMemoryStorage(HLERequestContext& ctx);
    void CreateHandleStorage(HLERequestContext& ctx);

    const std::shared_ptr<Applet> applet;
};

} // namespace Service::AM
