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

ResultVal<Handle> HandleTable::Create(Object* obj) {
    DEBUG_ASSERT(obj != nullptr);

    switch (obj->GetHandleType()) {
    case HandleType::SharedMemory:
    case HandleType::Thread:
    case HandleType::Event:
    case HandleType::Process:
    case HandleType::ReadableEvent:
    case HandleType::WritableEvent:
    case HandleType::ClientSession:
    case HandleType::ServerSession:
    case HandleType::Session: {
        Handle handle{};
        Add(&handle, reinterpret_cast<KAutoObject*>(obj), {});
        return MakeResult<Handle>(handle);
    }
    default:
        break;
    }

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
    objects[slot] = std::move(SharedFrom(obj));

    Handle handle = generation | (slot << 15);
    return MakeResult<Handle>(handle);
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
    objects_new[slot] = obj;
    obj->Open();

    *out_handle = generation | (slot << 15);

    return RESULT_SUCCESS;
}

ResultVal<Handle> HandleTable::Duplicate(Handle handle) {
    auto object = GetGeneric(handle);
    if (object == nullptr) {
        LOG_ERROR(Kernel, "Tried to duplicate invalid handle: {:08X}", handle);
        return ResultInvalidHandle;
    }
    return Create(object);
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

    if (objects_new[slot]) {
        objects_new[slot]->Close();
    }

    objects[slot] = nullptr;
    objects_new[slot] = nullptr;

    generations[slot] = next_free_slot;
    next_free_slot = slot;

    return true;
}

bool HandleTable::IsValid(Handle handle) const {
    const std::size_t slot = GetSlot(handle);
    const u16 generation = GetGeneration(handle);
    const bool is_object_valid = (objects[slot] != nullptr) || (objects_new[slot] != nullptr);
    return slot < table_size && is_object_valid && generations[slot] == generation;
}

Object* HandleTable::GetGeneric(Handle handle) const {
    if (handle == CurrentThread) {
        return (kernel.CurrentScheduler()->GetCurrentThread());
    } else if (handle == CurrentProcess) {
        return (kernel.CurrentProcess());
    }

    if (!IsValid(handle)) {
        return nullptr;
    }
    return objects[GetSlot(handle)].get();
}

void HandleTable::Clear() {
    for (u16 i = 0; i < table_size; ++i) {
        generations[i] = static_cast<u16>(i + 1);
        objects[i] = nullptr;
        objects_new[i] = nullptr;
    }
    next_free_slot = 0;
}

} // namespace Kernel
