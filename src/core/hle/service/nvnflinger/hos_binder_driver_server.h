// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <mutex>
#include <ankerl/unordered_dense.h>

#include "common/common_types.h"
#include "core/hle/service/nvnflinger/binder.h"

namespace Core {
class System;
}

namespace Service::Nvnflinger {

class HosBinderDriverServer final {
public:
    explicit HosBinderDriverServer();
    ~HosBinderDriverServer();

    s32 RegisterBinder(std::shared_ptr<android::IBinder>&& binder);
    void UnregisterBinder(s32 binder_id);

    std::shared_ptr<android::IBinder> TryGetBinder(s32 id) const;

    void AdjustRefcount(s32 binder_id, s32 delta, bool is_weak);

private:
    struct RefCounts {
        s32 strong{1};
        s32 weak{0};
    };

    mutable std::mutex lock;
    s32 last_id = 0;
    ankerl::unordered_dense::map<s32, std::shared_ptr<android::IBinder>> binders;
    ankerl::unordered_dense::map<s32, RefCounts> refcounts;
};

} // namespace Service::Nvnflinger
