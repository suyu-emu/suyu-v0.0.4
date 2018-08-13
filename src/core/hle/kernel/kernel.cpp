// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"

namespace Kernel {

std::atomic<u32> Object::next_object_id{0};

/// Initialize the kernel
void Init() {
    Kernel::ResourceLimitsInit();
    Kernel::ThreadingInit();
    Kernel::TimersInit();

    Object::next_object_id = 0;
    // TODO(Subv): Start the process ids from 10 for now, as lower PIDs are
    // reserved for low-level services
    Process::next_process_id = 10;
}

/// Shutdown the kernel
void Shutdown() {
    // Free all kernel objects
    g_handle_table.Clear();

    Kernel::ThreadingShutdown();

    Kernel::TimersShutdown();
    Kernel::ResourceLimitsShutdown();
}

} // namespace Kernel
