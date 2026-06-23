// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/kernel/k_synchronization_object.h"

namespace Kernel {

class KWorkerTask : public KSynchronizationObject {
public:
    explicit KWorkerTask(KernelCore& kernel);
    void DoWorkerTask(KernelCore& kernel);
};

} // namespace Kernel
