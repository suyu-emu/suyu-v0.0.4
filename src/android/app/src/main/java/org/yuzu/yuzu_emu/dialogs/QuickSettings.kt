// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.dialogs

import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.RadioGroup
import android.widget.TextView
import androidx.drawerlayout.widget.DrawerLayout
import com.google.android.material.color.MaterialColors
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.fragments.EmulationFragment
import org.yuzu.yuzu_emu.utils.NativeConfig
import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractShortSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractIntSetting

class QuickSettings(val emulationFragment: EmulationFragment) {
    // Kinda a crappy workaround to get a title from setting keys
    // Idk how to do this witthout hardcoding every single one
    private fun getSettingTitle(settingKey: String): String {
        return settingKey.replace("_", " ").split(" ")
            .joinToString(" ") { it.replaceFirstChar { c -> c.uppercase() } }

    }

    private fun saveSettings() {
        if (emulationFragment.shouldUseCustom) {
            NativeConfig.savePerGameConfig()
        } else {
            NativeConfig.saveGlobalConfig()
        }
    }

    fun addPerGameConfigStatusIndicator(container: ViewGroup) {
        val inflater = LayoutInflater.from(emulationFragment.requireContext())
        val statusView = inflater.inflate(R.layout.item_quick_settings_status, container, false)

        val statusIcon = statusView.findViewById<android.widget.ImageView>(R.id.status_icon)
        val statusText = statusView.findViewById<TextView>(R.id.status_text)

        statusIcon.setImageResource(R.drawable.ic_settings_outline)
        statusText.text = emulationFragment.getString(R.string.using_per_game_config)
        statusText.setTextColor(
            MaterialColors.getColor(
                statusText,
                com.google.android.material.R.attr.colorPrimary
            )
        )

        container.addView(statusView)
    }

    // settings

    fun addIntSetting(
        container: ViewGroup,
        setting: IntSetting,
        namesArrayId: Int,
        valuesArrayId: Int
    ) {
        val inflater = LayoutInflater.from(emulationFragment.requireContext())
        val itemView = inflater.inflate(R.layout.item_quick_settings_menu, container, false)
        val headerView = itemView.findViewById<ViewGroup>(R.id.setting_header)
        val titleView = itemView.findViewById<TextView>(R.id.setting_title)
        val valueView = itemView.findViewById<TextView>(R.id.setting_value)
        val expandIcon = itemView.findViewById<android.widget.ImageView>(R.id.expand_icon)
        val radioGroup = itemView.findViewById<RadioGroup>(R.id.radio_group)

        titleView.text = getSettingTitle(setting.key)

        val names = emulationFragment.resources.getStringArray(namesArrayId)
        val values = emulationFragment.resources.getIntArray(valuesArrayId)
        val currentIndex = values.indexOf(setting.getInt())

        valueView.text = if (currentIndex >= 0) names[currentIndex] else "Null"
        headerView.visibility = View.VISIBLE

        var isExpanded = false
        names.forEachIndexed { index, name ->
            val radioButton = com.google.android.material.radiobutton.MaterialRadioButton(emulationFragment.requireContext())
            radioButton.text = name
            radioButton.id = View.generateViewId()
            radioButton.isChecked = index == currentIndex
            radioButton.setPadding(16, 8, 16, 8)

            radioButton.setOnCheckedChangeListener { _, isChecked ->
                if (isChecked) {
                    setting.setInt(values[index])
                    saveSettings()
                    valueView.text = name
                }
            }
            radioGroup.addView(radioButton)
        }

        headerView.setOnClickListener {
            isExpanded = !isExpanded
            if (isExpanded) {
                radioGroup.visibility = View.VISIBLE
                expandIcon.animate().rotation(180f).setDuration(200).start()
            } else {
                radioGroup.visibility = View.GONE
                expandIcon.animate().rotation(0f).setDuration(200).start()
            }
        }

        container.addView(itemView)
    }

    fun addBooleanSetting(
        container: ViewGroup,
        setting: BooleanSetting
    ) {
        val inflater = LayoutInflater.from(emulationFragment.requireContext())
        val itemView = inflater.inflate(R.layout.item_quick_settings_menu, container, false)

        val switchContainer = itemView.findViewById<ViewGroup>(R.id.switch_container)
        val titleView = itemView.findViewById<TextView>(R.id.switch_title)
        val switchView = itemView.findViewById<com.google.android.material.materialswitch.MaterialSwitch>(R.id.setting_switch)

        titleView.text = getSettingTitle(setting.key)
        switchContainer.visibility = View.VISIBLE
        switchView.isChecked = setting.getBoolean()

        switchView.setOnCheckedChangeListener { _, isChecked ->
            setting.setBoolean(isChecked)
            saveSettings()
        }

        switchContainer.setOnClickListener {
            switchView.toggle()
        }
        container.addView(itemView)
    }

    fun addSliderSetting(
        container: ViewGroup,
        setting: AbstractSetting,
        minValue: Int = 0,
        maxValue: Int = 100,
        units: String = ""
    ) {
        val inflater = LayoutInflater.from(emulationFragment.requireContext())
        val itemView = inflater.inflate(R.layout.item_quick_settings_menu, container, false)

        val sliderContainer = itemView.findViewById<ViewGroup>(R.id.slider_container)
        val titleView = itemView.findViewById<TextView>(R.id.slider_title)
        val valueDisplay = itemView.findViewById<TextView>(R.id.slider_value_display)
        val slider = itemView.findViewById<com.google.android.material.slider.Slider>(R.id.setting_slider)


        titleView.text = getSettingTitle(setting.key)
        sliderContainer.visibility = View.VISIBLE

        slider.valueFrom = minValue.toFloat()
        slider.valueTo = maxValue.toFloat()
        slider.stepSize = 1f
        val currentValue = when (setting) {
            is AbstractShortSetting -> setting.getShort(needsGlobal = false).toInt()
            is AbstractIntSetting -> setting.getInt(needsGlobal = false)
            else -> 0
        }
        slider.value = currentValue.toFloat().coerceIn(minValue.toFloat(), maxValue.toFloat())

        val displayValue = "${slider.value.toInt()}$units"
        valueDisplay.text = displayValue

        slider.addOnChangeListener { _, value, chanhed ->
            if (chanhed) {
                val intValue = value.toInt()
                when (setting) {
                    is AbstractShortSetting -> setting.setShort(intValue.toShort())
                    is AbstractIntSetting -> setting.setInt(intValue)
                }
                saveSettings()
                valueDisplay.text = "$intValue$units"
            }
        }

        slider.setOnTouchListener { _, event ->
            val drawer = emulationFragment.view?.findViewById<DrawerLayout>(R.id.drawer_layout)
            when (event.action) {
                MotionEvent.ACTION_DOWN -> {
                    drawer?.requestDisallowInterceptTouchEvent(true)
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    drawer?.requestDisallowInterceptTouchEvent(false)
                }
            }
            false
        }

        container.addView(itemView)
    }

    fun addDivider(container: ViewGroup) {
        val inflater = LayoutInflater.from(emulationFragment.requireContext())
        val dividerView = inflater.inflate(R.layout.item_quick_settings_divider, container, false)
        container.addView(dividerView)
    }
}