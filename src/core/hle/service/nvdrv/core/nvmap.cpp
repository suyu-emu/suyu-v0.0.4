// Copyright 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/hle/service/nvdrv/core/nvmap.h"
#include "core/memory.h"

using Core::Memory::YUZU_PAGESIZE;

namespace Service::Nvidia::NvCore {
NvMap::Handle::Handle(u64 size, Id id) : size(size), aligned_size(size), orig_size(size), id(id) {}

NvResult NvMap::Handle::Alloc(Flags pFlags, u32 pAlign, u8 pKind, u64 pAddress) {
    std::scoped_lock lock(mutex);

    // Handles cannot be allocated twice
    if (allocated) [[unlikely]]
        return NvResult::AccessDenied;

    flags = pFlags;
    kind = pKind;
    align = pAlign < YUZU_PAGESIZE ? YUZU_PAGESIZE : pAlign;

    // This flag is only applicable for handles with an address passed
    if (pAddress)
        flags.keep_uncached_after_free = 0;
    else
        LOG_CRITICAL(Service_NVDRV,
                     "Mapping nvmap handles without a CPU side address is unimplemented!");

    size = Common::AlignUp(size, YUZU_PAGESIZE);
    aligned_size = Common::AlignUp(size, align);
    address = pAddress;

    // TODO: pin init

    allocated = true;

    return NvResult::Success;
}

NvResult NvMap::Handle::Duplicate(bool internal_session) {
    // Unallocated handles cannot be duplicated as duplication requires memory accounting (in HOS)
    if (!allocated) [[unlikely]]
        return NvResult::BadValue;

    std::scoped_lock lock(mutex);

    // If we internally use FromId the duplication tracking of handles won't work accurately due to
    // us not implementing per-process handle refs.
    if (internal_session)
        internal_dupes++;
    else
        dupes++;

    return NvResult::Success;
}

NvMap::NvMap() = default;

void NvMap::AddHandle(std::shared_ptr<Handle> handleDesc) {
    std::scoped_lock lock(handles_lock);

    handles.emplace(handleDesc->id, std::move(handleDesc));
}

void NvMap::UnmapHandle(Handle& handleDesc) {
    // Remove pending unmap queue entry if needed
    if (handleDesc.unmap_queue_entry) {
        unmap_queue.erase(*handleDesc.unmap_queue_entry);
        handleDesc.unmap_queue_entry.reset();
    }

    // Free and unmap the handle from the SMMU
    /*
    state.soc->smmu.Unmap(handleDesc.pin_virt_address, static_cast<u32>(handleDesc.aligned_size));
    smmuAllocator.Free(handleDesc.pin_virt_address, static_cast<u32>(handleDesc.aligned_size));
    handleDesc.pin_virt_address = 0;
    */
}

bool NvMap::TryRemoveHandle(const Handle& handleDesc) {
    // No dupes left, we can remove from handle map
    if (handleDesc.dupes == 0 && handleDesc.internal_dupes == 0) {
        std::scoped_lock lock(handles_lock);

        auto it{handles.find(handleDesc.id)};
        if (it != handles.end())
            handles.erase(it);

        return true;
    } else {
        return false;
    }
}

NvResult NvMap::CreateHandle(u64 size, std::shared_ptr<NvMap::Handle>& result_out) {
    if (!size) [[unlikely]]
        return NvResult::BadValue;

    u32 id{next_handle_id.fetch_add(HandleIdIncrement, std::memory_order_relaxed)};
    auto handleDesc{std::make_shared<Handle>(size, id)};
    AddHandle(handleDesc);

    result_out = handleDesc;
    return NvResult::Success;
}

std::shared_ptr<NvMap::Handle> NvMap::GetHandle(Handle::Id handle) {
    std::scoped_lock lock(handles_lock);
    try {
        return handles.at(handle);
    } catch ([[maybe_unused]] std::out_of_range& e) {
        return nullptr;
    }
}

u32 NvMap::PinHandle(NvMap::Handle::Id handle) {
    UNIMPLEMENTED_MSG("pinning");
    return 0;
    /*
    auto handleDesc{GetHandle(handle)};
    if (!handleDesc)
        [[unlikely]] return 0;

    std::scoped_lock lock(handleDesc->mutex);
    if (!handleDesc->pins) {
        // If we're in the unmap queue we can just remove ourselves and return since we're already
        // mapped
        {
            // Lock now to prevent our queue entry from being removed for allocation in-between the
            // following check and erase
            std::scoped_lock queueLock(unmap_queue_lock);
            if (handleDesc->unmap_queue_entry) {
                unmap_queue.erase(*handleDesc->unmap_queue_entry);
                handleDesc->unmap_queue_entry.reset();

                handleDesc->pins++;
                return handleDesc->pin_virt_address;
            }
        }

        // If not then allocate some space and map it
        u32 address{};
        while (!(address = smmuAllocator.Allocate(static_cast<u32>(handleDesc->aligned_size)))) {
            // Free handles until the allocation succeeds
            std::scoped_lock queueLock(unmap_queue_lock);
            if (auto freeHandleDesc{unmap_queue.front()}) {
                // Handles in the unmap queue are guaranteed not to be pinned so don't bother
                // checking if they are before unmapping
                std::scoped_lock freeLock(freeHandleDesc->mutex);
                if (handleDesc->pin_virt_address)
                    UnmapHandle(*freeHandleDesc);
            } else {
                LOG_CRITICAL(Service_NVDRV, "Ran out of SMMU address space!");
            }
        }

        state.soc->smmu.Map(address, handleDesc->GetPointer(),
                            static_cast<u32>(handleDesc->aligned_size));
        handleDesc->pin_virt_address = address;
    }

    handleDesc->pins++;
    return handleDesc->pin_virt_address;
    */
}

void NvMap::UnpinHandle(Handle::Id handle) {
    UNIMPLEMENTED_MSG("Unpinning");
    /*
    auto handleDesc{GetHandle(handle)};
    if (!handleDesc)
        return;

    std::scoped_lock lock(handleDesc->mutex);
    if (--handleDesc->pins < 0) {
        LOG_WARNING(Service_NVDRV, "Pin count imbalance detected!");
    } else if (!handleDesc->pins) {
        std::scoped_lock queueLock(unmap_queue_lock);

        // Add to the unmap queue allowing this handle's memory to be freed if needed
        unmap_queue.push_back(handleDesc);
        handleDesc->unmap_queue_entry = std::prev(unmap_queue.end());
    }
    */
}

std::optional<NvMap::FreeInfo> NvMap::FreeHandle(Handle::Id handle, bool internal_session) {
    std::weak_ptr<Handle> hWeak{GetHandle(handle)};
    FreeInfo freeInfo;

    // We use a weak ptr here so we can tell when the handle has been freed and report that back to
    // guest
    if (auto handleDesc = hWeak.lock()) {
        std::scoped_lock lock(handleDesc->mutex);

        if (internal_session) {
            if (--handleDesc->internal_dupes < 0)
                LOG_WARNING(Service_NVDRV, "Internal duplicate count imbalance detected!");
        } else {
            if (--handleDesc->dupes < 0) {
                LOG_WARNING(Service_NVDRV, "User duplicate count imbalance detected!");
            } else if (handleDesc->dupes == 0) {
                // Force unmap the handle
                if (handleDesc->pin_virt_address) {
                    std::scoped_lock queueLock(unmap_queue_lock);
                    UnmapHandle(*handleDesc);
                }

                handleDesc->pins = 0;
            }
        }

        // Try to remove the shared ptr to the handle from the map, if nothing else is using the
        // handle then it will now be freed when `handleDesc` goes out of scope
        if (TryRemoveHandle(*handleDesc))
            LOG_ERROR(Service_NVDRV, "Removed nvmap handle: {}", handle);
        else
            LOG_ERROR(Service_NVDRV,
                      "Tried to free nvmap handle: {} but didn't as it still has duplicates",
                      handle);

        freeInfo = {
            .address = handleDesc->address,
            .size = handleDesc->size,
            .was_uncached = handleDesc->flags.map_uncached.Value() != 0,
        };
    } else {
        return std::nullopt;
    }

    // Handle hasn't been freed from memory, set address to 0 to mark that the handle wasn't freed
    if (!hWeak.expired()) {
        LOG_ERROR(Service_NVDRV, "nvmap handle: {} wasn't freed as it is still in use", handle);
        freeInfo.address = 0;
    }

    return freeInfo;
}

} // namespace Service::Nvidia::NvCore
