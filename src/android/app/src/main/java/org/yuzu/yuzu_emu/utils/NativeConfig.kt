// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

object NativeConfig {
    /**
     * Creates a Config object and opens the emulation config.
     */
    @Synchronized
    external fun initializeConfig()

    /**
     * Destroys the stored config object. This automatically saves the existing config.
     */
    @Synchronized
    external fun unloadConfig()

    /**
     * Reads values saved to the config file and saves them.
     */
    @Synchronized
    external fun reloadSettings()

    /**
     * Saves settings values in memory to disk.
     */
    @Synchronized
    external fun saveSettings()

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

    external fun getPairedSettingKey(key: String): String
}
