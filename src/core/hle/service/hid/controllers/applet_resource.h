// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>

#include "common/bit_field.h"
#include "common/common_types.h"
#include "core/hle/result.h"

namespace Core {
class System;
}

namespace Kernel {
class KSharedMemory;
}

namespace Service::HID {
class AppletResource {
public:
    explicit AppletResource(Core::System& system_);
    ~AppletResource();

    Result CreateAppletResource(u64 aruid);

    Result RegisterAppletResourceUserId(u64 aruid, bool enable_input);
    void UnregisterAppletResourceUserId(u64 aruid);

    void FreeAppletResourceId(u64 aruid);

    u64 GetActiveAruid();
    Result GetSharedMemoryHandle(Kernel::KSharedMemory** out_handle, u64 aruid);

    u64 GetIndexFromAruid(u64 aruid);

    Result DestroySevenSixAxisTransferMemory();

    void EnableInput(u64 aruid, bool is_enabled);
    void EnableSixAxisSensor(u64 aruid, bool is_enabled);
    void EnablePadInput(u64 aruid, bool is_enabled);
    void EnableTouchScreen(u64 aruid, bool is_enabled);
    void SetIsPalmaConnectable(u64 aruid, bool is_connectable);
    void EnablePalmaBoostMode(u64 aruid, bool is_enabled);

    Result RegisterCoreAppletResource();
    Result UnregisterCoreAppletResource();

private:
    static constexpr std::size_t AruidIndexMax = 0x20;

    enum RegistrationStatus : u32 {
        None,
        Initialized,
        PendingDelete,
    };

    struct DataStatusFlag {
        union {
            u32 raw{};

            BitField<0, 1, u32> is_initialized;
            BitField<1, 1, u32> is_assigned;
            BitField<16, 1, u32> enable_pad_input;
            BitField<17, 1, u32> enable_six_axis_sensor;
            BitField<18, 1, u32> bit_18;
            BitField<19, 1, u32> is_palma_connectable;
            BitField<20, 1, u32> enable_palma_boost_mode;
            BitField<21, 1, u32> enable_touchscreen;
        };
    };

    struct AruidRegisterList {
        std::array<RegistrationStatus, AruidIndexMax> flag{};
        std::array<u64, AruidIndexMax> aruid{};
    };
    static_assert(sizeof(AruidRegisterList) == 0x180, "AruidRegisterList is an invalid size");

    struct AruidData {
        DataStatusFlag flag{};
        u64 aruid{};
        Kernel::KSharedMemory* shared_memory_handle{nullptr};
    };

    u64 active_aruid{};
    AruidRegisterList registration_list{};
    std::array<AruidData, AruidIndexMax> data{};
    s32 ref_counter{};

    Core::System& system;
};
} // namespace Service::HID
