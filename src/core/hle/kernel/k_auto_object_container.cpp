// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/k_auto_object_container.h"

namespace Kernel {

void KAutoObjectWithListContainer::Register(KAutoObjectWithList* obj) {
    KScopedLightLock lk(m_lock);

    m_object_list.insert(*obj);
}

void KAutoObjectWithListContainer::Unregister(KAutoObjectWithList* obj) {
    KScopedLightLock lk(m_lock);

    m_object_list.erase(m_object_list.iterator_to(*obj));
}

size_t KAutoObjectWithListContainer::GetOwnedCount(Process* owner) {
    KScopedLightLock lk(m_lock);

    size_t count = 0;

    for (auto& obj : m_object_list) {
        if (obj.GetOwner() == owner) {
            count++;
        }
    }

    return count;
}

} // namespace Kernel
