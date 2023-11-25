// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import kotlin.math.sqrt
import org.yuzu.yuzu_emu.NativeLibrary

object InputHandler {
    private var controllerIds = getGameControllerIds()

    fun initialize() {
        // Connect first controller
        NativeLibrary.onGamePadConnectEvent(getPlayerNumber(NativeLibrary.Player1Device))
    }

    fun updateControllerIds() {
        controllerIds = getGameControllerIds()
    }

    fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val button: Int = when (event.device.vendorId) {
            0x045E -> getInputXboxButtonKey(event.keyCode)
            0x054C -> getInputDS5ButtonKey(event.keyCode)
            0x057E -> getInputJoyconButtonKey(event.keyCode)
            0x1532 -> getInputRazerButtonKey(event.keyCode)
            0x3537 -> getInputRedmagicButtonKey(event.keyCode)
            else -> getInputGenericButtonKey(event.keyCode)
        }

        val action = when (event.action) {
            KeyEvent.ACTION_DOWN -> NativeLibrary.ButtonState.PRESSED
            KeyEvent.ACTION_UP -> NativeLibrary.ButtonState.RELEASED
            else -> return false
        }

        // Ignore invalid buttons
        if (button < 0) {
            return false
        }

        return NativeLibrary.onGamePadButtonEvent(
            getPlayerNumber(event.device.controllerNumber, event.deviceId),
            button,
            action
        )
    }

    fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        val device = event.device
        // Check every axis input available on the controller
        for (range in device.motionRanges) {
            val axis = range.axis
            when (device.vendorId) {
                0x045E -> setGenericAxisInput(event, axis)
                0x054C -> setGenericAxisInput(event, axis)
                0x057E -> setJoyconAxisInput(event, axis)
                0x1532 -> setRazerAxisInput(event, axis)
                else -> setGenericAxisInput(event, axis)
            }
        }

        return true
    }

    private fun getPlayerNumber(index: Int, deviceId: Int = -1): Int {
        var deviceIndex = index
        if (deviceId != -1) {
            deviceIndex = controllerIds[deviceId] ?: 0
        }

        // TODO: Joycons are handled as different controllers. Find a way to merge them.
        return when (deviceIndex) {
            2 -> NativeLibrary.Player2Device
            3 -> NativeLibrary.Player3Device
            4 -> NativeLibrary.Player4Device
            5 -> NativeLibrary.Player5Device
            6 -> NativeLibrary.Player6Device
            7 -> NativeLibrary.Player7Device
            8 -> NativeLibrary.Player8Device
            else -> if (NativeLibrary.isHandheldOnly()) {
                NativeLibrary.ConsoleDevice
            } else {
                NativeLibrary.Player1Device
            }
        }
    }

    private fun setStickState(playerNumber: Int, index: Int, xAxis: Float, yAxis: Float) {
        // Calculate vector size
        val r2 = xAxis * xAxis + yAxis * yAxis
        var r = sqrt(r2.toDouble()).toFloat()

        // Adjust range of joystick
        val deadzone = 0.15f
        var x = xAxis
        var y = yAxis

        if (r > deadzone) {
            val deadzoneFactor = 1.0f / r * (r - deadzone) / (1.0f - deadzone)
            x *= deadzoneFactor
            y *= deadzoneFactor
            r *= deadzoneFactor
        } else {
            x = 0.0f
            y = 0.0f
        }

        // Normalize joystick
        if (r > 1.0f) {
            x /= r
            y /= r
        }

        NativeLibrary.onGamePadJoystickEvent(
            playerNumber,
            index,
            x,
            -y
        )
    }

    private fun getAxisToButton(axis: Float): Int {
        return if (axis > 0.5f) {
            NativeLibrary.ButtonState.PRESSED
        } else {
            NativeLibrary.ButtonState.RELEASED
        }
    }

    private fun setAxisDpadState(playerNumber: Int, xAxis: Float, yAxis: Float) {
        NativeLibrary.onGamePadButtonEvent(
            playerNumber,
            NativeLibrary.ButtonType.DPAD_UP,
            getAxisToButton(-yAxis)
        )
        NativeLibrary.onGamePadButtonEvent(
            playerNumber,
            NativeLibrary.ButtonType.DPAD_DOWN,
            getAxisToButton(yAxis)
        )
        NativeLibrary.onGamePadButtonEvent(
            playerNumber,
            NativeLibrary.ButtonType.DPAD_LEFT,
            getAxisToButton(-xAxis)
        )
        NativeLibrary.onGamePadButtonEvent(
            playerNumber,
            NativeLibrary.ButtonType.DPAD_RIGHT,
            getAxisToButton(xAxis)
        )
    }

    private fun getInputDS5ButtonKey(key: Int): Int {
        // The missing ds5 buttons are axis
        return when (key) {
            KeyEvent.KEYCODE_BUTTON_A -> NativeLibrary.ButtonType.BUTTON_B
            KeyEvent.KEYCODE_BUTTON_B -> NativeLibrary.ButtonType.BUTTON_A
            KeyEvent.KEYCODE_BUTTON_X -> NativeLibrary.ButtonType.BUTTON_Y
            KeyEvent.KEYCODE_BUTTON_Y -> NativeLibrary.ButtonType.BUTTON_X
            KeyEvent.KEYCODE_BUTTON_L1 -> NativeLibrary.ButtonType.TRIGGER_L
            KeyEvent.KEYCODE_BUTTON_R1 -> NativeLibrary.ButtonType.TRIGGER_R
            KeyEvent.KEYCODE_BUTTON_THUMBL -> NativeLibrary.ButtonType.STICK_L
            KeyEvent.KEYCODE_BUTTON_THUMBR -> NativeLibrary.ButtonType.STICK_R
            KeyEvent.KEYCODE_BUTTON_START -> NativeLibrary.ButtonType.BUTTON_PLUS
            KeyEvent.KEYCODE_BUTTON_SELECT -> NativeLibrary.ButtonType.BUTTON_MINUS
            else -> -1
        }
    }

    private fun getInputJoyconButtonKey(key: Int): Int {
        // Joycon support is half dead. A lot of buttons can't be mapped
        return when (key) {
            KeyEvent.KEYCODE_BUTTON_A -> NativeLibrary.ButtonType.BUTTON_B
            KeyEvent.KEYCODE_BUTTON_B -> NativeLibrary.ButtonType.BUTTON_A
            KeyEvent.KEYCODE_BUTTON_X -> NativeLibrary.ButtonType.BUTTON_X
            KeyEvent.KEYCODE_BUTTON_Y -> NativeLibrary.ButtonType.BUTTON_Y
            KeyEvent.KEYCODE_DPAD_UP -> NativeLibrary.ButtonType.DPAD_UP
            KeyEvent.KEYCODE_DPAD_DOWN -> NativeLibrary.ButtonType.DPAD_DOWN
            KeyEvent.KEYCODE_DPAD_LEFT -> NativeLibrary.ButtonType.DPAD_LEFT
            KeyEvent.KEYCODE_DPAD_RIGHT -> NativeLibrary.ButtonType.DPAD_RIGHT
            KeyEvent.KEYCODE_BUTTON_L1 -> NativeLibrary.ButtonType.TRIGGER_L
            KeyEvent.KEYCODE_BUTTON_R1 -> NativeLibrary.ButtonType.TRIGGER_R
            KeyEvent.KEYCODE_BUTTON_L2 -> NativeLibrary.ButtonType.TRIGGER_ZL
            KeyEvent.KEYCODE_BUTTON_R2 -> NativeLibrary.ButtonType.TRIGGER_ZR
            KeyEvent.KEYCODE_BUTTON_THUMBL -> NativeLibrary.ButtonType.STICK_L
            KeyEvent.KEYCODE_BUTTON_THUMBR -> NativeLibrary.ButtonType.STICK_R
            KeyEvent.KEYCODE_BUTTON_START -> NativeLibrary.ButtonType.BUTTON_PLUS
            KeyEvent.KEYCODE_BUTTON_SELECT -> NativeLibrary.ButtonType.BUTTON_MINUS
            else -> -1
        }
    }

    private fun getInputXboxButtonKey(key: Int): Int {
        // The missing xbox buttons are axis
        return when (key) {
            KeyEvent.KEYCODE_BUTTON_A -> NativeLibrary.ButtonType.BUTTON_A
            KeyEvent.KEYCODE_BUTTON_B -> NativeLibrary.ButtonType.BUTTON_B
            KeyEvent.KEYCODE_BUTTON_X -> NativeLibrary.ButtonType.BUTTON_X
            KeyEvent.KEYCODE_BUTTON_Y -> NativeLibrary.ButtonType.BUTTON_Y
            KeyEvent.KEYCODE_BUTTON_L1 -> NativeLibrary.ButtonType.TRIGGER_L
            KeyEvent.KEYCODE_BUTTON_R1 -> NativeLibrary.ButtonType.TRIGGER_R
            KeyEvent.KEYCODE_BUTTON_THUMBL -> NativeLibrary.ButtonType.STICK_L
            KeyEvent.KEYCODE_BUTTON_THUMBR -> NativeLibrary.ButtonType.STICK_R
            KeyEvent.KEYCODE_BUTTON_START -> NativeLibrary.ButtonType.BUTTON_PLUS
            KeyEvent.KEYCODE_BUTTON_SELECT -> NativeLibrary.ButtonType.BUTTON_MINUS
            else -> -1
        }
    }

    private fun getInputRazerButtonKey(key: Int): Int {
        // The missing xbox buttons are axis
        return when (key) {
            KeyEvent.KEYCODE_BUTTON_A -> NativeLibrary.ButtonType.BUTTON_B
            KeyEvent.KEYCODE_BUTTON_B -> NativeLibrary.ButtonType.BUTTON_A
            KeyEvent.KEYCODE_BUTTON_X -> NativeLibrary.ButtonType.BUTTON_Y
            KeyEvent.KEYCODE_BUTTON_Y -> NativeLibrary.ButtonType.BUTTON_X
            KeyEvent.KEYCODE_BUTTON_L1 -> NativeLibrary.ButtonType.TRIGGER_L
            KeyEvent.KEYCODE_BUTTON_R1 -> NativeLibrary.ButtonType.TRIGGER_R
            KeyEvent.KEYCODE_BUTTON_THUMBL -> NativeLibrary.ButtonType.STICK_L
            KeyEvent.KEYCODE_BUTTON_THUMBR -> NativeLibrary.ButtonType.STICK_R
            KeyEvent.KEYCODE_BUTTON_START -> NativeLibrary.ButtonType.BUTTON_PLUS
            KeyEvent.KEYCODE_BUTTON_SELECT -> NativeLibrary.ButtonType.BUTTON_MINUS
            else -> -1
        }
    }

    private fun getInputRedmagicButtonKey(key: Int): Int {
        return when (key) {
            KeyEvent.KEYCODE_BUTTON_A -> NativeLibrary.ButtonType.BUTTON_B
            KeyEvent.KEYCODE_BUTTON_B -> NativeLibrary.ButtonType.BUTTON_A
            KeyEvent.KEYCODE_BUTTON_X -> NativeLibrary.ButtonType.BUTTON_Y
            KeyEvent.KEYCODE_BUTTON_Y -> NativeLibrary.ButtonType.BUTTON_X
            KeyEvent.KEYCODE_BUTTON_L1 -> NativeLibrary.ButtonType.TRIGGER_L
            KeyEvent.KEYCODE_BUTTON_R1 -> NativeLibrary.ButtonType.TRIGGER_R
            KeyEvent.KEYCODE_BUTTON_L2 -> NativeLibrary.ButtonType.TRIGGER_ZL
            KeyEvent.KEYCODE_BUTTON_R2 -> NativeLibrary.ButtonType.TRIGGER_ZR
            KeyEvent.KEYCODE_BUTTON_THUMBL -> NativeLibrary.ButtonType.STICK_L
            KeyEvent.KEYCODE_BUTTON_THUMBR -> NativeLibrary.ButtonType.STICK_R
            KeyEvent.KEYCODE_BUTTON_START -> NativeLibrary.ButtonType.BUTTON_PLUS
            KeyEvent.KEYCODE_BUTTON_SELECT -> NativeLibrary.ButtonType.BUTTON_MINUS
            else -> -1
        }
    }

    private fun getInputGenericButtonKey(key: Int): Int {
        return when (key) {
            KeyEvent.KEYCODE_BUTTON_A -> NativeLibrary.ButtonType.BUTTON_A
            KeyEvent.KEYCODE_BUTTON_B -> NativeLibrary.ButtonType.BUTTON_B
            KeyEvent.KEYCODE_BUTTON_X -> NativeLibrary.ButtonType.BUTTON_X
            KeyEvent.KEYCODE_BUTTON_Y -> NativeLibrary.ButtonType.BUTTON_Y
            KeyEvent.KEYCODE_DPAD_UP -> NativeLibrary.ButtonType.DPAD_UP
            KeyEvent.KEYCODE_DPAD_DOWN -> NativeLibrary.ButtonType.DPAD_DOWN
            KeyEvent.KEYCODE_DPAD_LEFT -> NativeLibrary.ButtonType.DPAD_LEFT
            KeyEvent.KEYCODE_DPAD_RIGHT -> NativeLibrary.ButtonType.DPAD_RIGHT
            KeyEvent.KEYCODE_BUTTON_L1 -> NativeLibrary.ButtonType.TRIGGER_L
            KeyEvent.KEYCODE_BUTTON_R1 -> NativeLibrary.ButtonType.TRIGGER_R
            KeyEvent.KEYCODE_BUTTON_L2 -> NativeLibrary.ButtonType.TRIGGER_ZL
            KeyEvent.KEYCODE_BUTTON_R2 -> NativeLibrary.ButtonType.TRIGGER_ZR
            KeyEvent.KEYCODE_BUTTON_THUMBL -> NativeLibrary.ButtonType.STICK_L
            KeyEvent.KEYCODE_BUTTON_THUMBR -> NativeLibrary.ButtonType.STICK_R
            KeyEvent.KEYCODE_BUTTON_START -> NativeLibrary.ButtonType.BUTTON_PLUS
            KeyEvent.KEYCODE_BUTTON_SELECT -> NativeLibrary.ButtonType.BUTTON_MINUS
            else -> -1
        }
    }

    private fun setGenericAxisInput(event: MotionEvent, axis: Int) {
        val playerNumber = getPlayerNumber(event.device.controllerNumber, event.deviceId)

        when (axis) {
            MotionEvent.AXIS_X, MotionEvent.AXIS_Y ->
                setStickState(
                    playerNumber,
                    NativeLibrary.StickType.STICK_L,
                    event.getAxisValue(MotionEvent.AXIS_X),
                    event.getAxisValue(MotionEvent.AXIS_Y)
                )
            MotionEvent.AXIS_RX, MotionEvent.AXIS_RY ->
                setStickState(
                    playerNumber,
                    NativeLibrary.StickType.STICK_R,
                    event.getAxisValue(MotionEvent.AXIS_RX),
                    event.getAxisValue(MotionEvent.AXIS_RY)
                )
            MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ ->
                setStickState(
                    playerNumber,
                    NativeLibrary.StickType.STICK_R,
                    event.getAxisValue(MotionEvent.AXIS_Z),
                    event.getAxisValue(MotionEvent.AXIS_RZ)
                )
            MotionEvent.AXIS_LTRIGGER ->
                NativeLibrary.onGamePadButtonEvent(
                    playerNumber,
                    NativeLibrary.ButtonType.TRIGGER_ZL,
                    getAxisToButton(event.getAxisValue(MotionEvent.AXIS_LTRIGGER))
                )
            MotionEvent.AXIS_BRAKE ->
                NativeLibrary.onGamePadButtonEvent(
                    playerNumber,
                    NativeLibrary.ButtonType.TRIGGER_ZL,
                    getAxisToButton(event.getAxisValue(MotionEvent.AXIS_BRAKE))
                )
            MotionEvent.AXIS_RTRIGGER ->
                NativeLibrary.onGamePadButtonEvent(
                    playerNumber,
                    NativeLibrary.ButtonType.TRIGGER_ZR,
                    getAxisToButton(event.getAxisValue(MotionEvent.AXIS_RTRIGGER))
                )
            MotionEvent.AXIS_GAS ->
                NativeLibrary.onGamePadButtonEvent(
                    playerNumber,
                    NativeLibrary.ButtonType.TRIGGER_ZR,
                    getAxisToButton(event.getAxisValue(MotionEvent.AXIS_GAS))
                )
            MotionEvent.AXIS_HAT_X, MotionEvent.AXIS_HAT_Y ->
                setAxisDpadState(
                    playerNumber,
                    event.getAxisValue(MotionEvent.AXIS_HAT_X),
                    event.getAxisValue(MotionEvent.AXIS_HAT_Y)
                )
        }
    }

    private fun setJoyconAxisInput(event: MotionEvent, axis: Int) {
        // Joycon support is half dead. Right joystick doesn't work
        val playerNumber = getPlayerNumber(event.device.controllerNumber, event.deviceId)

        when (axis) {
            MotionEvent.AXIS_X, MotionEvent.AXIS_Y ->
                setStickState(
                    playerNumber,
                    NativeLibrary.StickType.STICK_L,
                    event.getAxisValue(MotionEvent.AXIS_X),
                    event.getAxisValue(MotionEvent.AXIS_Y)
                )
            MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ ->
                setStickState(
                    playerNumber,
                    NativeLibrary.StickType.STICK_R,
                    event.getAxisValue(MotionEvent.AXIS_Z),
                    event.getAxisValue(MotionEvent.AXIS_RZ)
                )
            MotionEvent.AXIS_RX, MotionEvent.AXIS_RY ->
                setStickState(
                    playerNumber,
                    NativeLibrary.StickType.STICK_R,
                    event.getAxisValue(MotionEvent.AXIS_RX),
                    event.getAxisValue(MotionEvent.AXIS_RY)
                )
        }
    }

    private fun setRazerAxisInput(event: MotionEvent, axis: Int) {
        val playerNumber = getPlayerNumber(event.device.controllerNumber, event.deviceId)

        when (axis) {
            MotionEvent.AXIS_X, MotionEvent.AXIS_Y ->
                setStickState(
                    playerNumber,
                    NativeLibrary.StickType.STICK_L,
                    event.getAxisValue(MotionEvent.AXIS_X),
                    event.getAxisValue(MotionEvent.AXIS_Y)
                )
            MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ ->
                setStickState(
                    playerNumber,
                    NativeLibrary.StickType.STICK_R,
                    event.getAxisValue(MotionEvent.AXIS_Z),
                    event.getAxisValue(MotionEvent.AXIS_RZ)
                )
            MotionEvent.AXIS_BRAKE ->
                NativeLibrary.onGamePadButtonEvent(
                    playerNumber,
                    NativeLibrary.ButtonType.TRIGGER_ZL,
                    getAxisToButton(event.getAxisValue(MotionEvent.AXIS_BRAKE))
                )
            MotionEvent.AXIS_GAS ->
                NativeLibrary.onGamePadButtonEvent(
                    playerNumber,
                    NativeLibrary.ButtonType.TRIGGER_ZR,
                    getAxisToButton(event.getAxisValue(MotionEvent.AXIS_GAS))
                )
            MotionEvent.AXIS_HAT_X, MotionEvent.AXIS_HAT_Y ->
                setAxisDpadState(
                    playerNumber,
                    event.getAxisValue(MotionEvent.AXIS_HAT_X),
                    event.getAxisValue(MotionEvent.AXIS_HAT_Y)
                )
        }
    }

    fun getGameControllerIds(): Map<Int, Int> {
        val gameControllerDeviceIds = mutableMapOf<Int, Int>()
        val deviceIds = InputDevice.getDeviceIds()
        var controllerSlot = 1
        deviceIds.forEach { deviceId ->
            InputDevice.getDevice(deviceId)?.apply {
                // Don't over-assign controllers
                if (controllerSlot >= 8) {
                    return gameControllerDeviceIds
                }

                // Verify that the device has gamepad buttons, control sticks, or both.
                if (sources and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
                    sources and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK
                ) {
                    // This device is a game controller. Store its device ID.
                    if (deviceId and id and vendorId and productId != 0) {
                        // Additionally filter out devices that have no ID
                        gameControllerDeviceIds
                            .takeIf { !it.contains(deviceId) }
                            ?.put(deviceId, controllerSlot)
                        controllerSlot++
                    }
                }
            }
        }
        return gameControllerDeviceIds
    }
}
