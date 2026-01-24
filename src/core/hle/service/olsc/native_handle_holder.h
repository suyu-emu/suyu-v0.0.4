// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

#include "core/hle/service/kernel_helpers.h"

namespace Kernel {
class KReadableEvent;
class KEvent;
}

namespace Service::OLSC {

class INativeHandleHolder final : public ServiceFramework<INativeHandleHolder> {
public:
    explicit INativeHandleHolder(Core::System& system_);
    ~INativeHandleHolder() override;

private:
    Result GetNativeHandle(OutCopyHandle<Kernel::KReadableEvent> out_event);

    Service::KernelHelpers::ServiceContext service_context;
    Kernel::KEvent* event{};
};

} // namespace Service::OLSC
