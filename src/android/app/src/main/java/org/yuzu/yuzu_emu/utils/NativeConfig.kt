// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import org.yuzu.yuzu_emu.model.GameDir

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

    @Synchronized
    external fun getBoolean(key: String, needsGlobal: Boolean): Boolean

    @Synchronized
    external fun setBoolean(key: String, value: Boolean)

    @Synchronized
    external fun getByte(key: String, needsGlobal: Boolean): Byte

    @Synchronized
    external fun setByte(key: String, value: Byte)

    @Synchronized
    external fun getShort(key: String, needsGlobal: Boolean): Short

    @Synchronized
    external fun setShort(key: String, value: Short)

    @Synchronized
    external fun getInt(key: String, needsGlobal: Boolean): Int

    @Synchronized
    external fun setInt(key: String, value: Int)

    @Synchronized
    external fun getFloat(key: String, needsGlobal: Boolean): Float

    @Synchronized
    external fun setFloat(key: String, value: Float)

    @Synchronized
    external fun getLong(key: String, needsGlobal: Boolean): Long

    @Synchronized
    external fun setLong(key: String, value: Long)

    @Synchronized
    external fun getString(key: String, needsGlobal: Boolean): String

    @Synchronized
    external fun setString(key: String, value: String)

    external fun getIsRuntimeModifiable(key: String): Boolean

    external fun getPairedSettingKey(key: String): String

    external fun getIsSwitchable(key: String): Boolean

    @Synchronized
    external fun usingGlobal(key: String): Boolean

    @Synchronized
    external fun setGlobal(key: String, global: Boolean)

    external fun getDefaultToString(key: String): String

    /**
     * Gets every [GameDir] in AndroidSettings::values.game_dirs
     */
    @Synchronized
    external fun getGameDirs(): Array<GameDir>

    /**
     * Clears the AndroidSettings::values.game_dirs array and replaces them with the provided array
     */
    @Synchronized
    external fun setGameDirs(dirs: Array<GameDir>)

    /**
     * Adds a single [GameDir] to the AndroidSettings::values.game_dirs array
     */
    @Synchronized
    external fun addGameDir(dir: GameDir)
}
