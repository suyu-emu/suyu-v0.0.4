// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

object NativeConfig {
    external fun getBoolean(key: String, getDefault: Boolean): Boolean
    external fun setBoolean(key: String, value: Boolean)

    external fun getByte(key: String, getDefault: Boolean): Byte
    external fun setByte(key: String, value: Byte)

    external fun getShort(key: String, getDefault: Boolean): Short
    external fun setShort(key: String, value: Short)

    external fun getInt(key: String, getDefault: Boolean): Int
    external fun setInt(key: String, value: Int)

    external fun getFloat(key: String, getDefault: Boolean): Float
    external fun setFloat(key: String, value: Float)

    external fun getLong(key: String, getDefault: Boolean): Long
    external fun setLong(key: String, value: Long)

    external fun getString(key: String, getDefault: Boolean): String
    external fun setString(key: String, value: String)

    external fun getIsRuntimeModifiable(key: String): Boolean

    external fun getConfigHeader(category: Int): String
}
