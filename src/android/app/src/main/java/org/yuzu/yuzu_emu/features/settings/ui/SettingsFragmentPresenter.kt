// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.text.TextUtils
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.Setting
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.model.StringSetting
import org.yuzu.yuzu_emu.features.settings.model.view.*
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile

class SettingsFragmentPresenter(private val fragmentView: SettingsFragmentView) {
    private var menuTag: String? = null
    private lateinit var gameId: String
    private var settings: Settings? = null
    private var settingsList: ArrayList<SettingsItem>? = null

    fun onCreate(menuTag: String, gameId: String) {
        this.gameId = gameId
        this.menuTag = menuTag
    }

    fun onViewCreated(settings: Settings) {
        setSettings(settings)
    }

    fun putSetting(setting: Setting) {
        settings!!.getSection(setting.section)!!.putSetting(setting)
    }

    private fun asStringSetting(setting: Setting?): StringSetting? {
        if (setting == null) {
            return null
        }
        val stringSetting = StringSetting(setting.key, setting.section, setting.valueAsString)
        putSetting(stringSetting)
        return stringSetting
    }

    fun loadDefaultSettings() {
        loadSettingsList()
    }

    fun setSettings(settings: Settings) {
        if (settingsList == null) {
            this.settings = settings
            loadSettingsList()
        } else {
            fragmentView.fragmentActivity.setTitle(R.string.preferences_settings)
            fragmentView.showSettingsList(settingsList!!)
        }
    }

    private fun loadSettingsList() {
        if (!TextUtils.isEmpty(gameId)) {
            fragmentView.fragmentActivity.title = "Game Settings: $gameId"
        }
        val sl = ArrayList<SettingsItem>()
        if (menuTag == null) {
            return
        }
        when (menuTag) {
            SettingsFile.FILE_NAME_CONFIG -> addConfigSettings(sl)
            Settings.SECTION_GENERAL -> addGeneralSettings(sl)
            Settings.SECTION_SYSTEM -> addSystemSettings(sl)
            Settings.SECTION_RENDERER -> addGraphicsSettings(sl)
            Settings.SECTION_AUDIO -> addAudioSettings(sl)
            else -> {
                fragmentView.showToastMessage("Unimplemented menu", false)
                return
            }
        }
        settingsList = sl
        fragmentView.showSettingsList(settingsList!!)
    }

    private fun addConfigSettings(sl: ArrayList<SettingsItem>) {
        fragmentView.fragmentActivity.setTitle(R.string.preferences_settings)
        sl.apply {
            add(
                SubmenuSetting(
                    null,
                    null,
                    R.string.preferences_general,
                    0,
                    Settings.SECTION_GENERAL
                )
            )
            add(
                SubmenuSetting(
                    null,
                    null,
                    R.string.preferences_system,
                    0,
                    Settings.SECTION_SYSTEM
                )
            )
            add(
                SubmenuSetting(
                    null,
                    null,
                    R.string.preferences_graphics,
                    0,
                    Settings.SECTION_RENDERER
                )
            )
            add(
                SubmenuSetting(
                    null,
                    null,
                    R.string.preferences_audio,
                    0,
                    Settings.SECTION_AUDIO
                )
            )
        }
    }

    private fun addGeneralSettings(sl: ArrayList<SettingsItem>) {
        fragmentView.fragmentActivity.setTitle(R.string.preferences_general)
        val rendererSection = settings!!.getSection(Settings.SECTION_RENDERER)
        val frameLimitEnable =
            rendererSection!!.getSetting(SettingsFile.KEY_RENDERER_USE_SPEED_LIMIT)
        val frameLimitValue = rendererSection.getSetting(SettingsFile.KEY_RENDERER_SPEED_LIMIT)
        val cpuSection = settings!!.getSection(Settings.SECTION_CPU)
        val cpuAccuracy = cpuSection!!.getSetting(SettingsFile.KEY_CPU_ACCURACY)
        sl.apply {
            add(
                SwitchSetting(
                    SettingsFile.KEY_RENDERER_USE_SPEED_LIMIT,
                    Settings.SECTION_RENDERER,
                    frameLimitEnable,
                    R.string.frame_limit_enable,
                    R.string.frame_limit_enable_description,
                    true
                )
            )
            add(
                SliderSetting(
                    SettingsFile.KEY_RENDERER_SPEED_LIMIT,
                    Settings.SECTION_RENDERER,
                    frameLimitValue,
                    R.string.frame_limit_slider,
                    R.string.frame_limit_slider_description,
                    1,
                    200,
                    "%",
                    100
                )
            )
            add(
                SingleChoiceSetting(
                    SettingsFile.KEY_CPU_ACCURACY,
                    Settings.SECTION_CPU,
                    cpuAccuracy,
                    R.string.cpu_accuracy,
                    0,
                    R.array.cpuAccuracyNames,
                    R.array.cpuAccuracyValues,
                    0
                )
            )
        }
    }

    private fun addSystemSettings(sl: ArrayList<SettingsItem>) {
        fragmentView.fragmentActivity.setTitle(R.string.preferences_system)
        val systemSection = settings!!.getSection(Settings.SECTION_SYSTEM)
        val dockedMode = systemSection!!.getSetting(SettingsFile.KEY_USE_DOCKED_MODE)
        val region = systemSection.getSetting(SettingsFile.KEY_REGION_INDEX)
        val language = systemSection.getSetting(SettingsFile.KEY_LANGUAGE_INDEX)
        sl.apply {
            add(
                SwitchSetting(
                    SettingsFile.KEY_USE_DOCKED_MODE,
                    Settings.SECTION_SYSTEM,
                    dockedMode,
                    R.string.use_docked_mode,
                    R.string.use_docked_mode_description,
                    true,
                )
            )
            add(
                SingleChoiceSetting(
                    SettingsFile.KEY_REGION_INDEX,
                    Settings.SECTION_SYSTEM,
                    region,
                    R.string.emulated_region,
                    0,
                    R.array.regionNames,
                    R.array.regionValues,
                    -1
                )
            )
            add(
                SingleChoiceSetting(
                    SettingsFile.KEY_LANGUAGE_INDEX,
                    Settings.SECTION_SYSTEM,
                    language,
                    R.string.emulated_language,
                    0,
                    R.array.languageNames,
                    R.array.languageValues,
                    1
                )
            )
        }
    }

    private fun addGraphicsSettings(sl: ArrayList<SettingsItem>) {
        fragmentView.fragmentActivity.setTitle(R.string.preferences_graphics)
        val rendererSection = settings!!.getSection(Settings.SECTION_RENDERER)
        val rendererBackend = rendererSection!!.getSetting(SettingsFile.KEY_RENDERER_BACKEND)
        val rendererAccuracy = rendererSection.getSetting(SettingsFile.KEY_RENDERER_ACCURACY)
        val rendererResolution = rendererSection.getSetting(SettingsFile.KEY_RENDERER_RESOLUTION)
        val rendererAspectRatio =
            rendererSection.getSetting(SettingsFile.KEY_RENDERER_ASPECT_RATIO)
        val rendererForceMaxClocks =
            rendererSection.getSetting(SettingsFile.KEY_RENDERER_FORCE_MAX_CLOCK)
        val rendererAsynchronousShaders =
            rendererSection.getSetting(SettingsFile.KEY_RENDERER_ASYNCHRONOUS_SHADERS)
        val rendererDebug = rendererSection.getSetting(SettingsFile.KEY_RENDERER_DEBUG)
        sl.apply {
            add(
                SingleChoiceSetting(
                    SettingsFile.KEY_RENDERER_BACKEND,
                    Settings.SECTION_RENDERER,
                    rendererBackend,
                    R.string.renderer_api,
                    0,
                    R.array.rendererApiNames,
                    R.array.rendererApiValues,
                    1
                )
            )
            add(
                SingleChoiceSetting(
                    SettingsFile.KEY_RENDERER_ACCURACY,
                    Settings.SECTION_RENDERER,
                    rendererAccuracy,
                    R.string.renderer_accuracy,
                    0,
                    R.array.rendererAccuracyNames,
                    R.array.rendererAccuracyValues,
                    1
                )
            )
            add(
                SingleChoiceSetting(
                    SettingsFile.KEY_RENDERER_RESOLUTION,
                    Settings.SECTION_RENDERER,
                    rendererResolution,
                    R.string.renderer_resolution,
                    0,
                    R.array.rendererResolutionNames,
                    R.array.rendererResolutionValues,
                    2
                )
            )
            add(
                SingleChoiceSetting(
                    SettingsFile.KEY_RENDERER_ASPECT_RATIO,
                    Settings.SECTION_RENDERER,
                    rendererAspectRatio,
                    R.string.renderer_aspect_ratio,
                    0,
                    R.array.rendererAspectRatioNames,
                    R.array.rendererAspectRatioValues,
                    0
                )
            )
            add(
                SwitchSetting(
                    SettingsFile.KEY_RENDERER_FORCE_MAX_CLOCK,
                    Settings.SECTION_RENDERER,
                    rendererForceMaxClocks,
                    R.string.renderer_force_max_clock,
                    R.string.renderer_force_max_clock_description,
                    true
                )
            )
            add(
                SwitchSetting(
                    SettingsFile.KEY_RENDERER_ASYNCHRONOUS_SHADERS,
                    Settings.SECTION_RENDERER,
                    rendererAsynchronousShaders,
                    R.string.renderer_asynchronous_shaders,
                    R.string.renderer_asynchronous_shaders_description,
                    false
                )
            )
            add(
                SwitchSetting(
                    SettingsFile.KEY_RENDERER_DEBUG,
                    Settings.SECTION_RENDERER,
                    rendererDebug,
                    R.string.renderer_debug,
                    R.string.renderer_debug_description,
                    false
                )
            )
        }
    }

    private fun addAudioSettings(sl: ArrayList<SettingsItem>) {
        fragmentView.fragmentActivity.setTitle(R.string.preferences_audio)
        val audioSection = settings!!.getSection(Settings.SECTION_AUDIO)
        val audioVolume = audioSection!!.getSetting(SettingsFile.KEY_AUDIO_VOLUME)
        sl.add(
            SliderSetting(
                SettingsFile.KEY_AUDIO_VOLUME,
                Settings.SECTION_AUDIO,
                audioVolume,
                R.string.audio_volume,
                R.string.audio_volume_description,
                0,
                100,
                "%",
                100
            )
        )
    }
}
