// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include "common/common_types.h"
#include "core/hle/kernel/k_auto_object.h"
#include "core/hle/kernel/k_spin_lock.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/object.h"
#include "core/hle/result.h"

namespace Kernel {

class KernelCore;

enum KernelHandle : Handle {
    InvalidHandle = 0,
    CurrentThread = 0xFFFF8000,
    CurrentProcess = 0xFFFF8001,
};

/**
 * This class allows the creation of Handles, which are references to objects that can be tested
 * for validity and looked up. Here they are used to pass references to kernel objects to/from the
 * emulated process. it has been designed so that it follows the same handle format and has
 * approximately the same restrictions as the handle manager in the CTR-OS.
 *
 * Handles contain two sub-fields: a slot index (bits 31:15) and a generation value (bits 14:0).
 * The slot index is used to index into the arrays in this class to access the data corresponding
 * to the Handle.
 *
 * To prevent accidental use of a freed Handle whose slot has already been reused, a global counter
 * is kept and incremented every time a Handle is created. This is the Handle's "generation". The
 * value of the counter is stored into the Handle as well as in the handle table (in the
 * "generations" array). When looking up a handle, the Handle's generation must match with the
 * value stored on the class, otherwise the Handle is considered invalid.
 *
 * To find free slots when allocating a Handle without needing to scan the entire object array, the
 * generations field of unallocated slots is re-purposed as a linked list of indices to free slots.
 * When a Handle is created, an index is popped off the list and used for the new Handle. When it
 * is destroyed, it is again pushed onto the list to be re-used by the next allocation. It is
 * likely that this allocation strategy differs from the one used in CTR-OS, but this hasn't been
 * verified and isn't likely to cause any problems.
 */
class HandleTable final : NonCopyable {
public:
    /// This is the maximum limit of handles allowed per process in Horizon
    static constexpr std::size_t MAX_COUNT = 1024;

    explicit HandleTable(KernelCore& kernel);
    ~HandleTable();

    /**
     * Sets the number of handles that may be in use at one time
     * for this handle table.
     *
     * @param handle_table_size The desired size to limit the handle table to.
     *
     * @returns an error code indicating if initialization was successful.
     *          If initialization was not successful, then ERR_OUT_OF_MEMORY
     *          will be returned.
     *
     * @pre handle_table_size must be within the range [0, 1024]
     */
    ResultCode SetSize(s32 handle_table_size);

    /**
     * Returns a new handle that points to the same object as the passed in handle.
     * @return The duplicated Handle or one of the following errors:
     *           - `ERR_INVALID_HANDLE`: an invalid handle was passed in.
     *           - Any errors returned by `Create()`.
     */
    ResultVal<Handle> Duplicate(Handle handle);

    /**
     * Closes a handle, removing it from the table and decreasing the object's ref-count.
     * @return `RESULT_SUCCESS` or one of the following errors:
     *           - `ERR_INVALID_HANDLE`: an invalid handle was passed in.
     */
    bool Remove(Handle handle);

    /// Checks if a handle is valid and points to an existing object.
    bool IsValid(Handle handle) const;

    template <typename T = KAutoObject>
    KAutoObject* GetObjectImpl(Handle handle) const {
        if (!IsValid(handle)) {
            return nullptr;
        }

        auto* obj = objects[static_cast<u16>(handle >> 15)];
        return obj->DynamicCast<T*>();
    }

    template <typename T = KAutoObject>
    KScopedAutoObject<T> GetObject(Handle handle) const {
        if (handle == CurrentThread) {
            return kernel.CurrentScheduler()->GetCurrentThread()->DynamicCast<T*>();
        } else if (handle == CurrentProcess) {
            return kernel.CurrentProcess()->DynamicCast<T*>();
        }

        if (!IsValid(handle)) {
            return nullptr;
        }

        auto* obj = objects[static_cast<u16>(handle >> 15)];
        return obj->DynamicCast<T*>();
    }

    template <typename T = KAutoObject>
    KScopedAutoObject<T> GetObjectWithoutPseudoHandle(Handle handle) const {
        if (!IsValid(handle)) {
            return nullptr;
        }
        auto* obj = objects[static_cast<u16>(handle >> 15)];
        return obj->DynamicCast<T*>();
    }

    /// Closes all handles held in this table.
    void Clear();

    // NEW IMPL

    template <typename T>
    ResultCode Add(Handle* out_handle, T* obj) {
        static_assert(std::is_base_of<KAutoObject, T>::value);
        return this->Add(out_handle, obj, obj->GetTypeObj().GetClassToken());
    }

    ResultCode Add(Handle* out_handle, KAutoObject* obj, u16 type);

    template <typename T>
    bool GetMultipleObjects(T** out, const Handle* handles, size_t num_handles) const {
        // Try to convert and open all the handles.
        size_t num_opened;
        {
            // Lock the table.
            KScopedSpinLock lk(lock);
            for (num_opened = 0; num_opened < num_handles; num_opened++) {
                // Get the current handle.
                const auto cur_handle = handles[num_opened];

                // Get the object for the current handle.
                KAutoObject* cur_object = this->GetObjectImpl(cur_handle);
                if (cur_object == nullptr) {
                    break;
                }

                // Cast the current object to the desired type.
                T* cur_t = cur_object->DynamicCast<T*>();
                if (cur_t == nullptr) {
                    break;
                }

                // Open a reference to the current object.
                cur_t->Open();
                out[num_opened] = cur_t;
            }
        }

        // If we converted every object, succeed.
        if (num_opened == num_handles) {
            return true;
        }

        // If we didn't convert entry object, close the ones we opened.
        for (size_t i = 0; i < num_opened; i++) {
            out[i]->Close();
        }

        return false;
    }

private:
    /// Stores the Object referenced by the handle or null if the slot is empty.
    std::array<KAutoObject*, MAX_COUNT> objects{};

    /**
     * The value of `next_generation` when the handle was created, used to check for validity. For
     * empty slots, contains the index of the next free slot in the list.
     */
    std::array<u16, MAX_COUNT> generations;

    /**
     * The limited size of the handle table. This can be specified by process
     * capabilities in order to restrict the overall number of handles that
     * can be created in a process instance
     */
    u16 table_size = static_cast<u16>(MAX_COUNT);

    /**
     * Global counter of the number of created handles. Stored in `generations` when a handle is
     * created, and wraps around to 1 when it hits 0x8000.
     */
    u16 next_generation = 1;

    /// Head of the free slots linked list.
    u16 next_free_slot = 0;

    mutable KSpinLock lock;

    /// Underlying kernel instance that this handle table operates under.
    KernelCore& kernel;
};

} // namespace Kernel
