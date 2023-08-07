// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.utils

import java.io.*
import java.util.*
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

/**
 * Contains static methods for interacting with .ini files in which settings are stored.
 */
object SettingsFile {
    const val FILE_NAME_CONFIG = "config"

    private var sectionsMap = BiMap<String?, String?>()

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
        view: SettingsActivityView? = null
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
                    val setting = settingFromLine(line!!)
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

    fun readFile(fileName: String, view: SettingsActivityView?): HashMap<String, SettingSection?> {
        return readFile(getSettingsFile(fileName), false, view)
    }

    fun readFile(fileName: String): HashMap<String, SettingSection?> =
        readFile(getSettingsFile(fileName), false)

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
        view: SettingsActivityView?
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
                    gameId,
                    mapSectionNameFromIni(
                        section.name
                    ),
                    setting!!.key,
                    setting.valueAsString
                )
            }
        }
    }

    private fun mapSectionNameFromIni(generalSectionName: String): String? {
        return if (sectionsMap.getForward(generalSectionName) != null) {
            sectionsMap.getForward(generalSectionName)
        } else {
            generalSectionName
        }
    }

    private fun mapSectionNameToIni(generalSectionName: String): String {
        return if (sectionsMap.getBackward(generalSectionName) != null) {
            sectionsMap.getBackward(generalSectionName).toString()
        } else {
            generalSectionName
        }
    }

    fun getSettingsFile(fileName: String): File {
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
     * @param line    The line of text being parsed.
     * @return A typed Setting containing the key/value contained in the line.
     */
    private fun settingFromLine(line: String): AbstractSetting? {
        val splitLine = line.split("=".toRegex()).dropLastWhile { it.isEmpty() }.toTypedArray()
        if (splitLine.size != 2) {
            return null
        }
        val key = splitLine[0].trim { it <= ' ' }
        val value = splitLine[1].trim { it <= ' ' }
        if (value.isEmpty()) {
            return null
        }

        val booleanSetting = BooleanSetting.from(key)
        if (booleanSetting != null) {
            booleanSetting.boolean = value.toBoolean()
            return booleanSetting
        }

        val intSetting = IntSetting.from(key)
        if (intSetting != null) {
            intSetting.int = value.toInt()
            return intSetting
        }

        val floatSetting = FloatSetting.from(key)
        if (floatSetting != null) {
            floatSetting.float = value.toFloat()
            return floatSetting
        }

        val stringSetting = StringSetting.from(key)
        if (stringSetting != null) {
            stringSetting.string = value
            return stringSetting
        }

        return null
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

        BooleanSetting.values().forEach {
            if (!keySet.contains(it.key)) {
                parser.put(header, it.key, it.valueAsString)
            }
        }
        IntSetting.values().forEach {
            if (!keySet.contains(it.key)) {
                parser.put(header, it.key, it.valueAsString)
            }
        }
        StringSetting.values().forEach {
            if (!keySet.contains(it.key)) {
                parser.put(header, it.key, it.valueAsString)
            }
        }
    }
}
