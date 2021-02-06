// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/object.h"

namespace Kernel {

Object::Object(KernelCore& kernel_)
    : kernel{kernel_}, object_id{kernel_.CreateNewObjectID()}, name{"[UNKNOWN KERNEL OBJECT]"} {}
Object::Object(KernelCore& kernel_, std::string&& name_)
    : kernel{kernel_}, object_id{kernel_.CreateNewObjectID()}, name{std::move(name_)} {}
Object::~Object() = default;

bool Object::IsWaitable() const {
    switch (GetHandleType()) {
    case HandleType::ReadableEvent:
    case HandleType::Thread:
    case HandleType::Process:
    case HandleType::ServerPort:
    case HandleType::ServerSession:
        return true;

    case HandleType::Unknown:
    case HandleType::Event:
    case HandleType::WritableEvent:
    case HandleType::SharedMemory:
    case HandleType::TransferMemory:
    case HandleType::ResourceLimit:
    case HandleType::ClientPort:
    case HandleType::ClientSession:
    case HandleType::Session:
        return false;
    }

    UNREACHABLE();
    return false;
}

} // namespace Kernel
