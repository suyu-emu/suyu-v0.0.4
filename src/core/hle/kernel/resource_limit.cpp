// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/hle/kernel/resource_limit.h"

namespace Kernel {

ResourceLimit::ResourceLimit(KernelCore& kernel) : Object{kernel} {}
ResourceLimit::~ResourceLimit() = default;

SharedPtr<ResourceLimit> ResourceLimit::Create(KernelCore& kernel, std::string name) {
    SharedPtr<ResourceLimit> resource_limit(new ResourceLimit(kernel));

    resource_limit->name = std::move(name);
    return resource_limit;
}

s32 ResourceLimit::GetCurrentResourceValue(ResourceType resource) const {
    switch (resource) {
    case ResourceType::Commit:
        return current_commit;
    case ResourceType::Thread:
        return current_threads;
    case ResourceType::Event:
        return current_events;
    case ResourceType::Mutex:
        return current_mutexes;
    case ResourceType::Semaphore:
        return current_semaphores;
    case ResourceType::Timer:
        return current_timers;
    case ResourceType::SharedMemory:
        return current_shared_mems;
    case ResourceType::AddressArbiter:
        return current_address_arbiters;
    case ResourceType::CPUTime:
        return current_cpu_time;
    default:
        LOG_ERROR(Kernel, "Unknown resource type={:08X}", static_cast<u32>(resource));
        UNIMPLEMENTED();
        return 0;
    }
}

u32 ResourceLimit::GetMaxResourceValue(ResourceType resource) const {
    switch (resource) {
    case ResourceType::Priority:
        return max_priority;
    case ResourceType::Commit:
        return max_commit;
    case ResourceType::Thread:
        return max_threads;
    case ResourceType::Event:
        return max_events;
    case ResourceType::Mutex:
        return max_mutexes;
    case ResourceType::Semaphore:
        return max_semaphores;
    case ResourceType::Timer:
        return max_timers;
    case ResourceType::SharedMemory:
        return max_shared_mems;
    case ResourceType::AddressArbiter:
        return max_address_arbiters;
    case ResourceType::CPUTime:
        return max_cpu_time;
    default:
        LOG_ERROR(Kernel, "Unknown resource type={:08X}", static_cast<u32>(resource));
        UNIMPLEMENTED();
        return 0;
    }
}
} // namespace Kernel
