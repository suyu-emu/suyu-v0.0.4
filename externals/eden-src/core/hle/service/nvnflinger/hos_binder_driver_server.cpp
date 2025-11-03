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
    refcounts[last_id] = {}; // strong = 1, weak = 0

    return last_id;
}

void HosBinderDriverServer::UnregisterBinder(s32 binder_id) {
    std::scoped_lock lk{lock};

    binders.erase(binder_id);
    refcounts.erase(binder_id);
}

void HosBinderDriverServer::AdjustRefcount(s32 binder_id, s32 delta, bool is_weak) {
    std::scoped_lock lk{lock};

    auto search_rc = refcounts.find(binder_id);
    if (search_rc == refcounts.end()) {
        LOG_WARNING(Service_VI, "AdjustRefcount called for unknown binder id {}", binder_id);
        return;
    }

    auto& rc = search_rc->second;
    s32& counter = is_weak ? rc.weak : rc.strong;
    counter += delta;

    if (counter < 0)
        counter = 0;

    if (rc.strong == 0 && rc.weak == 0) {
        binders.erase(binder_id);
        refcounts.erase(search_rc);
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
