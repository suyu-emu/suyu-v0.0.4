// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/frontend/applets/software_keyboard.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/applets/software_keyboard.h"

namespace Service::AM::Applets {

SoftwareKeyboard::SoftwareKeyboard(Core::System& system_,
                                   const Core::Frontend::SoftwareKeyboardApplet& frontend_)
    : Applet{system_.Kernel()}, frontend{frontend_}, system{system_} {}

SoftwareKeyboard::~SoftwareKeyboard() = default;

void SoftwareKeyboard::Initialize() {}

bool SoftwareKeyboard::TransactionComplete() const {}

ResultCode SoftwareKeyboard::GetStatus() const {}

void SoftwareKeyboard::ExecuteInteractive() {}

void SoftwareKeyboard::Execute() {}

} // namespace Service::AM::Applets
