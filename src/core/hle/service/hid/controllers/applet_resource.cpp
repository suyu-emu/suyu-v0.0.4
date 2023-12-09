// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/core.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/service/hid/controllers/applet_resource.h"
#include "core/hle/service/hid/errors.h"

namespace Service::HID {

AppletResource::AppletResource(Core::System& system_) : system{system_} {}

AppletResource::~AppletResource() = default;

Result AppletResource::CreateAppletResource(u64 aruid) {
    const u64 index = GetIndexFromAruid(aruid);

    if (index >= AruidIndexMax) {
        return ResultAruidNotRegistered;
    }

    if (data[index].flag.is_assigned) {
        return ResultAruidAlreadyRegistered;
    }

    // TODO: Here shared memory is created for the process we don't quite emulate this part so
    // obtain this pointer from system
    auto& shared_memory = system.Kernel().GetHidSharedMem();

    data[index].shared_memory_handle = &shared_memory;
    data[index].flag.is_assigned.Assign(true);
    // TODO: InitializeSixAxisControllerConfig(false);
    active_aruid = aruid;
    return ResultSuccess;
}

Result AppletResource::RegisterAppletResourceUserId(u64 aruid, bool enable_input) {
    const u64 index = GetIndexFromAruid(aruid);

    if (index < AruidIndexMax) {
        return ResultAruidAlreadyRegistered;
    }

    std::size_t data_index = AruidIndexMax;
    for (std::size_t i = 0; i < AruidIndexMax; i++) {
        if (!data[i].flag.is_initialized) {
            data_index = i;
            break;
        }
    }

    if (data_index == AruidIndexMax) {
        return ResultAruidNoAvailableEntries;
    }

    AruidData& aruid_data = data[data_index];

    aruid_data.aruid = aruid;
    aruid_data.flag.is_initialized.Assign(true);
    if (enable_input) {
        aruid_data.flag.enable_pad_input.Assign(true);
        aruid_data.flag.enable_six_axis_sensor.Assign(true);
        aruid_data.flag.bit_18.Assign(true);
        aruid_data.flag.enable_touchscreen.Assign(true);
    }

    data_index = AruidIndexMax;
    for (std::size_t i = 0; i < AruidIndexMax; i++) {
        if (registration_list.flag[i] == RegistrationStatus::Initialized) {
            if (registration_list.aruid[i] != aruid) {
                continue;
            }
            data_index = i;
            break;
        }
        if (registration_list.flag[i] == RegistrationStatus::None) {
            data_index = i;
            break;
        }
    }

    if (data_index == AruidIndexMax) {
        return ResultSuccess;
    }

    registration_list.flag[data_index] = RegistrationStatus::Initialized;
    registration_list.aruid[data_index] = aruid;

    return ResultSuccess;
}

void AppletResource::UnregisterAppletResourceUserId(u64 aruid) {
    u64 index = GetIndexFromAruid(aruid);

    if (index < AruidIndexMax) {
        if (data[index].flag.is_assigned) {
            data[index].shared_memory_handle = nullptr;
            data[index].flag.is_assigned.Assign(false);
        }
    }

    index = GetIndexFromAruid(aruid);
    if (index < AruidIndexMax) {
        DestroySevenSixAxisTransferMemory();
        data[index].flag.raw = 0;
        data[index].aruid = 0;

        index = GetIndexFromAruid(aruid);
        if (index < AruidIndexMax) {
            registration_list.flag[index] = RegistrationStatus::PendingDelete;
        }
    }
}

u64 AppletResource::GetActiveAruid() {
    return active_aruid;
}

Result AppletResource::GetSharedMemoryHandle(Kernel::KSharedMemory** out_handle, u64 aruid) {
    u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return ResultAruidNotRegistered;
    }

    *out_handle = data[index].shared_memory_handle;
    return ResultSuccess;
}

u64 AppletResource::GetIndexFromAruid(u64 aruid) {
    for (std::size_t i = 0; i < AruidIndexMax; i++) {
        if (registration_list.flag[i] == RegistrationStatus::Initialized &&
            registration_list.aruid[i] == aruid) {
            return i;
        }
    }
    return AruidIndexMax;
}

Result AppletResource::DestroySevenSixAxisTransferMemory() {
    // TODO
    return ResultSuccess;
}

void AppletResource::EnableInput(u64 aruid, bool is_enabled) {
    const u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return;
    }

    data[index].flag.enable_pad_input.Assign(is_enabled);
    data[index].flag.enable_touchscreen.Assign(is_enabled);
}

void AppletResource::EnableSixAxisSensor(u64 aruid, bool is_enabled) {
    const u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return;
    }

    data[index].flag.enable_six_axis_sensor.Assign(is_enabled);
}

void AppletResource::EnablePadInput(u64 aruid, bool is_enabled) {
    const u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return;
    }

    data[index].flag.enable_pad_input.Assign(is_enabled);
}

void AppletResource::EnableTouchScreen(u64 aruid, bool is_enabled) {
    const u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return;
    }

    data[index].flag.enable_touchscreen.Assign(is_enabled);
}

void AppletResource::SetIsPalmaConnectable(u64 aruid, bool is_connectable) {
    const u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return;
    }

    data[index].flag.is_palma_connectable.Assign(is_connectable);
}

void AppletResource::EnablePalmaBoostMode(u64 aruid, bool is_enabled) {
    const u64 index = GetIndexFromAruid(aruid);
    if (index >= AruidIndexMax) {
        return;
    }

    data[index].flag.enable_palma_boost_mode.Assign(is_enabled);
}

} // namespace Service::HID
