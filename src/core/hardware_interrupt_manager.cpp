
#include "core/core.h"
#include "core/hardware_interrupt_manager.h"
#include "core/hle/service/nvdrv/interface.h"
#include "core/hle/service/sm/sm.h"

namespace Core::Hardware {

InterruptManager::InterruptManager(Core::System& system_in) : system(system_in) {
    gpu_interrupt_event =
        system.CoreTiming().RegisterEvent("GPUInterrupt", [this](u64 event_index, s64) {
            auto nvdrv = system.ServiceManager().GetService<Service::Nvidia::NVDRV>("nvdrv");
            nvdrv->SignalGPUInterrupt(static_cast<u32>(event_index));
        });
}

void InterruptManager::InterruptGPU(const u32 event_index) {
    system.CoreTiming().ScheduleEvent(10, gpu_interrupt_event, static_cast<u64>(event_index));
}

} // namespace Core::Hardware
