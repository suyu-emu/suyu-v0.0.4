// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Service::OLSC {

class INativeHandleHolder;

class ITransferTaskListController final : public ServiceFramework<ITransferTaskListController> {
public:
    explicit ITransferTaskListController(Core::System& system_);
    ~ITransferTaskListController() override;

private:
    Result GetTransferTaskEndEventNativeHandleHolder(Out<SharedPointer<INativeHandleHolder>> out_holder);
    Result GetTransferTaskStartEventNativeHandleHolder(Out<SharedPointer<INativeHandleHolder>> out_holder);
};

} // namespace Service::OLSC
