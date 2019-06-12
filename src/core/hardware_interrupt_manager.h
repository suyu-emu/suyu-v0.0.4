#pragma once

#include "common/common_types.h"
#include "core/core_timing.h"

namespace Core {
class System;
}

namespace Core::Hardware {

class InterruptManager {
public:
    InterruptManager(Core::System& system);
    ~InterruptManager() = default;

    void GPUInterruptSyncpt(const u32 syncpoint_id, const u32 value);

private:
    Core::System& system;
    Core::Timing::EventType* gpu_interrupt_event{};
};

} // namespace Core::Hardware
