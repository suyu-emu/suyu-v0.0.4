// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <mutex>

#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/service/nvnflinger/hos_binder_driver_server.h"

namespace Service::Nvnflinger {

HosBinderDriverServer::HosBinderDriverServer() = default;
HosBinderDriverServer::~HosBinderDriverServer() = default;

s32 HosBinderDriverServer::RegisterBinder(std::shared_ptr<android::IBinder>&& binder) {
    std::scoped_lock lk{lock};

    last_id++;

    binders[last_id] = std::move(binder);
    refcounts[last_id] = {};

    return last_id;
}

void HosBinderDriverServer::UnregisterBinder(s32 binder_id) {
    std::scoped_lock lk{lock};

    binders.erase(binder_id);
    refcounts.erase(binder_id);
}

void HosBinderDriverServer::AdjustRefcount(s32 binder_id, s32 delta, bool is_weak) {
    std::scoped_lock lk{lock};

    auto it = refcounts.find(binder_id);
    if (it == refcounts.end()) {
        LOG_WARNING(Service_VI, "AdjustRefcount called for unknown binder id={}", binder_id);
        return;
    }

    auto& refcounts_for_id = it->second;
    s32& counter = is_weak ? refcounts_for_id.weak : refcounts_for_id.strong;
    counter += delta;
    if (counter < 0) {
        counter = 0;
    }

    if (refcounts_for_id.strong == 0 && refcounts_for_id.weak == 0) {
        binders.erase(binder_id);
        refcounts.erase(it);
    }
}

std::shared_ptr<android::IBinder> HosBinderDriverServer::TryGetBinder(s32 id) const {
    std::scoped_lock lk{lock};

    if (auto search = binders.find(id); search != binders.end()) {
        return search->second;
    }

    return {};
}

} // namespace Service::Nvnflinger
