// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel {
namespace {
constexpr u16 GetSlot(Handle handle) {
    return static_cast<u16>(handle >> 15);
}

constexpr u16 GetGeneration(Handle handle) {
    return static_cast<u16>(handle & 0x7FFF);
}
} // Anonymous namespace

HandleTable::HandleTable(KernelCore& kernel) : kernel{kernel} {
    Clear();
}

HandleTable::~HandleTable() = default;

ResultCode HandleTable::SetSize(s32 handle_table_size) {
    if (static_cast<u32>(handle_table_size) > MAX_COUNT) {
        LOG_ERROR(Kernel, "Handle table size {} is greater than {}", handle_table_size, MAX_COUNT);
        return ResultOutOfMemory;
    }

    // Values less than or equal to zero indicate to use the maximum allowable
    // size for the handle table in the actual kernel, so we ignore the given
    // value in that case, since we assume this by default unless this function
    // is called.
    if (handle_table_size > 0) {
        table_size = static_cast<u16>(handle_table_size);
    }

    return RESULT_SUCCESS;
}

ResultVal<Handle> HandleTable::Create(std::shared_ptr<Object> obj) {
    DEBUG_ASSERT(obj != nullptr);

    const u16 slot = next_free_slot;
    if (slot >= table_size) {
        LOG_ERROR(Kernel, "Unable to allocate Handle, too many slots in use.");
        return ResultHandleTableFull;
    }
    next_free_slot = generations[slot];

    const u16 generation = next_generation++;

    // Overflow count so it fits in the 15 bits dedicated to the generation in the handle.
    // Horizon OS uses zero to represent an invalid handle, so skip to 1.
    if (next_generation >= (1 << 15)) {
        next_generation = 1;
    }

    generations[slot] = generation;
    objects[slot] = std::move(obj);

    Handle handle = generation | (slot << 15);
    return MakeResult<Handle>(handle);
}

ResultVal<Handle> HandleTable::Duplicate(Handle handle) {
    std::shared_ptr<Object> object = GetGeneric(handle);
    if (object == nullptr) {
        LOG_ERROR(Kernel, "Tried to duplicate invalid handle: {:08X}", handle);
        return ResultInvalidHandle;
    }
    return Create(std::move(object));
}

ResultCode HandleTable::Close(Handle handle) {
    if (!IsValid(handle)) {
        LOG_ERROR(Kernel, "Handle is not valid! handle={:08X}", handle);
        return ResultInvalidHandle;
    }

    const u16 slot = GetSlot(handle);

    if (objects[slot].use_count() == 1) {
        objects[slot]->Finalize();
    }

    objects[slot] = nullptr;

    generations[slot] = next_free_slot;
    next_free_slot = slot;
    return RESULT_SUCCESS;
}

bool HandleTable::IsValid(Handle handle) const {
    const std::size_t slot = GetSlot(handle);
    const u16 generation = GetGeneration(handle);

    return slot < table_size && objects[slot] != nullptr && generations[slot] == generation;
}

std::shared_ptr<Object> HandleTable::GetGeneric(Handle handle) const {
    if (handle == CurrentThread) {
        return SharedFrom(kernel.CurrentScheduler()->GetCurrentThread());
    } else if (handle == CurrentProcess) {
        return SharedFrom(kernel.CurrentProcess());
    }

    if (!IsValid(handle)) {
        return nullptr;
    }
    return objects[GetSlot(handle)];
}

void HandleTable::Clear() {
    for (u16 i = 0; i < table_size; ++i) {
        generations[i] = static_cast<u16>(i + 1);
        objects[i] = nullptr;
    }
    next_free_slot = 0;
}

} // namespace Kernel
