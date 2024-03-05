// SPDX-FileCopyrightText: 2023 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.suyu.suyu_emu.features.settings.model

interface AbstractIntSetting : AbstractSetting {
    fun getInt(needsGlobal: Boolean = false): Int
    fun setInt(value: Int)
}
