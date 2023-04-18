// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

enum class StringSetting(
    override val key: String,
    override val section: String,
    defaultValue: String
) : AbstractStringSetting {
    // No string settings currently exist
    EMPTY_SETTING("", "", "");

    override var string: String = defaultValue

    override val valueAsString: String
        get() = string

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
        private val NOT_RUNTIME_EDITABLE = emptyList<StringSetting>()

        fun from(key: String): StringSetting? = StringSetting.values().firstOrNull { it.key == key }
    }
}
