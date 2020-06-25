// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/result.h"

namespace Kernel {
namespace {
constexpr std::size_t ResourceTypeToIndex(ResourceType type) {
    return static_cast<std::size_t>(type);
}
} // Anonymous namespace

ResourceLimit::ResourceLimit(KernelCore& kernel) : Object{kernel} {}
ResourceLimit::~ResourceLimit() = default;

bool ResourceLimit::Reserve(ResourceType resource, s64 amount) {
    return Reserve(resource, amount, 10000000000);
}

bool ResourceLimit::Reserve(ResourceType resource, s64 amount, u64 timeout) {
    const std::size_t index{ResourceTypeToIndex(resource)};

    s64 new_value = current[index] + amount;
    if (new_value > limit[index] && available[index] + amount <= limit[index]) {
        // TODO(bunnei): This is wrong for multicore, we should wait the calling thread for timeout
        new_value = current[index] + amount;
    }

    if (new_value <= limit[index]) {
        current[index] = new_value;
        return true;
    }
    return false;
}

void ResourceLimit::Release(ResourceType resource, u64 amount) {
    Release(resource, amount, amount);
}

void ResourceLimit::Release(ResourceType resource, u64 used_amount, u64 available_amount) {
    const std::size_t index{ResourceTypeToIndex(resource)};

    current[index] -= used_amount;
    available[index] -= available_amount;
}

std::shared_ptr<ResourceLimit> ResourceLimit::Create(KernelCore& kernel) {
    return std::make_shared<ResourceLimit>(kernel);
}

s64 ResourceLimit::GetCurrentResourceValue(ResourceType resource) const {
    return limit.at(ResourceTypeToIndex(resource)) - current.at(ResourceTypeToIndex(resource));
}

s64 ResourceLimit::GetMaxResourceValue(ResourceType resource) const {
    return limit.at(ResourceTypeToIndex(resource));
}

ResultCode ResourceLimit::SetLimitValue(ResourceType resource, s64 value) {
    const std::size_t index{ResourceTypeToIndex(resource)};
    if (current[index] <= value) {
        limit[index] = value;
        return RESULT_SUCCESS;
    } else {
        LOG_ERROR(Kernel, "Limit value is too large! resource={}, value={}, index={}",
                  static_cast<u32>(resource), value, index);
        return ERR_INVALID_STATE;
    }
}
} // namespace Kernel
