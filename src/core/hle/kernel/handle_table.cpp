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

ResultCode HandleTable::Add(Handle* out_handle, KAutoObject* obj, u16 type) {
    ASSERT(obj != nullptr);

    const u16 slot = next_free_slot;
    if (slot >= table_size) {
        LOG_ERROR(Kernel, "Unable to allocate Handle, too many slots in use.");
        return ResultOutOfHandles;
    }
    next_free_slot = generations[slot];

    const u16 generation = next_generation++;

    // Overflow count so it fits in the 15 bits dedicated to the generation in the handle.
    // Horizon OS uses zero to represent an invalid handle, so skip to 1.
    if (next_generation >= (1 << 15)) {
        next_generation = 1;
    }

    generations[slot] = generation;
    objects[slot] = obj;
    obj->Open();

    *out_handle = generation | (slot << 15);

    return RESULT_SUCCESS;
}

ResultVal<Handle> HandleTable::Duplicate(Handle handle) {
    auto object = GetObject(handle);
    if (object.IsNull()) {
        LOG_ERROR(Kernel, "Tried to duplicate invalid handle: {:08X}", handle);
        return ResultInvalidHandle;
    }

    Handle out_handle{};
    R_TRY(Add(&out_handle, object.GetPointerUnsafe()));

    return MakeResult(out_handle);
}

bool HandleTable::Remove(Handle handle) {
    if (!IsValid(handle)) {
        LOG_ERROR(Kernel, "Handle is not valid! handle={:08X}", handle);
        return {};
    }

    const u16 slot = GetSlot(handle);

    if (objects[slot]) {
        objects[slot]->Close();
    }

    objects[slot] = nullptr;

    generations[slot] = next_free_slot;
    next_free_slot = slot;

    return true;
}

bool HandleTable::IsValid(Handle handle) const {
    const std::size_t slot = GetSlot(handle);
    const u16 generation = GetGeneration(handle);
    const bool is_object_valid = (objects[slot] != nullptr);
    return slot < table_size && is_object_valid && generations[slot] == generation;
}

void HandleTable::Clear() {
    for (u16 i = 0; i < table_size; ++i) {
        generations[i] = static_cast<u16>(i + 1);
        objects[i] = nullptr;
    }
    next_free_slot = 0;
}

} // namespace Kernel
