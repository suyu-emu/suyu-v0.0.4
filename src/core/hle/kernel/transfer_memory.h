// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "core/hle/kernel/memory/memory_block.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/physical_memory.h"

union ResultCode;

namespace Core::Memory {
class Memory;
}

namespace Kernel {

class KernelCore;
class Process;

/// Defines the interface for transfer memory objects.
///
/// Transfer memory is typically used for the purpose of
/// transferring memory between separate process instances,
/// thus the name.
///
class TransferMemory final : public Object {
public:
    explicit TransferMemory(KernelCore& kernel, Core::Memory::Memory& memory);
    ~TransferMemory() override;

    static constexpr HandleType HANDLE_TYPE = HandleType::TransferMemory;

    static std::shared_ptr<TransferMemory> Create(KernelCore& kernel, Core::Memory::Memory& memory,
                                                  VAddr base_address, std::size_t size,
                                                  Memory::MemoryPermission permissions);

    TransferMemory(const TransferMemory&) = delete;
    TransferMemory& operator=(const TransferMemory&) = delete;

    TransferMemory(TransferMemory&&) = delete;
    TransferMemory& operator=(TransferMemory&&) = delete;

    std::string GetTypeName() const override {
        return "TransferMemory";
    }

    std::string GetName() const override {
        return GetTypeName();
    }

    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    /// Gets a pointer to the backing block of this instance.
    const u8* GetPointer() const;

    /// Gets the size of the memory backing this instance in bytes.
    constexpr std::size_t GetSize() const {
        return size;
    }

    /// Reserves the region to be used for the transfer memory, called after the transfer memory is
    /// created.
    ResultCode Reserve();

    /// Resets the region previously used for the transfer memory, called after the transfer memory
    /// is closed.
    ResultCode Reset();

    void Finalize() override {}

private:
    /// The base address for the memory managed by this instance.
    VAddr base_address{};

    /// Size of the memory, in bytes, that this instance manages.
    std::size_t size{};

    /// The memory permissions that are applied to this instance.
    Memory::MemoryPermission owner_permissions{};

    /// The process that this transfer memory instance was created under.
    Process* owner_process{};

    Core::Memory::Memory& memory;
};

} // namespace Kernel
