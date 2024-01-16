// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hid_core/hid_result.h"
#include "hid_core/hid_util.h"
#include "hid_core/resources/abstracted_pad/abstract_pad_holder.h"
#include "hid_core/resources/abstracted_pad/abstract_properties_handler.h"
#include "hid_core/resources/abstracted_pad/abstract_vibration_handler.h"
#include "hid_core/resources/applet_resource.h"
#include "hid_core/resources/npad/npad_vibration.h"
#include "hid_core/resources/vibration/gc_vibration_device.h"
#include "hid_core/resources/vibration/n64_vibration_device.h"
#include "hid_core/resources/vibration/vibration_device.h"

namespace Service::HID {

NpadAbstractVibrationHandler::NpadAbstractVibrationHandler() {}

NpadAbstractVibrationHandler::~NpadAbstractVibrationHandler() = default;

void NpadAbstractVibrationHandler::SetAbstractPadHolder(NpadAbstractedPadHolder* holder) {
    abstract_pad_holder = holder;
}

void NpadAbstractVibrationHandler::SetAppletResource(AppletResourceHolder* applet_resource) {
    applet_resource_holder = applet_resource;
}

void NpadAbstractVibrationHandler::SetPropertiesHandler(NpadAbstractPropertiesHandler* handler) {
    properties_handler = handler;
}

void NpadAbstractVibrationHandler::SetN64Vibration(NpadN64VibrationDevice* n64_device) {
    n64_vibration_device = n64_device;
}

void NpadAbstractVibrationHandler::SetVibration(std::span<NpadVibrationDevice*> device) {
    for (std::size_t i = 0; i < device.size() && i < vibration_device.size(); i++) {
        vibration_device[i] = device[i];
    }
}

void NpadAbstractVibrationHandler::SetGcVibration(NpadGcVibrationDevice* gc_device) {
    gc_vibration_device = gc_device;
}

Result NpadAbstractVibrationHandler::IncrementRefCounter() {
    if (ref_counter == std::numeric_limits<s32>::max() - 1) {
        return ResultNpadHandlerOverflow;
    }
    ref_counter++;
    return ResultSuccess;
}

Result NpadAbstractVibrationHandler::DecrementRefCounter() {
    if (ref_counter == 0) {
        return ResultNpadHandlerNotInitialized;
    }
    ref_counter--;
    return ResultSuccess;
}

void NpadAbstractVibrationHandler::UpdateVibrationState() {
    const bool is_handheld_hid_enabled =
        applet_resource_holder->handheld_config->is_handheld_hid_enabled;
    const bool is_force_handheld_style_vibration =
        applet_resource_holder->handheld_config->is_force_handheld_style_vibration;

    if (!is_handheld_hid_enabled && is_force_handheld_style_vibration) {
        // TODO
    }
}
} // namespace Service::HID
