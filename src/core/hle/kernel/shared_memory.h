// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "common/common_types.h"
#include "core/device_memory.h"
#include "core/hle/kernel/memory/memory_block.h"
#include "core/hle/kernel/memory/page_linked_list.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/process.h"
#include "core/hle/result.h"

namespace Kernel {

class KernelCore;

class SharedMemory final : public Object {
public:
    explicit SharedMemory(KernelCore& kernel, Core::DeviceMemory& device_memory);
    ~SharedMemory() override;

    static std::shared_ptr<SharedMemory> Create(
        KernelCore& kernel, Core::DeviceMemory& device_memory, Process* owner_process,
        Memory::PageLinkedList&& page_list, Memory::MemoryPermission owner_permission,
        Memory::MemoryPermission user_permission, PAddr physical_address, std::size_t size,
        std::string name);

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
                   Memory::MemoryPermission permissions);

    /**
     * Gets a pointer to the shared memory block
     * @param offset Offset from the start of the shared memory block to get pointer
     * @return A pointer to the shared memory block from the specified offset
     */
    u8* GetPointer(std::size_t offset = 0) {
        return device_memory.GetPointer(physical_address + offset);
    }

    /**
     * Gets a pointer to the shared memory block
     * @param offset Offset from the start of the shared memory block to get pointer
     * @return A pointer to the shared memory block from the specified offset
     */
    const u8* GetPointer(std::size_t offset = 0) const {
        return device_memory.GetPointer(physical_address + offset);
    }

    void Finalize() override {}

private:
    Core::DeviceMemory& device_memory;
    Process* owner_process{};
    Memory::PageLinkedList page_list;
    Memory::MemoryPermission owner_permission{};
    Memory::MemoryPermission user_permission{};
    PAddr physical_address{};
    std::size_t size{};
    std::string name;
};

} // namespace Kernel
