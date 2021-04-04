// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "common/common_types.h"
#include "core/device_memory.h"
#include "core/hle/kernel/k_memory_block.h"
#include "core/hle/kernel/k_page_linked_list.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/slab_helpers.h"
#include "core/hle/result.h"

namespace Kernel {

class KernelCore;

class KSharedMemory final
    : public KAutoObjectWithSlabHeapAndContainer<KSharedMemory, KAutoObjectWithList> {
    KERNEL_AUTOOBJECT_TRAITS(KSharedMemory, KAutoObject);

public:
    explicit KSharedMemory(KernelCore& kernel);
    ~KSharedMemory() override;

    ResultCode Initialize(KernelCore& kernel_, Core::DeviceMemory& device_memory_,
                          Process* owner_process_, KPageLinkedList&& page_list_,
                          KMemoryPermission owner_permission_, KMemoryPermission user_permission_,
                          PAddr physical_address_, std::size_t size_, std::string name_);

    std::string GetTypeName() const override {
        return "SharedMemory";
    }

    std::string GetName() const override {
        return name;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::SharedMemory;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    /**
     * Maps a shared memory block to an address in the target process' address space
     * @param target_process Process on which to map the memory block
     * @param address Address in system memory to map shared memory block to
     * @param size Size of the shared memory block to map
     * @param permissions Memory block map permissions (specified by SVC field)
     */
    ResultCode Map(Process& target_process, VAddr address, std::size_t size,
                   KMemoryPermission permissions);

    /**
     * Gets a pointer to the shared memory block
     * @param offset Offset from the start of the shared memory block to get pointer
     * @return A pointer to the shared memory block from the specified offset
     */
    u8* GetPointer(std::size_t offset = 0) {
        return device_memory->GetPointer(physical_address + offset);
    }

    /**
     * Gets a pointer to the shared memory block
     * @param offset Offset from the start of the shared memory block to get pointer
     * @return A pointer to the shared memory block from the specified offset
     */
    const u8* GetPointer(std::size_t offset = 0) const {
        return device_memory->GetPointer(physical_address + offset);
    }

    virtual void Finalize() override;

    virtual bool IsInitialized() const override {
        return is_initialized;
    }
    static void PostDestroy([[maybe_unused]] uintptr_t arg) {}

private:
    Core::DeviceMemory* device_memory;
    Process* owner_process{};
    KPageLinkedList page_list;
    KMemoryPermission owner_permission{};
    KMemoryPermission user_permission{};
    PAddr physical_address{};
    std::size_t size{};
    std::shared_ptr<KResourceLimit> resource_limit;
    bool is_initialized{};
};

} // namespace Kernel
