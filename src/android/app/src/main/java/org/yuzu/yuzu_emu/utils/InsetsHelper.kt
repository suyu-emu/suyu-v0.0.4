package org.yuzu.yuzu_emu.utils

import android.content.Context

object InsetsHelper {
    const val THREE_BUTTON_NAVIGATION = 0
    const val TWO_BUTTON_NAVIGATION = 1
    const val GESTURE_NAVIGATION = 2

    fun getSystemGestureType(context: Context): Int {
        val resources = context.resources
        val resourceId =
            resources.getIdentifier("config_navBarInteractionMode", "integer", "android")
        return if (resourceId != 0) {
            resources.getInteger(resourceId)
        } else 0
    }
}
