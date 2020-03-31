// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/caps/caps.h"
#include "core/hle/service/caps/caps_a.h"
#include "core/hle/service/caps/caps_c.h"
#include "core/hle/service/caps/caps_sc.h"
#include "core/hle/service/caps/caps_ss.h"
#include "core/hle/service/caps/caps_su.h"
#include "core/hle/service/caps/caps_u.h"
#include "core/hle/service/service.h"

namespace Service::Capture {

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<CAPS_A>()->InstallAsService(sm);
    std::make_shared<CAPS_C>()->InstallAsService(sm);
    std::make_shared<CAPS_U>()->InstallAsService(sm);
    std::make_shared<CAPS_SC>()->InstallAsService(sm);
    std::make_shared<CAPS_SS>()->InstallAsService(sm);
    std::make_shared<CAPS_SU>()->InstallAsService(sm);
}

} // namespace Service::Capture
