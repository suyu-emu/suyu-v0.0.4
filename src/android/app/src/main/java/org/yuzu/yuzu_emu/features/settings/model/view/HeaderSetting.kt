// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.features.settings.model.Setting

class HeaderSetting(
    key: String?,
    setting: Setting?,
    titleId: Int,
    descriptionId: Int?
) : SettingsItem(key, null, setting, titleId, descriptionId) {
    override val type = TYPE_HEADER
}
