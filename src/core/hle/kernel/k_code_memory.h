// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_client_session.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_handle_table.h"
#include "core/hle/kernel/k_memory_block.h"
#include "core/hle/kernel/k_memory_layout.h"
#include "core/hle/kernel/k_page_table.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/k_resource_limit.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/kernel/k_synchronization_object.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/k_transfer_memory.h"
#include "core/hle/kernel/k_writable_event.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/physical_core.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_results.h"
#include "core/hle/kernel/svc_types.h"
#include "core/hle/kernel/svc_wrap.h"
#include "core/hle/lock.h"
#include "core/hle/result.h"
#include "core/memory.h"
#include "core/reporter.h"

namespace Kernel {

enum class CodeMemoryOperation : u32 { Map = 0, MapToOwner = 1, Unmap = 2, UnmapFromOwner = 3 };

class KCodeMemory final
    : public KAutoObjectWithSlabHeapAndContainer<KCodeMemory, KAutoObjectWithList> {
    KERNEL_AUTOOBJECT_TRAITS(KCodeMemory, KAutoObject);

private:
    KPageLinkedList m_page_group;
    KProcess* m_owner;
    VAddr m_address;
    KLightLock m_lock;
    bool m_is_initialized;
    bool m_is_owner_mapped;
    bool m_is_mapped;

public:
    explicit KCodeMemory(KernelCore& kernel_);

    ResultCode Initialize(Core::DeviceMemory& device_memory, VAddr address, size_t size);
    void Finalize();

    ResultCode Map(VAddr address, size_t size);
    ResultCode Unmap(VAddr address, size_t size);
    ResultCode MapToOwner(VAddr address, size_t size, Svc::MemoryPermission perm);
    ResultCode UnmapFromOwner(VAddr address, size_t size);

    bool IsInitialized() const {
        return m_is_initialized;
    }
    static void PostDestroy([[maybe_unused]] uintptr_t arg) {}

    KProcess* GetOwner() const {
        return m_owner;
    }
    VAddr GetSourceAddress() {
        return m_address;
    }
    size_t GetSize() const {
        return m_is_initialized ? m_page_group.GetNumPages() * PageSize : 0;
    }
};
} // namespace Kernel