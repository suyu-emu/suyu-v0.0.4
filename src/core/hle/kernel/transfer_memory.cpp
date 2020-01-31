// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/kernel/transfer_memory.h"
#include "core/hle/result.h"
#include "core/memory.h"

namespace Kernel {

TransferMemory::TransferMemory(KernelCore& kernel, Memory::Memory& memory)
    : Object{kernel}, memory{memory} {}

TransferMemory::~TransferMemory() {
    // Release memory region when transfer memory is destroyed
    Reset();
}

std::shared_ptr<TransferMemory> TransferMemory::Create(KernelCore& kernel, Memory::Memory& memory,
                                                       VAddr base_address, u64 size,
                                                       MemoryPermission permissions) {
    std::shared_ptr<TransferMemory> transfer_memory{
        std::make_shared<TransferMemory>(kernel, memory)};

    transfer_memory->base_address = base_address;
    transfer_memory->memory_size = size;
    transfer_memory->owner_permissions = permissions;
    transfer_memory->owner_process = kernel.CurrentProcess();

    return transfer_memory;
}

const u8* TransferMemory::GetPointer() const {
    return memory.GetPointer(base_address);
}

u64 TransferMemory::GetSize() const {
    return memory_size;
}

ResultCode TransferMemory::MapMemory(VAddr address, u64 size, MemoryPermission permissions) {
    if (memory_size != size) {
        return ERR_INVALID_SIZE;
    }

    if (owner_permissions != permissions) {
        return ERR_INVALID_STATE;
    }

    if (is_mapped) {
        return ERR_INVALID_STATE;
    }

    backing_block = std::make_shared<PhysicalMemory>(size);

    const auto map_state = owner_permissions == MemoryPermission::None
                               ? MemoryState::TransferMemoryIsolated
                               : MemoryState::TransferMemory;
    auto& vm_manager = owner_process->VMManager();
    const auto map_result = vm_manager.MapMemoryBlock(address, backing_block, 0, size, map_state);
    if (map_result.Failed()) {
        return map_result.Code();
    }

    is_mapped = true;
    return RESULT_SUCCESS;
}

ResultCode TransferMemory::Reserve() {
    auto& vm_manager{owner_process->VMManager()};
    const auto check_range_result{vm_manager.CheckRangeState(
        base_address, memory_size, MemoryState::FlagTransfer | MemoryState::FlagMemoryPoolAllocated,
        MemoryState::FlagTransfer | MemoryState::FlagMemoryPoolAllocated, VMAPermission::All,
        VMAPermission::ReadWrite, MemoryAttribute::Mask, MemoryAttribute::None,
        MemoryAttribute::IpcAndDeviceMapped)};

    if (check_range_result.Failed()) {
        return check_range_result.Code();
    }

    auto [state_, permissions_, attribute] = *check_range_result;

    if (const auto result{vm_manager.ReprotectRange(
            base_address, memory_size, SharedMemory::ConvertPermissions(owner_permissions))};
        result.IsError()) {
        return result;
    }

    return vm_manager.SetMemoryAttribute(base_address, memory_size, MemoryAttribute::Mask,
                                         attribute | MemoryAttribute::Locked);
}

ResultCode TransferMemory::Reset() {
    auto& vm_manager{owner_process->VMManager()};
    if (const auto result{vm_manager.CheckRangeState(
            base_address, memory_size,
            MemoryState::FlagTransfer | MemoryState::FlagMemoryPoolAllocated,
            MemoryState::FlagTransfer | MemoryState::FlagMemoryPoolAllocated, VMAPermission::None,
            VMAPermission::None, MemoryAttribute::Mask, MemoryAttribute::Locked,
            MemoryAttribute::IpcAndDeviceMapped)};
        result.Failed()) {
        return result.Code();
    }

    if (const auto result{
            vm_manager.ReprotectRange(base_address, memory_size, VMAPermission::ReadWrite)};
        result.IsError()) {
        return result;
    }

    return vm_manager.SetMemoryAttribute(base_address, memory_size, MemoryAttribute::Mask,
                                         MemoryAttribute::None);
}

ResultCode TransferMemory::UnmapMemory(VAddr address, u64 size) {
    if (memory_size != size) {
        return ERR_INVALID_SIZE;
    }

    auto& vm_manager = owner_process->VMManager();
    const auto result = vm_manager.UnmapRange(address, size);

    if (result.IsError()) {
        return result;
    }

    is_mapped = false;
    return RESULT_SUCCESS;
}

} // namespace Kernel
