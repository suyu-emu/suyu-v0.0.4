// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "core/core.h"
#include "core/hle/kernel/k_page_table.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/kernel/kernel.h"

namespace Kernel {

KSharedMemory::KSharedMemory(KernelCore& kernel, Core::DeviceMemory& device_memory)
    : Object{kernel}, device_memory{device_memory} {}

KSharedMemory::~KSharedMemory() {
    kernel.GetSystemResourceLimit()->Release(LimitableResource::PhysicalMemory, size);
}

std::shared_ptr<KSharedMemory> KSharedMemory::Create(
    KernelCore& kernel, Core::DeviceMemory& device_memory, Process* owner_process,
    KPageLinkedList&& page_list, KMemoryPermission owner_permission,
    KMemoryPermission user_permission, PAddr physical_address, std::size_t size, std::string name) {

    const auto resource_limit = kernel.GetSystemResourceLimit();
    KScopedResourceReservation memory_reservation(resource_limit, LimitableResource::PhysicalMemory,
                                                  size);
    ASSERT(memory_reservation.Succeeded());

    std::shared_ptr<KSharedMemory> shared_memory{
        std::make_shared<KSharedMemory>(kernel, device_memory)};

    shared_memory->owner_process = owner_process;
    shared_memory->page_list = std::move(page_list);
    shared_memory->owner_permission = owner_permission;
    shared_memory->user_permission = user_permission;
    shared_memory->physical_address = physical_address;
    shared_memory->size = size;
    shared_memory->name = name;

    memory_reservation.Commit();
    return shared_memory;
}

ResultCode KSharedMemory::Map(Process& target_process, VAddr address, std::size_t size,
                              KMemoryPermission permissions) {
    const u64 page_count{(size + PageSize - 1) / PageSize};

    if (page_list.GetNumPages() != page_count) {
        UNIMPLEMENTED_MSG("Page count does not match");
    }

    const KMemoryPermission expected =
        &target_process == owner_process ? owner_permission : user_permission;

    if (permissions != expected) {
        UNIMPLEMENTED_MSG("Permission does not match");
    }

    return target_process.PageTable().MapPages(address, page_list, KMemoryState::Shared,
                                               permissions);
}

} // namespace Kernel
