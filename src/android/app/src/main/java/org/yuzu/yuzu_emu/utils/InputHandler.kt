// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import org.yuzu.yuzu_emu.features.input.NativeInput
import org.yuzu.yuzu_emu.features.input.YuzuInputOverlayDevice
import org.yuzu.yuzu_emu.features.input.YuzuPhysicalDevice

object InputHandler {
    var androidControllers = mapOf<Int, YuzuPhysicalDevice>()
    var registeredControllers = mutableListOf<ParamPackage>()

    private val controllerButtons = intArrayOf(
        KeyEvent.KEYCODE_BUTTON_A,
        KeyEvent.KEYCODE_BUTTON_B,
        KeyEvent.KEYCODE_BUTTON_X,
        KeyEvent.KEYCODE_BUTTON_Y,
        KeyEvent.KEYCODE_BUTTON_L1,
        KeyEvent.KEYCODE_BUTTON_R1,
        KeyEvent.KEYCODE_BUTTON_L2,
        KeyEvent.KEYCODE_BUTTON_R2,
        KeyEvent.KEYCODE_BUTTON_THUMBL,
        KeyEvent.KEYCODE_BUTTON_THUMBR,
        KeyEvent.KEYCODE_BUTTON_START,
        KeyEvent.KEYCODE_BUTTON_SELECT,
        KeyEvent.KEYCODE_DPAD_UP,
        KeyEvent.KEYCODE_DPAD_DOWN,
        KeyEvent.KEYCODE_DPAD_LEFT,
        KeyEvent.KEYCODE_DPAD_RIGHT
    )

    private val controllerAxes = intArrayOf(
        MotionEvent.AXIS_X,
        MotionEvent.AXIS_Y,
        MotionEvent.AXIS_Z,
        MotionEvent.AXIS_RX,
        MotionEvent.AXIS_RY,
        MotionEvent.AXIS_RZ,
        MotionEvent.AXIS_HAT_X,
        MotionEvent.AXIS_HAT_Y,
        MotionEvent.AXIS_LTRIGGER,
        MotionEvent.AXIS_RTRIGGER
    )

    fun isPhysicalGameController(device: InputDevice?): Boolean {
        device ?: return false

        if (device.isVirtual) {
            return false
        }

        val sources = device.sources
        val hasControllerSource =
            sources and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
                sources and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK
        if (!hasControllerSource) {
            return false
        }

        val hasControllerButtons = device.hasKeys(*controllerButtons).any { it }
        val hasControllerAxes = device.motionRanges.any { range ->
            controllerAxes.contains(range.axis)
        }
        return hasControllerButtons || hasControllerAxes
    }

    fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val action = when (event.action) {
            KeyEvent.ACTION_DOWN -> NativeInput.ButtonState.PRESSED
            KeyEvent.ACTION_UP -> NativeInput.ButtonState.RELEASED
            else -> return false
        }

        var controllerData = androidControllers[event.device.controllerNumber]
        if (controllerData == null) {
            updateControllerData()
            controllerData = androidControllers[event.device.controllerNumber] ?: return false
        }

        NativeInput.onGamePadButtonEvent(
            controllerData.getGUID(),
            controllerData.getPort(),
            event.keyCode,
            action
        )
        return true
    }

    fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        val controllerData =
            androidControllers[event.device.controllerNumber] ?: return false
        event.device.motionRanges.forEach {
            NativeInput.onGamePadAxisEvent(
                controllerData.getGUID(),
                controllerData.getPort(),
                it.axis,
                event.getAxisValue(it.axis)
            )
        }
        return true
    }

    fun getDevices(): Map<Int, YuzuPhysicalDevice> {
        val gameControllerDeviceIds = mutableMapOf<Int, YuzuPhysicalDevice>()
        val deviceIds = InputDevice.getDeviceIds()
        var port = 0
        val inputSettings = NativeConfig.getInputSettings(true)
        deviceIds.forEach { deviceId ->
            InputDevice.getDevice(deviceId)?.apply {
                if (isPhysicalGameController(this)) {
                    if (!gameControllerDeviceIds.contains(controllerNumber)) {
                        gameControllerDeviceIds[controllerNumber] = YuzuPhysicalDevice(
                            this,
                            port,
                            inputSettings[port].useSystemVibrator
                        )
                    }
                    port++
                }
            }
        }
        return gameControllerDeviceIds
    }

    fun updateControllerData() {
        androidControllers = getDevices()
        androidControllers.forEach {
            NativeInput.registerController(it.value)
        }

        // Register the input overlay on a dedicated port for all player 1 vibrations
        NativeInput.registerController(YuzuInputOverlayDevice(androidControllers.isEmpty(), 100))
        registeredControllers.clear()
        NativeInput.getInputDevices().forEach {
            registeredControllers.add(ParamPackage(it))
        }
        registeredControllers.sortBy { it.get("port", 0) }
    }

    fun InputDevice.getGUID(): String = String.format("%016x%016x", productId, vendorId)
}
