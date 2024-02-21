// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/service.h"

namespace Service::OLSC {

class IOlscServiceForSystemService final : public ServiceFramework<IOlscServiceForSystemService> {
public:
    explicit IOlscServiceForSystemService(Core::System& system_);
    ~IOlscServiceForSystemService() override;

private:
    void OpenTransferTaskListController(HLERequestContext& ctx);
};

} // namespace Service::OLSC
