// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_auto_object.h"
#include "core/hle/kernel/kernel.h"

namespace Kernel {

KAutoObject* KAutoObject::Create(KAutoObject* obj) {
    obj->m_ref_count = 1;
    obj->m_class_token = obj->GetTypeObj().GetClassToken();
    return obj;
}

void KAutoObject::RegisterWithKernel() {
    m_kernel.RegisterKernelObject(this);
}

void KAutoObject::UnregisterWithKernel(KernelCore& kernel, KAutoObject* self) {
    kernel.UnregisterKernelObject(self);
}

} // namespace Kernel
