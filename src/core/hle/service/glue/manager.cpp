// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/glue/errors.h"
#include "core/hle/service/glue/manager.h"

namespace Service::Glue {

ARPManager::ARPManager() = default;

ARPManager::~ARPManager() = default;

ResultVal<ApplicationLaunchProperty> ARPManager::GetLaunchProperty(u64 title_id) const {
    if (title_id == 0) {
        return ERR_TITLE_ID_ZERO;
    }

    const auto iter = entries.find(title_id);
    if (iter == entries.end()) {
        return ERR_NONEXISTENT;
    }

    return MakeResult<ApplicationLaunchProperty>(iter->second.launch);
}

ResultVal<std::vector<u8>> ARPManager::GetControlProperty(u64 title_id) const {
    if (title_id == 0) {
        return ERR_TITLE_ID_ZERO;
    }

    const auto iter = entries.find(title_id);
    if (iter == entries.end()) {
        return ERR_NONEXISTENT;
    }

    return MakeResult<std::vector<u8>>(iter->second.control);
}

ResultCode ARPManager::Register(u64 title_id, ApplicationLaunchProperty launch,
                                std::vector<u8> control) {
    if (title_id == 0) {
        return ERR_TITLE_ID_ZERO;
    }

    const auto iter = entries.find(title_id);
    if (iter != entries.end()) {
        return ERR_ALREADY_ISSUED;
    }

    entries.insert_or_assign(title_id, MapEntry{launch, std::move(control)});
    return RESULT_SUCCESS;
}

ResultCode ARPManager::Unregister(u64 title_id) {
    if (title_id == 0) {
        return ERR_TITLE_ID_ZERO;
    }

    const auto iter = entries.find(title_id);
    if (iter == entries.end()) {
        return ERR_NONEXISTENT;
    }

    entries.erase(iter);
    return RESULT_SUCCESS;
}

void ARPManager::ResetAll() {
    entries.clear();
}

} // namespace Service::Glue
