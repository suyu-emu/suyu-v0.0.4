// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

class FloatSetting(
    key: String,
    section: String,
    var value: Float
) : Setting(key, section) {
    override val valueAsString = value.toString()
}
