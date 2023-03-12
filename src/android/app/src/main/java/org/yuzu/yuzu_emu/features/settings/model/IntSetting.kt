// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

class IntSetting(
    key: String,
    section: String,
    var value: Int
) : Setting(key, section) {
    override val valueAsString get() = value.toString()
}
