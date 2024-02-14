// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/service.h"

namespace Service::VI {

class IHOSBinderDriver final : public ServiceFramework<IHOSBinderDriver> {
public:
    explicit IHOSBinderDriver(Core::System& system_, Nvnflinger::HosBinderDriverServer& server_);
    ~IHOSBinderDriver() override;

private:
    void TransactParcel(HLERequestContext& ctx);
    void AdjustRefcount(HLERequestContext& ctx);
    void GetNativeHandle(HLERequestContext& ctx);

private:
    Nvnflinger::HosBinderDriverServer& server;
};

} // namespace Service::VI
