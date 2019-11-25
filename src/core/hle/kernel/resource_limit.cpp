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

std::shared_ptr<ResourceLimit> ResourceLimit::Create(KernelCore& kernel) {
    return std::make_shared<ResourceLimit>(kernel);
}

s64 ResourceLimit::GetCurrentResourceValue(ResourceType resource) const {
    return values.at(ResourceTypeToIndex(resource));
}

s64 ResourceLimit::GetMaxResourceValue(ResourceType resource) const {
    return limits.at(ResourceTypeToIndex(resource));
}

ResultCode ResourceLimit::SetLimitValue(ResourceType resource, s64 value) {
    const auto index = ResourceTypeToIndex(resource);

    if (value < values[index]) {
        return ERR_INVALID_STATE;
    }

    values[index] = value;
    return RESULT_SUCCESS;
}
} // namespace Kernel
