// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

namespace Kernel {

PhysicalCore::PhysicalCore(KernelCore& kernel, std::size_t id, ExclusiveMonitor& exclusive_monitor)
    : core_index{id}, kernel{kernel} {
#ifdef ARCHITECTURE_x86_64
    arm_interface = std::make_unique<ARM_Dynarmic>(system, exclusive_monitor, core_index);
#else
    arm_interface = std::make_unique<ARM_Unicorn>(system);
    LOG_WARNING(Core, "CPU JIT requested, but Dynarmic not available");
#endif

    scheduler = std::make_unique<Kernel::Scheduler>(system, *arm_interface, core_index);
}

} // namespace Kernel
