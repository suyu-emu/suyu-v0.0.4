
#include "core/core.h"
#include "core/hardware_interrupt_manager.h"
#include "core/hle/service/nvdrv/interface.h"
#include "core/hle/service/sm/sm.h"

namespace Core::Hardware {

InterruptManager::InterruptManager(Core::System& system_in) : system(system_in) {
    gpu_interrupt_event =
        system.CoreTiming().RegisterEvent("GPUInterrupt", [this](u64 message, s64) {
            auto nvdrv = system.ServiceManager().GetService<Service::Nvidia::NVDRV>("nvdrv");
            const u32 syncpt = static_cast<u32>(message >> 32);
            const u32 value = static_cast<u32>(message & 0x00000000FFFFFFFFULL);
            nvdrv->SignalGPUInterruptSyncpt(syncpt, value);
        });
}

void InterruptManager::GPUInterruptSyncpt(const u32 syncpoint_id, const u32 value) {
    const u64 msg = (static_cast<u64>(syncpoint_id) << 32ULL) | value;
    system.CoreTiming().ScheduleEvent(10, gpu_interrupt_event, msg);
}

} // namespace Core::Hardware
