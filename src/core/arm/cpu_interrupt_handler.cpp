// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/thread.h"
#include "core/arm/cpu_interrupt_handler.h"

namespace Core {

CPUInterruptHandler::CPUInterruptHandler() : is_interrupted{} {
    interrupt_event = std::make_unique<Common::Event>();
}

CPUInterruptHandler::~CPUInterruptHandler() = default;

void CPUInterruptHandler::SetInterrupt(bool is_interrupted_) {
    if (is_interrupted_) {
        interrupt_event->Set();
    }
    this->is_interrupted = is_interrupted_;
}

void CPUInterruptHandler::AwaitInterrupt() {
    interrupt_event->Wait();
}

} // namespace Core
