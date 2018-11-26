// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "common/common_types.h"
#include "core/hle/kernel/object.h"

union ResultCode;

namespace Kernel {

class KernelCore;

enum class ResourceType : u32 {
    PhysicalMemory,
    Threads,
    Events,
    TransferMemory,
    Sessions,

    // Used as a count, not an actual type.
    ResourceTypeCount
};

constexpr bool IsValidResourceType(ResourceType type) {
    return type < ResourceType::ResourceTypeCount;
}

class ResourceLimit final : public Object {
public:
    /**
     * Creates a resource limit object.
     */
    static SharedPtr<ResourceLimit> Create(KernelCore& kernel, std::string name = "Unknown");

    std::string GetTypeName() const override {
        return "ResourceLimit";
    }
    std::string GetName() const override {
        return name;
    }

    static const HandleType HANDLE_TYPE = HandleType::ResourceLimit;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    /**
     * Gets the current value for the specified resource.
     * @param resource Requested resource type
     * @returns The current value of the resource type
     */
    s64 GetCurrentResourceValue(ResourceType resource) const;

    /**
     * Gets the max value for the specified resource.
     * @param resource Requested resource type
     * @returns The max value of the resource type
     */
    s64 GetMaxResourceValue(ResourceType resource) const;

    /**
     * Sets the limit value for a given resource type.
     *
     * @param resource The resource type to apply the limit to.
     * @param value    The limit to apply to the given resource type.
     *
     * @return A result code indicating if setting the limit value
     *         was successful or not.
     *
     * @note The supplied limit value *must* be greater than or equal to
     *       the current resource value for the given resource type,
     *       otherwise ERR_INVALID_STATE will be returned.
     */
    ResultCode SetLimitValue(ResourceType resource, s64 value);

private:
    explicit ResourceLimit(KernelCore& kernel);
    ~ResourceLimit() override;

    // TODO(Subv): Increment resource limit current values in their respective Kernel::T::Create
    // functions
    //
    // Currently we have no way of distinguishing if a Create was called by the running application,
    // or by a service module. Approach this once we have separated the service modules into their
    // own processes

    using ResourceArray =
        std::array<s64, static_cast<std::size_t>(ResourceType::ResourceTypeCount)>;

    /// Maximum values a resource type may reach.
    ResourceArray limits{};
    /// Current resource limit values.
    ResourceArray values{};

    /// Name of resource limit object.
    std::string name;
};

} // namespace Kernel
