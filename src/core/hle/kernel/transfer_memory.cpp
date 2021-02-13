// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/k_resource_limit.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory/page_table.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/transfer_memory.h"
#include "core/hle/result.h"
#include "core/memory.h"

namespace Kernel {

TransferMemory::TransferMemory(KernelCore& kernel, Core::Memory::Memory& memory)
    : Object{kernel}, memory{memory} {}

TransferMemory::~TransferMemory() {
    // Release memory region when transfer memory is destroyed
    Reset();
    owner_process->GetResourceLimit()->Release(LimitableResource::TransferMemory, 1);
}

std::shared_ptr<TransferMemory> TransferMemory::Create(KernelCore& kernel,
                                                       Core::Memory::Memory& memory,
                                                       VAddr base_address, std::size_t size,
                                                       Memory::MemoryPermission permissions) {
    std::shared_ptr<TransferMemory> transfer_memory{
        std::make_shared<TransferMemory>(kernel, memory)};

    transfer_memory->base_address = base_address;
    transfer_memory->size = size;
    transfer_memory->owner_permissions = permissions;
    transfer_memory->owner_process = kernel.CurrentProcess();

    return transfer_memory;
}

const u8* TransferMemory::GetPointer() const {
    return memory.GetPointer(base_address);
}

ResultCode TransferMemory::Reserve() {
    return owner_process->PageTable().ReserveTransferMemory(base_address, size, owner_permissions);
}

ResultCode TransferMemory::Reset() {
    return owner_process->PageTable().ResetTransferMemory(base_address, size);
}

} // namespace Kernel
