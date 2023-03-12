// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

class BooleanSetting(
    key: String,
    section: String,
    var value: Boolean
) : Setting(key, section) {
    override val valueAsString get() = if (value) "True" else "False"
}
