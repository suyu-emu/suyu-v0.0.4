// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "core/hle/kernel/object.h"
#include "core/hle/result.h"

namespace Kernel {

class KernelCore;
class Synchronization;
class KThread;

/// Class that represents a Kernel object that a thread can be waiting on
class KSynchronizationObject : public Object {
public:
    struct ThreadListNode {
        ThreadListNode* next{};
        KThread* thread{};
    };

    [[nodiscard]] static ResultCode Wait(KernelCore& kernel, s32* out_index,
                                         KSynchronizationObject** objects, const s32 num_objects,
                                         s64 timeout);

    [[nodiscard]] virtual bool IsSignaled() const = 0;

    [[nodiscard]] std::vector<KThread*> GetWaitingThreadsForDebugging() const;

protected:
    explicit KSynchronizationObject(KernelCore& kernel);
    explicit KSynchronizationObject(KernelCore& kernel, std::string&& name);
    virtual ~KSynchronizationObject();

    void NotifyAvailable(ResultCode result);
    void NotifyAvailable() {
        return this->NotifyAvailable(RESULT_SUCCESS);
    }

private:
    ThreadListNode* thread_list_head{};
    ThreadListNode* thread_list_tail{};
};

// Specialization of DynamicObjectCast for KSynchronizationObjects
template <>
inline std::shared_ptr<KSynchronizationObject> DynamicObjectCast<KSynchronizationObject>(
    std::shared_ptr<Object> object) {
    if (object != nullptr && object->IsWaitable()) {
        return std::static_pointer_cast<KSynchronizationObject>(object);
    }
    return nullptr;
}

} // namespace Kernel
