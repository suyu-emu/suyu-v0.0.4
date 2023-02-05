// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_code_memory.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {
namespace {

constexpr bool IsValidMapCodeMemoryPermission(MemoryPermission perm) {
    return perm == MemoryPermission::ReadWrite;
}

constexpr bool IsValidMapToOwnerCodeMemoryPermission(MemoryPermission perm) {
    return perm == MemoryPermission::Read || perm == MemoryPermission::ReadExecute;
}

constexpr bool IsValidUnmapCodeMemoryPermission(MemoryPermission perm) {
    return perm == MemoryPermission::None;
}

constexpr bool IsValidUnmapFromOwnerCodeMemoryPermission(MemoryPermission perm) {
    return perm == MemoryPermission::None;
}

} // namespace

Result CreateCodeMemory(Core::System& system, Handle* out, VAddr address, size_t size) {
    LOG_TRACE(Kernel_SVC, "called, address=0x{:X}, size=0x{:X}", address, size);

    // Get kernel instance.
    auto& kernel = system.Kernel();

    // Validate address / size.
    R_UNLESS(Common::IsAligned(address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(size, PageSize), ResultInvalidSize);
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS((address < address + size), ResultInvalidCurrentMemory);

    // Create the code memory.

    KCodeMemory* code_mem = KCodeMemory::Create(kernel);
    R_UNLESS(code_mem != nullptr, ResultOutOfResource);

    // Verify that the region is in range.
    R_UNLESS(system.CurrentProcess()->PageTable().Contains(address, size),
             ResultInvalidCurrentMemory);

    // Initialize the code memory.
    R_TRY(code_mem->Initialize(system.DeviceMemory(), address, size));

    // Register the code memory.
    KCodeMemory::Register(kernel, code_mem);

    // Add the code memory to the handle table.
    R_TRY(system.CurrentProcess()->GetHandleTable().Add(out, code_mem));

    code_mem->Close();

    return ResultSuccess;
}

Result CreateCodeMemory32(Core::System& system, Handle* out, u32 address, u32 size) {
    return CreateCodeMemory(system, out, address, size);
}

Result ControlCodeMemory(Core::System& system, Handle code_memory_handle, u32 operation,
                         VAddr address, size_t size, MemoryPermission perm) {

    LOG_TRACE(Kernel_SVC,
              "called, code_memory_handle=0x{:X}, operation=0x{:X}, address=0x{:X}, size=0x{:X}, "
              "permission=0x{:X}",
              code_memory_handle, operation, address, size, perm);

    // Validate the address / size.
    R_UNLESS(Common::IsAligned(address, PageSize), ResultInvalidAddress);
    R_UNLESS(Common::IsAligned(size, PageSize), ResultInvalidSize);
    R_UNLESS(size > 0, ResultInvalidSize);
    R_UNLESS((address < address + size), ResultInvalidCurrentMemory);

    // Get the code memory from its handle.
    KScopedAutoObject code_mem =
        system.CurrentProcess()->GetHandleTable().GetObject<KCodeMemory>(code_memory_handle);
    R_UNLESS(code_mem.IsNotNull(), ResultInvalidHandle);

    // NOTE: Here, Atmosphere extends the SVC to allow code memory operations on one's own process.
    // This enables homebrew usage of these SVCs for JIT.

    // Perform the operation.
    switch (static_cast<CodeMemoryOperation>(operation)) {
    case CodeMemoryOperation::Map: {
        // Check that the region is in range.
        R_UNLESS(
            system.CurrentProcess()->PageTable().CanContain(address, size, KMemoryState::CodeOut),
            ResultInvalidMemoryRegion);

        // Check the memory permission.
        R_UNLESS(IsValidMapCodeMemoryPermission(perm), ResultInvalidNewMemoryPermission);

        // Map the memory.
        R_TRY(code_mem->Map(address, size));
    } break;
    case CodeMemoryOperation::Unmap: {
        // Check that the region is in range.
        R_UNLESS(
            system.CurrentProcess()->PageTable().CanContain(address, size, KMemoryState::CodeOut),
            ResultInvalidMemoryRegion);

        // Check the memory permission.
        R_UNLESS(IsValidUnmapCodeMemoryPermission(perm), ResultInvalidNewMemoryPermission);

        // Unmap the memory.
        R_TRY(code_mem->Unmap(address, size));
    } break;
    case CodeMemoryOperation::MapToOwner: {
        // Check that the region is in range.
        R_UNLESS(code_mem->GetOwner()->PageTable().CanContain(address, size,
                                                              KMemoryState::GeneratedCode),
                 ResultInvalidMemoryRegion);

        // Check the memory permission.
        R_UNLESS(IsValidMapToOwnerCodeMemoryPermission(perm), ResultInvalidNewMemoryPermission);

        // Map the memory to its owner.
        R_TRY(code_mem->MapToOwner(address, size, perm));
    } break;
    case CodeMemoryOperation::UnmapFromOwner: {
        // Check that the region is in range.
        R_UNLESS(code_mem->GetOwner()->PageTable().CanContain(address, size,
                                                              KMemoryState::GeneratedCode),
                 ResultInvalidMemoryRegion);

        // Check the memory permission.
        R_UNLESS(IsValidUnmapFromOwnerCodeMemoryPermission(perm), ResultInvalidNewMemoryPermission);

        // Unmap the memory from its owner.
        R_TRY(code_mem->UnmapFromOwner(address, size));
    } break;
    default:
        return ResultInvalidEnumValue;
    }

    return ResultSuccess;
}

Result ControlCodeMemory32(Core::System& system, Handle code_memory_handle, u32 operation,
                           u64 address, u64 size, MemoryPermission perm) {
    return ControlCodeMemory(system, code_memory_handle, operation, address, size, perm);
}

} // namespace Kernel::Svc
