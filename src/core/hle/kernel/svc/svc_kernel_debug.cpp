// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

void KernelDebug([[maybe_unused]] Core::System& system, [[maybe_unused]] u32 kernel_debug_type,
                 [[maybe_unused]] u64 param1, [[maybe_unused]] u64 param2,
                 [[maybe_unused]] u64 param3) {
    // Intentionally do nothing, as this does nothing in released kernel binaries.
}

void ChangeKernelTraceState([[maybe_unused]] Core::System& system,
                            [[maybe_unused]] u32 trace_state) {
    // Intentionally do nothing, as this does nothing in released kernel binaries.
}

} // namespace Kernel::Svc
