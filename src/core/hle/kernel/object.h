// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "common/common_types.h"

namespace Kernel {

class KernelCore;

using Handle = u32;

enum class HandleType : u32 {
    Unknown,
    Event,
    WritableEvent,
    ReadableEvent,
    SharedMemory,
    TransferMemory,
    Thread,
    Process,
    ResourceLimit,
    ClientPort,
    ServerPort,
    ClientSession,
    ServerSession,
    Session,
};

class Object : NonCopyable, public std::enable_shared_from_this<Object> {
public:
    explicit Object(KernelCore& kernel_);
    explicit Object(KernelCore& kernel_, std::string&& name_);
    virtual ~Object();

    /// Returns a unique identifier for the object. For debugging purposes only.
    u32 GetObjectId() const {
        return object_id.load(std::memory_order_relaxed);
    }

    virtual std::string GetTypeName() const {
        return "[BAD KERNEL OBJECT TYPE]";
    }
    virtual std::string GetName() const {
        return name;
    }
    virtual HandleType GetHandleType() const = 0;

    void Close() {
        // TODO(bunnei): This is a placeholder to decrement the reference count, which we will use
        // when we implement KAutoObject instead of using shared_ptr.
    }

    /**
     * Check if a thread can wait on the object
     * @return True if a thread can wait on the object, otherwise false
     */
    bool IsWaitable() const;

    virtual void Finalize() = 0;

protected:
    /// The kernel instance this object was created under.
    KernelCore& kernel;

private:
    std::atomic<u32> object_id{0};
    std::string name;
};

template <typename T>
std::shared_ptr<T> SharedFrom(T* raw) {
    if (raw == nullptr)
        return nullptr;
    return std::static_pointer_cast<T>(raw->shared_from_this());
}

/**
 * Attempts to downcast the given Object pointer to a pointer to T.
 * @return Derived pointer to the object, or `nullptr` if `object` isn't of type T.
 */
template <typename T>
inline std::shared_ptr<T> DynamicObjectCast(std::shared_ptr<Object> object) {
    if (object != nullptr && object->GetHandleType() == T::HANDLE_TYPE) {
        return std::static_pointer_cast<T>(object);
    }
    return nullptr;
}

} // namespace Kernel
