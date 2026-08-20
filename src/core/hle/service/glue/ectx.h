// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Glue {

class ECTX_W final : public ServiceFramework<ECTX_W> {
public:
    explicit ECTX_W(Core::System& system_);
    ~ECTX_W() override;
};

class ECTX_R final : public ServiceFramework<ECTX_R> {
public:
    explicit ECTX_R(Core::System& system_);
    ~ECTX_R() override;
};

class ECTX_AW final : public ServiceFramework<ECTX_AW> {
public:
    explicit ECTX_AW(Core::System& system_);
    ~ECTX_AW() override;

private:
    void CreateContextRegistrar(HLERequestContext& ctx);
};

} // namespace Service::Glue
