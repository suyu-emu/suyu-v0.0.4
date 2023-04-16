// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.utils

import org.ini4j.Wini
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.*
import org.yuzu.yuzu_emu.features.settings.model.Settings.SettingsSectionMap
import org.yuzu.yuzu_emu.features.settings.ui.SettingsActivityView
import org.yuzu.yuzu_emu.utils.BiMap
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.Log
import java.io.*
import java.util.*

/**
 * Contains static methods for interacting with .ini files in which settings are stored.
 */
object SettingsFile {
    const val FILE_NAME_CONFIG = "config"
    const val KEY_DESIGN = "design"

    // CPU
    const val KEY_CPU_ACCURACY = "cpu_accuracy"

    // System
    const val KEY_USE_DOCKED_MODE = "use_docked_mode"
    const val KEY_REGION_INDEX = "region_index"
    const val KEY_LANGUAGE_INDEX = "language_index"
    const val KEY_RENDERER_BACKEND = "backend"

    // Renderer
    const val KEY_RENDERER_RESOLUTION = "resolution_setup"
    const val KEY_RENDERER_SCALING_FILTER = "scaling_filter"
    const val KEY_RENDERER_ANTI_ALIASING = "anti_aliasing"
    const val KEY_RENDERER_ASPECT_RATIO = "aspect_ratio"
    const val KEY_RENDERER_ACCURACY = "gpu_accuracy"
    const val KEY_RENDERER_USE_DISK_SHADER_CACHE = "use_disk_shader_cache"
    const val KEY_RENDERER_ASYNCHRONOUS_SHADERS = "use_asynchronous_shaders"
    const val KEY_RENDERER_FORCE_MAX_CLOCK = "force_max_clock"
    const val KEY_RENDERER_USE_SPEED_LIMIT = "use_speed_limit"
    const val KEY_RENDERER_DEBUG = "debug"
    const val KEY_RENDERER_SPEED_LIMIT = "speed_limit"

    // Audio
    const val KEY_AUDIO_VOLUME = "volume"
    private val sectionsMap = BiMap<String?, String?>()

    /**
     * Reads a given .ini file from disk and returns it as a HashMap of Settings, themselves
     * effectively a HashMap of key/value settings. If unsuccessful, outputs an error telling why it
     * failed.
     *
     * @param ini          The ini file to load the settings from
     * @param isCustomGame
     * @param view         The current view.
     * @return An Observable that emits a HashMap of the file's contents, then completes.
     */
    private fun readFile(
        ini: File?,
        isCustomGame: Boolean,
        view: SettingsActivityView?
    ): HashMap<String, SettingSection?> {
        val sections: HashMap<String, SettingSection?> = SettingsSectionMap()
        var reader: BufferedReader? = null
        try {
            reader = BufferedReader(FileReader(ini))
            var current: SettingSection? = null
            var line: String?
            while (reader.readLine().also { line = it } != null) {
                if (line!!.startsWith("[") && line!!.endsWith("]")) {
                    current = sectionFromLine(line!!, isCustomGame)
                    sections[current.name] = current
                } else if (current != null) {
                    val setting = settingFromLine(current, line!!)
                    if (setting != null) {
                        current.putSetting(setting)
                    }
                }
            }
        } catch (e: FileNotFoundException) {
            Log.error("[SettingsFile] File not found: " + e.message)
            view?.onSettingsFileNotFound()
        } catch (e: IOException) {
            Log.error("[SettingsFile] Error reading from: " + e.message)
            view?.onSettingsFileNotFound()
        } finally {
            if (reader != null) {
                try {
                    reader.close()
                } catch (e: IOException) {
                    Log.error("[SettingsFile] Error closing: " + e.message)
                }
            }
        }
        return sections
    }

    fun readFile(fileName: String, view: SettingsActivityView): HashMap<String, SettingSection?> {
        return readFile(getSettingsFile(fileName), false, view)
    }

    /**
     * Reads a given .ini file from disk and returns it as a HashMap of SettingSections, themselves
     * effectively a HashMap of key/value settings. If unsuccessful, outputs an error telling why it
     * failed.
     *
     * @param gameId the id of the game to load it's settings.
     * @param view   The current view.
     */
    fun readCustomGameSettings(
        gameId: String,
        view: SettingsActivityView
    ): HashMap<String, SettingSection?> {
        return readFile(getCustomGameSettingsFile(gameId), true, view)
    }

    /**
     * Saves a Settings HashMap to a given .ini file on disk. If unsuccessful, outputs an error
     * telling why it failed.
     *
     * @param fileName The target filename without a path or extension.
     * @param sections The HashMap containing the Settings we want to serialize.
     * @param view     The current view.
     */
    fun saveFile(
        fileName: String,
        sections: TreeMap<String, SettingSection>,
        view: SettingsActivityView
    ) {
        val ini = getSettingsFile(fileName)
        try {
            val writer = Wini(ini)
            val keySet: Set<String> = sections.keys
            for (key in keySet) {
                val section = sections[key]
                writeSection(writer, section!!)
            }
            writer.store()
        } catch (e: IOException) {
            Log.error("[SettingsFile] File not found: " + fileName + ".ini: " + e.message)
            view.showToastMessage(
                YuzuApplication.appContext
                    .getString(R.string.error_saving, fileName, e.message),
                false
            )
        }
    }

    fun saveCustomGameSettings(gameId: String?, sections: HashMap<String, SettingSection?>) {
        val sortedSections: Set<String> = TreeSet(sections.keys)
        for (sectionKey in sortedSections) {
            val section = sections[sectionKey]
            val settings = section!!.settings
            val sortedKeySet: Set<String> = TreeSet(settings.keys)
            for (settingKey in sortedKeySet) {
                val setting = settings[settingKey]
                NativeLibrary.setUserSetting(
                    gameId, mapSectionNameFromIni(
                        section.name
                    ), setting!!.key, setting.valueAsString
                )
            }
        }
    }

    private fun mapSectionNameFromIni(generalSectionName: String): String? {
        return if (sectionsMap.getForward(generalSectionName) != null) {
            sectionsMap.getForward(generalSectionName)
        } else generalSectionName
    }

    private fun mapSectionNameToIni(generalSectionName: String): String {
        return if (sectionsMap.getBackward(generalSectionName) != null) {
            sectionsMap.getBackward(generalSectionName).toString()
        } else generalSectionName
    }

    private fun getSettingsFile(fileName: String): File {
        return File(
            DirectoryInitialization.userDirectory + "/config/" + fileName + ".ini"
        )
    }

    private fun getCustomGameSettingsFile(gameId: String): File {
        return File(DirectoryInitialization.userDirectory + "/GameSettings/" + gameId + ".ini")
    }

    private fun sectionFromLine(line: String, isCustomGame: Boolean): SettingSection {
        var sectionName: String = line.substring(1, line.length - 1)
        if (isCustomGame) {
            sectionName = mapSectionNameToIni(sectionName)
        }
        return SettingSection(sectionName)
    }

    /**
     * For a line of text, determines what type of data is being represented, and returns
     * a Setting object containing this data.
     *
     * @param current The section currently being parsed by the consuming method.
     * @param line    The line of text being parsed.
     * @return A typed Setting containing the key/value contained in the line.
     */
    private fun settingFromLine(current: SettingSection, line: String): Setting? {
        val splitLine = line.split("=".toRegex()).dropLastWhile { it.isEmpty() }.toTypedArray()
        if (splitLine.size != 2) {
            Log.warning("Skipping invalid config line \"$line\"")
            return null
        }
        val key = splitLine[0].trim { it <= ' ' }
        val value = splitLine[1].trim { it <= ' ' }
        if (value.isEmpty()) {
            Log.warning("Skipping null value in config line \"$line\"")
            return null
        }
        try {
            val valueAsInt = value.toInt()
            return IntSetting(key, current.name, valueAsInt)
        } catch (_: NumberFormatException) {
        }
        try {
            val valueAsFloat = value.toFloat()
            return FloatSetting(key, current.name, valueAsFloat)
        } catch (_: NumberFormatException) {
        }
        return StringSetting(key, current.name, value)
    }

    /**
     * Writes the contents of a Section HashMap to disk.
     *
     * @param parser  A Wini pointed at a file on disk.
     * @param section A section containing settings to be written to the file.
     */
    private fun writeSection(parser: Wini, section: SettingSection) {
        // Write the section header.
        val header = section.name

        // Write this section's values.
        val settings = section.settings
        val keySet: Set<String> = settings.keys
        for (key in keySet) {
            val setting = settings[key]
            parser.put(header, setting!!.key, setting.valueAsString)
        }
    }
}
