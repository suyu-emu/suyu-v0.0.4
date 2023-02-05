// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"
#include "core/hle/kernel/svc_types.h"

namespace Kernel::Svc {

Result FlushProcessDataCache32(Core::System& system, Handle process_handle, u64 address, u64 size) {
    // Validate address/size.
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS(address == static_cast<uintptr_t>(address), ResultInvalidCurrentMemory);
    R_UNLESS(size == static_cast<size_t>(size), ResultInvalidCurrentMemory);

    // Get the process from its handle.
    KScopedAutoObject process =
        system.Kernel().CurrentProcess()->GetHandleTable().GetObject<KProcess>(process_handle);
    R_UNLESS(process.IsNotNull(), ResultInvalidHandle);

    // Verify the region is within range.
    auto& page_table = process->PageTable();
    R_UNLESS(page_table.Contains(address, size), ResultInvalidCurrentMemory);

    // Perform the operation.
    R_RETURN(system.Memory().FlushDataCache(*process, address, size));
}

} // namespace Kernel::Svc
