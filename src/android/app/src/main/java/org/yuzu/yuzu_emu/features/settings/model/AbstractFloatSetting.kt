// SPDX-FileCopyrightText: 2023 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.suyu.suyu_emu.features.settings.model

interface AbstractFloatSetting : AbstractSetting {
    fun getFloat(needsGlobal: Boolean = false): Float
    fun setFloat(value: Float)
}
