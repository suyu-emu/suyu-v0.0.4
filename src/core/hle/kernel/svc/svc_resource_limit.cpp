// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_resource_limit.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

Result CreateResourceLimit(Core::System& system, Handle* out_handle) {
    LOG_DEBUG(Kernel_SVC, "called");

    // Create a new resource limit.
    auto& kernel = system.Kernel();
    KResourceLimit* resource_limit = KResourceLimit::Create(kernel);
    R_UNLESS(resource_limit != nullptr, ResultOutOfResource);

    // Ensure we don't leak a reference to the limit.
    SCOPE_EXIT({ resource_limit->Close(); });

    // Initialize the resource limit.
    resource_limit->Initialize(&system.CoreTiming());

    // Register the limit.
    KResourceLimit::Register(kernel, resource_limit);

    // Add the limit to the handle table.
    R_TRY(kernel.CurrentProcess()->GetHandleTable().Add(out_handle, resource_limit));

    return ResultSuccess;
}

Result GetResourceLimitLimitValue(Core::System& system, u64* out_limit_value,
                                  Handle resource_limit_handle, LimitableResource which) {
    LOG_DEBUG(Kernel_SVC, "called, resource_limit_handle={:08X}, which={}", resource_limit_handle,
              which);

    // Validate the resource.
    R_UNLESS(IsValidResourceType(which), ResultInvalidEnumValue);

    // Get the resource limit.
    auto& kernel = system.Kernel();
    KScopedAutoObject resource_limit =
        kernel.CurrentProcess()->GetHandleTable().GetObject<KResourceLimit>(resource_limit_handle);
    R_UNLESS(resource_limit.IsNotNull(), ResultInvalidHandle);

    // Get the limit value.
    *out_limit_value = resource_limit->GetLimitValue(which);

    return ResultSuccess;
}

Result GetResourceLimitCurrentValue(Core::System& system, u64* out_current_value,
                                    Handle resource_limit_handle, LimitableResource which) {
    LOG_DEBUG(Kernel_SVC, "called, resource_limit_handle={:08X}, which={}", resource_limit_handle,
              which);

    // Validate the resource.
    R_UNLESS(IsValidResourceType(which), ResultInvalidEnumValue);

    // Get the resource limit.
    auto& kernel = system.Kernel();
    KScopedAutoObject resource_limit =
        kernel.CurrentProcess()->GetHandleTable().GetObject<KResourceLimit>(resource_limit_handle);
    R_UNLESS(resource_limit.IsNotNull(), ResultInvalidHandle);

    // Get the current value.
    *out_current_value = resource_limit->GetCurrentValue(which);

    return ResultSuccess;
}

Result SetResourceLimitLimitValue(Core::System& system, Handle resource_limit_handle,
                                  LimitableResource which, u64 limit_value) {
    LOG_DEBUG(Kernel_SVC, "called, resource_limit_handle={:08X}, which={}, limit_value={}",
              resource_limit_handle, which, limit_value);

    // Validate the resource.
    R_UNLESS(IsValidResourceType(which), ResultInvalidEnumValue);

    // Get the resource limit.
    auto& kernel = system.Kernel();
    KScopedAutoObject resource_limit =
        kernel.CurrentProcess()->GetHandleTable().GetObject<KResourceLimit>(resource_limit_handle);
    R_UNLESS(resource_limit.IsNotNull(), ResultInvalidHandle);

    // Set the limit value.
    R_TRY(resource_limit->SetLimitValue(which, limit_value));

    return ResultSuccess;
}

} // namespace Kernel::Svc
