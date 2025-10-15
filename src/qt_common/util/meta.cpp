// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qt_common/util/meta.h"
#include "common/common_types.h"
#include "core/core.h"
#include "core/frontend/applets/cabinet.h"
#include "core/frontend/applets/controller.h"
#include "core/frontend/applets/profile_select.h"
#include "core/frontend/applets/software_keyboard.h"
#include "core/hle/service/am/frontend/applet_web_browser_types.h"

namespace QtCommon::Meta {

void RegisterMetaTypes()
{
    // Register integral and floating point types
    qRegisterMetaType<u8>("u8");
    qRegisterMetaType<u16>("u16");
    qRegisterMetaType<u32>("u32");
    qRegisterMetaType<u64>("u64");
    qRegisterMetaType<u128>("u128");
    qRegisterMetaType<s8>("s8");
    qRegisterMetaType<s16>("s16");
    qRegisterMetaType<s32>("s32");
    qRegisterMetaType<s64>("s64");
    qRegisterMetaType<f32>("f32");
    qRegisterMetaType<f64>("f64");

    // Register string types
    qRegisterMetaType<std::string>("std::string");
    qRegisterMetaType<std::wstring>("std::wstring");
    qRegisterMetaType<std::u8string>("std::u8string");
    qRegisterMetaType<std::u16string>("std::u16string");
    qRegisterMetaType<std::u32string>("std::u32string");
    qRegisterMetaType<std::string_view>("std::string_view");
    qRegisterMetaType<std::wstring_view>("std::wstring_view");
    qRegisterMetaType<std::u8string_view>("std::u8string_view");
    qRegisterMetaType<std::u16string_view>("std::u16string_view");
    qRegisterMetaType<std::u32string_view>("std::u32string_view");

    // Register applet types

    // Cabinet Applet
    qRegisterMetaType<Core::Frontend::CabinetParameters>("Core::Frontend::CabinetParameters");
    qRegisterMetaType<std::shared_ptr<Service::NFC::NfcDevice>>(
        "std::shared_ptr<Service::NFC::NfcDevice>");

    // Controller Applet
    qRegisterMetaType<Core::Frontend::ControllerParameters>("Core::Frontend::ControllerParameters");

    // Profile Select Applet
    qRegisterMetaType<Core::Frontend::ProfileSelectParameters>(
        "Core::Frontend::ProfileSelectParameters");

    // Software Keyboard Applet
    qRegisterMetaType<Core::Frontend::KeyboardInitializeParameters>(
        "Core::Frontend::KeyboardInitializeParameters");
    qRegisterMetaType<Core::Frontend::InlineAppearParameters>(
        "Core::Frontend::InlineAppearParameters");
    qRegisterMetaType<Core::Frontend::InlineTextParameters>("Core::Frontend::InlineTextParameters");
    qRegisterMetaType<Service::AM::Frontend::SwkbdResult>("Service::AM::Frontend::SwkbdResult");
    qRegisterMetaType<Service::AM::Frontend::SwkbdTextCheckResult>(
        "Service::AM::Frontend::SwkbdTextCheckResult");
    qRegisterMetaType<Service::AM::Frontend::SwkbdReplyType>(
        "Service::AM::Frontend::SwkbdReplyType");

    // Web Browser Applet
    qRegisterMetaType<Service::AM::Frontend::WebExitReason>("Service::AM::Frontend::WebExitReason");

    // Register loader types
    qRegisterMetaType<Core::SystemResultStatus>("Core::SystemResultStatus");
}

}
