// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

enum class FloatSetting(
    override val key: String,
    override val section: String,
    override val defaultValue: Float
) : AbstractFloatSetting {
    // No float settings currently exist
    EMPTY_SETTING("", "", 0f);

    override var float: Float = defaultValue

    override val valueAsString: String
        get() = float.toString()

    override val isRuntimeEditable: Boolean
        get() {
            for (setting in NOT_RUNTIME_EDITABLE) {
                if (setting == this) {
                    return false
                }
            }
            return true
        }

    companion object {
        private val NOT_RUNTIME_EDITABLE = emptyList<FloatSetting>()

        fun from(key: String): FloatSetting? = FloatSetting.values().firstOrNull { it.key == key }

        fun clear() = FloatSetting.values().forEach { it.float = it.defaultValue }
    }
}
