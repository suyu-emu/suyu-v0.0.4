// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.app.Activity
import android.app.Application
import android.os.Bundle
import android.view.InputDevice
import android.view.KeyEvent
import android.view.View
import android.view.Window
import androidx.activity.ComponentActivity
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import java.util.concurrent.atomic.AtomicBoolean

object ControllerNavigationGlobalHook {
    private val installed = AtomicBoolean(false)

    fun install(application: Application) {
        if (!installed.compareAndSet(false, true)) {
            return
        }
        application.registerActivityLifecycleCallbacks(HookInstaller)
    }

    private object HookInstaller : Application.ActivityLifecycleCallbacks {
        override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle?) {
            installHookIfNeeded(activity)
        }

        override fun onActivityResumed(activity: Activity) {
            installHookIfNeeded(activity)
        }

        override fun onActivityStarted(activity: Activity) = Unit
        override fun onActivityPaused(activity: Activity) = Unit
        override fun onActivityStopped(activity: Activity) = Unit
        override fun onActivitySaveInstanceState(activity: Activity, outState: Bundle) = Unit
        override fun onActivityDestroyed(activity: Activity) = Unit
    }

    private fun installHookIfNeeded(activity: Activity) {
        val window = activity.window ?: return
        val currentCallback = window.callback ?: return
        if (currentCallback is ControllerNavigationWindowCallback) {
            return
        }
        window.callback = ControllerNavigationWindowCallback(activity, currentCallback)
    }

    private class ControllerNavigationWindowCallback(
        private val activity: Activity,
        private val delegate: Window.Callback
    ) : Window.Callback by delegate {
        private val componentActivity = activity as? ComponentActivity

        override fun dispatchKeyEvent(event: KeyEvent): Boolean {
            if (activity.isFinishing || activity.isDestroyed) {
                return delegate.dispatchKeyEvent(event)
            }

            if (!BooleanSetting.INVERT_CONFIRM_BACK_CONTROLLER_BUTTONS.getBoolean()) {
                return delegate.dispatchKeyEvent(event)
            }

            if (!isControllerInput(event) || componentActivity == null) {
                return delegate.dispatchKeyEvent(event)
            }
            if (shouldBypassInGameplay()) {
                return delegate.dispatchKeyEvent(event)
            }

            if (isConfirmAction(event.keyCode)) {
                return when (event.action) {
                    KeyEvent.ACTION_DOWN -> {
                        if (event.repeatCount == 0) {
                            componentActivity.onBackPressedDispatcher.onBackPressed()
                        }
                        true
                    }
                    KeyEvent.ACTION_UP -> true
                    else -> false
                }
            }

            if (isBackAction(event.keyCode)) {
                val remappedEvent = KeyEvent(
                    event.downTime,
                    event.eventTime,
                    event.action,
                    KeyEvent.KEYCODE_DPAD_CENTER,
                    event.repeatCount,
                    event.metaState,
                    event.deviceId,
                    event.scanCode,
                    event.flags,
                    event.source
                )
                return delegate.dispatchKeyEvent(remappedEvent)
            }

            return delegate.dispatchKeyEvent(event)
        }

        private fun shouldBypassInGameplay(): Boolean {
            if (activity.javaClass.name != "org.yuzu.yuzu_emu.activities.EmulationActivity") {
                return false
            }
            if (!NativeLibrary.isRunning()) {
                return false
            }

            val surface = activity.findViewById<View?>(R.id.surface_emulation) ?: return false
            if (!surface.isShown || surface.visibility != View.VISIBLE) {
                return false
            }

            val focused = activity.currentFocus
            if (focused != null) {
                val inputOverlay = activity.findViewById<View?>(R.id.surface_input_overlay)
                if (!isDescendantOf(focused, inputOverlay)) {
                    return false
                }
            }

            return true
        }

        private fun isDescendantOf(view: View, parent: View?): Boolean {
            if (parent == null) {
                return false
            }

            var current: View? = view
            while (current != null) {
                if (current === parent) {
                    return true
                }
                current = current.parent as? View
            }

            return false
        }

        private fun isControllerInput(event: KeyEvent): Boolean {
            val source = event.source
            val deviceSources = event.device?.sources ?: InputDevice.getDevice(event.deviceId)?.sources ?: 0
            return hasControllerSource(source) || hasControllerSource(deviceSources)
        }

        private fun isConfirmAction(keyCode: Int): Boolean {
            return keyCode == KeyEvent.KEYCODE_BUTTON_A
        }

        private fun isBackAction(keyCode: Int): Boolean {
            return keyCode == KeyEvent.KEYCODE_BUTTON_B
        }

        private fun hasControllerSource(source: Int): Boolean {
            return source and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
                source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK ||
                source and InputDevice.SOURCE_DPAD == InputDevice.SOURCE_DPAD
        }
    }
}
