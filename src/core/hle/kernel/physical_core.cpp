// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/arm/arm_interface.h"
#ifdef ARCHITECTURE_x86_64
#include "core/arm/dynarmic/arm_dynarmic_32.h"
#include "core/arm/dynarmic/arm_dynarmic_64.h"
#endif
#include "core/arm/exclusive_monitor.h"
#include "core/arm/unicorn/arm_unicorn.h"
#include "core/core.h"
#include "core/hle/kernel/physical_core.h"
#include "core/hle/kernel/scheduler.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

PhysicalCore::PhysicalCore(Core::System& system, std::size_t id,
                           Core::ExclusiveMonitor& exclusive_monitor)
    : core_index{id} {
#ifdef ARCHITECTURE_x86_64
    arm_interface_32 =
        std::make_unique<Core::ARM_Dynarmic_32>(system, exclusive_monitor, core_index);
    arm_interface_64 =
        std::make_unique<Core::ARM_Dynarmic_64>(system, exclusive_monitor, core_index);

#else
    using Core::ARM_Unicorn;
    arm_interface_32 = std::make_unique<ARM_Unicorn>(system, ARM_Unicorn::Arch::AArch32);
    arm_interface_64 = std::make_unique<ARM_Unicorn>(system, ARM_Unicorn::Arch::AArch64);
    LOG_WARNING(Core, "CPU JIT requested, but Dynarmic not available");
#endif

    scheduler = std::make_unique<Kernel::Scheduler>(system, core_index);
}

PhysicalCore::~PhysicalCore() = default;

void PhysicalCore::Run() {
    arm_interface->Run();
    arm_interface->ClearExclusiveState();
}

void PhysicalCore::Step() {
    arm_interface->Step();
}

void PhysicalCore::Stop() {
    arm_interface->PrepareReschedule();
}

void PhysicalCore::Shutdown() {
    scheduler->Shutdown();
}

void PhysicalCore::SetIs64Bit(bool is_64_bit) {
    if (is_64_bit) {
        arm_interface = arm_interface_64.get();
    } else {
        arm_interface = arm_interface_32.get();
    }
}

} // namespace Kernel
