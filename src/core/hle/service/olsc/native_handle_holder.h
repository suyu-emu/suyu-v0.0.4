// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/service.h"

namespace Service::OLSC {

class INativeHandleHolder final : public ServiceFramework<INativeHandleHolder> {
public:
    explicit INativeHandleHolder(Core::System& system_);
    ~INativeHandleHolder() override;
};

} // namespace Service::OLSC
