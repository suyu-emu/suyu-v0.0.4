// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

class HeaderSetting(
    titleId: Int
) : SettingsItem(emptySetting, titleId, 0) {
    override val type = TYPE_HEADER
}
