// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/object.h"

union ResultCode;

namespace Kernel {

class Thread;

class Mutex final {
public:
    /// Flag that indicates that a mutex still has threads waiting for it.
    static constexpr u32 MutexHasWaitersFlag = 0x40000000;
    /// Mask of the bits in a mutex address value that contain the mutex owner.
    static constexpr u32 MutexOwnerMask = 0xBFFFFFFF;

    /// Attempts to acquire a mutex at the specified address.
    static ResultCode TryAcquire(VAddr address, Handle holding_thread_handle,
                                 Handle requesting_thread_handle);

    /// Releases the mutex at the specified address.
    static ResultCode Release(VAddr address);

private:
    Mutex() = default;
    ~Mutex() = default;
};

} // namespace Kernel
