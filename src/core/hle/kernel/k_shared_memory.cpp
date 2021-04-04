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

KSharedMemory::KSharedMemory(KernelCore& kernel) : KAutoObjectWithSlabHeapAndContainer{kernel} {}

KSharedMemory::~KSharedMemory() {
    kernel.GetSystemResourceLimit()->Release(LimitableResource::PhysicalMemory, size);
}

ResultCode KSharedMemory::Initialize(KernelCore& kernel_, Core::DeviceMemory& device_memory_,
                                     Process* owner_process_, KPageLinkedList&& page_list_,
                                     KMemoryPermission owner_permission_,
                                     KMemoryPermission user_permission_, PAddr physical_address_,
                                     std::size_t size_, std::string name_) {

    resource_limit = kernel_.GetSystemResourceLimit();
    KScopedResourceReservation memory_reservation(resource_limit, LimitableResource::PhysicalMemory,
                                                  size_);
    ASSERT(memory_reservation.Succeeded());

    owner_process = owner_process_;
    device_memory = &device_memory_;
    page_list = std::move(page_list_);
    owner_permission = owner_permission_;
    user_permission = user_permission_;
    physical_address = physical_address_;
    size = size_;
    name = name_;
    is_initialized = true;

    memory_reservation.Commit();

    return RESULT_SUCCESS;
}

void KSharedMemory::Finalize() {
    ///* Get the number of pages. */
    // const size_t num_pages = m_page_group.GetNumPages();
    // const size_t size = num_pages * PageSize;

    ///* Close and finalize the page group. */
    // m_page_group.Close();
    // m_page_group.Finalize();

    // Release the memory reservation.
    resource_limit->Release(LimitableResource::PhysicalMemory, size);
    resource_limit->Close();

    // Perform inherited finalization.
    KAutoObjectWithSlabHeapAndContainer<KSharedMemory, KAutoObjectWithList>::Finalize();
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
