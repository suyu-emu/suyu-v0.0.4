// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

interface AbstractSetting {
    val key: String?
    val section: String?
    val isRuntimeEditable: Boolean
    val valueAsString: String
    val defaultValue: Any
}
