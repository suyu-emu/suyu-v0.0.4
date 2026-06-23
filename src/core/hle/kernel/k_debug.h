// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/kernel/k_auto_object.h"
#include "core/hle/kernel/slab_helpers.h"

namespace Kernel {

class KDebug final : public KAutoObjectWithSlabHeapAndContainer<KDebug, KAutoObjectWithList> {
    KERNEL_AUTOOBJECT_TRAITS(KDebug, KAutoObject);

public:
    explicit KDebug(KernelCore& kernel) : KAutoObjectWithSlabHeapAndContainer{kernel} {}

    static void PostDestroy(KernelCore& kernel, uintptr_t arg) {}
};

} // namespace Kernel
