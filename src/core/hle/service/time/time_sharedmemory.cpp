// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/hle/service/time/time_sharedmemory.h"

namespace Service::Time {

SharedMemory::SharedMemory(Core::System& system) : system(system) {
    shared_memory_holder = Kernel::SharedMemory::Create(
        system.Kernel(), nullptr, SHARED_MEMORY_SIZE, Kernel::MemoryPermission::ReadWrite,
        Kernel::MemoryPermission::Read, 0, Kernel::MemoryRegion::BASE, "Time:SharedMemory");
    shared_memory_format = reinterpret_cast<Format*>(shared_memory_holder->GetPointer());
    shared_memory_format->format_version =
        14; // Seems static from 1.0.0 -> 8.1.0. Specific games seem to check this value and crash
            // if it's set to anything else
}

SharedMemory::~SharedMemory() = default;

Kernel::SharedPtr<Kernel::SharedMemory> SharedMemory::GetSharedMemoryHolder() const {
    return shared_memory_holder;
}

void SharedMemory::SetStandardSteadyClockTimepoint(const SteadyClockTimePoint& timepoint) {
    shared_memory_format->standard_steady_clock_timepoint.StoreData(timepoint);
}

void SharedMemory::SetStandardLocalSystemClockContext(const SystemClockContext& context) {
    shared_memory_format->standard_local_system_clock_context.StoreData(context);
}

void SharedMemory::SetStandardNetworkSystemClockContext(const SystemClockContext& context) {
    shared_memory_format->standard_network_system_clock_context.StoreData(context);
}

void SharedMemory::SetStandardUserSystemClockAutomaticCorrectionEnabled(const bool enabled) {
    shared_memory_format->standard_user_system_clock_automatic_correction.StoreData(enabled ? 1
                                                                                            : 0);
}

SteadyClockTimePoint SharedMemory::GetStandardSteadyClockTimepoint() const {
    return shared_memory_format->standard_steady_clock_timepoint.ReadData();
}

SystemClockContext SharedMemory::GetStandardLocalSystemClockContext() const {
    return shared_memory_format->standard_local_system_clock_context.ReadData();
}

SystemClockContext SharedMemory::GetStandardNetworkSystemClockContext() const {
    return shared_memory_format->standard_network_system_clock_context.ReadData();
}

bool SharedMemory::GetStandardUserSystemClockAutomaticCorrectionEnabled() const {
    return shared_memory_format->standard_user_system_clock_automatic_correction.ReadData() > 0;
}

} // namespace Service::Time
