// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/service/hid/hid.h"
#include "core/settings.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

namespace Settings {

Values values = {};

void Apply() {

    GDBStub::SetServerPort(values.gdbstub_port);
    GDBStub::ToggleServer(values.use_gdbstub);

    VideoCore::g_toggle_framelimit_enabled = values.toggle_framelimit;

    auto& system_instance = Core::System::GetInstance();
    if (system_instance.IsPoweredOn()) {
        system_instance.Renderer().UpdateCurrentFramebufferLayout();
    }

    Service::HID::ReloadInputDevices();
}

} // namespace Settings
