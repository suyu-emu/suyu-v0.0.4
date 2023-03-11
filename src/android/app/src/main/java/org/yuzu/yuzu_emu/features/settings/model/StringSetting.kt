// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

class StringSetting(
    key: String,
    section: String,
    var value: String
) : Setting(key, section) {
    override var valueAsString = value
}
