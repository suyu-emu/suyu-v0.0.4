// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.content.Context
import android.content.DialogInterface
import android.icu.util.Calendar
import android.icu.util.TimeZone
import android.text.format.DateFormat
import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.lifecycle.ViewModelProvider
import androidx.navigation.findNavController
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.datepicker.MaterialDatePicker
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.slider.Slider
import com.google.android.material.timepicker.MaterialTimePicker
import com.google.android.material.timepicker.TimeFormat
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.SettingsNavigationDirections
import org.yuzu.yuzu_emu.databinding.DialogSliderBinding
import org.yuzu.yuzu_emu.databinding.ListItemSettingBinding
import org.yuzu.yuzu_emu.databinding.ListItemSettingSwitchBinding
import org.yuzu.yuzu_emu.databinding.ListItemSettingsHeaderBinding
import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting
import org.yuzu.yuzu_emu.features.settings.model.ByteSetting
import org.yuzu.yuzu_emu.features.settings.model.FloatSetting
import org.yuzu.yuzu_emu.features.settings.model.ShortSetting
import org.yuzu.yuzu_emu.features.settings.model.view.*
import org.yuzu.yuzu_emu.features.settings.ui.viewholder.*
import org.yuzu.yuzu_emu.model.SettingsViewModel

class SettingsAdapter(
    private val fragment: SettingsFragment,
    private val context: Context
) : RecyclerView.Adapter<SettingViewHolder?>(), DialogInterface.OnClickListener {
    private var settings = ArrayList<SettingsItem>()
    private var clickedItem: SettingsItem? = null
    private var clickedPosition: Int
    private var dialog: AlertDialog? = null
    private var sliderProgress = 0
    private var textSliderValue: TextView? = null

    private val settingsViewModel: SettingsViewModel
        get() = ViewModelProvider(fragment.requireActivity())[SettingsViewModel::class.java]

    private var defaultCancelListener =
        DialogInterface.OnClickListener { _: DialogInterface?, _: Int -> closeDialog() }

    init {
        clickedPosition = -1
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SettingViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        return when (viewType) {
            SettingsItem.TYPE_HEADER -> {
                HeaderViewHolder(ListItemSettingsHeaderBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_SWITCH -> {
                SwitchSettingViewHolder(ListItemSettingSwitchBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_SINGLE_CHOICE, SettingsItem.TYPE_STRING_SINGLE_CHOICE -> {
                SingleChoiceViewHolder(ListItemSettingBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_SLIDER -> {
                SliderViewHolder(ListItemSettingBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_SUBMENU -> {
                SubmenuViewHolder(ListItemSettingBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_DATETIME_SETTING -> {
                DateTimeViewHolder(ListItemSettingBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_RUNNABLE -> {
                RunnableViewHolder(ListItemSettingBinding.inflate(inflater), this)
            }

            else -> {
                // TODO: Create an error view since we can't return null now
                HeaderViewHolder(ListItemSettingsHeaderBinding.inflate(inflater), this)
            }
        }
    }

    override fun onBindViewHolder(holder: SettingViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    private fun getItem(position: Int): SettingsItem = settings[position]

    override fun getItemCount(): Int = settings.size

    override fun getItemViewType(position: Int): Int {
        return getItem(position).type
    }

    fun setSettingsList(settings: ArrayList<SettingsItem>) {
        this.settings = settings
        notifyDataSetChanged()
    }

    fun onBooleanClick(item: SwitchSetting, position: Int, checked: Boolean) {
        item.checked = checked
        settingsViewModel.shouldSave = true
    }

    private fun onSingleChoiceClick(item: SingleChoiceSetting) {
        clickedItem = item
        val value = getSelectionForSingleChoiceValue(item)
        dialog = MaterialAlertDialogBuilder(context)
            .setTitle(item.nameId)
            .setSingleChoiceItems(item.choicesId, value, this)
            .show()
    }

    fun onSingleChoiceClick(item: SingleChoiceSetting, position: Int) {
        clickedPosition = position
        onSingleChoiceClick(item)
    }

    private fun onStringSingleChoiceClick(item: StringSingleChoiceSetting) {
        clickedItem = item
        dialog = MaterialAlertDialogBuilder(context)
            .setTitle(item.nameId)
            .setSingleChoiceItems(item.choices, item.selectValueIndex, this)
            .show()
    }

    fun onStringSingleChoiceClick(item: StringSingleChoiceSetting, position: Int) {
        clickedPosition = position
        onStringSingleChoiceClick(item)
    }

    fun onDateTimeClick(item: DateTimeSetting, position: Int) {
        clickedItem = item
        clickedPosition = position
        val storedTime = item.value * 1000

        // Helper to extract hour and minute from epoch time
        val calendar: Calendar = Calendar.getInstance()
        calendar.timeInMillis = storedTime
        calendar.timeZone = TimeZone.getTimeZone("UTC")

        var timeFormat: Int = TimeFormat.CLOCK_12H
        if (DateFormat.is24HourFormat(context)) {
            timeFormat = TimeFormat.CLOCK_24H
        }

        val datePicker: MaterialDatePicker<Long> = MaterialDatePicker.Builder.datePicker()
            .setSelection(storedTime)
            .setTitleText(R.string.select_rtc_date)
            .build()
        val timePicker: MaterialTimePicker = MaterialTimePicker.Builder()
            .setTimeFormat(timeFormat)
            .setHour(calendar.get(Calendar.HOUR_OF_DAY))
            .setMinute(calendar.get(Calendar.MINUTE))
            .setTitleText(R.string.select_rtc_time)
            .build()

        datePicker.addOnPositiveButtonClickListener {
            timePicker.show(
                fragment.childFragmentManager,
                "TimePicker"
            )
        }
        timePicker.addOnPositiveButtonClickListener {
            var epochTime: Long = datePicker.selection!! / 1000
            epochTime += timePicker.hour.toLong() * 60 * 60
            epochTime += timePicker.minute.toLong() * 60
            if (item.value != epochTime) {
                settingsViewModel.shouldSave = true
                notifyItemChanged(clickedPosition)
                item.value = epochTime
            }
            clickedItem = null
        }
        datePicker.show(
            fragment.childFragmentManager,
            "DatePicker"
        )
    }

    fun onSliderClick(item: SliderSetting, position: Int) {
        clickedItem = item
        clickedPosition = position
        sliderProgress = item.selectedValue as Int

        val inflater = LayoutInflater.from(context)
        val sliderBinding = DialogSliderBinding.inflate(inflater)

        textSliderValue = sliderBinding.textValue
        textSliderValue!!.text = String.format(
            context.getString(R.string.value_with_units),
            sliderProgress.toString(),
            item.units
        )

        sliderBinding.slider.apply {
            valueFrom = item.min.toFloat()
            valueTo = item.max.toFloat()
            value = sliderProgress.toFloat()
            addOnChangeListener { _: Slider, value: Float, _: Boolean ->
                sliderProgress = value.toInt()
                textSliderValue!!.text = String.format(
                    context.getString(R.string.value_with_units),
                    sliderProgress.toString(),
                    item.units
                )
            }
        }

        dialog = MaterialAlertDialogBuilder(context)
            .setTitle(item.nameId)
            .setView(sliderBinding.root)
            .setPositiveButton(android.R.string.ok, this)
            .setNegativeButton(android.R.string.cancel, defaultCancelListener)
            .show()
    }

    fun onSubmenuClick(item: SubmenuSetting) {
        val action = SettingsNavigationDirections.actionGlobalSettingsFragment(item.menuKey, null)
        fragment.view?.findNavController()?.navigate(action)
    }

    override fun onClick(dialog: DialogInterface, which: Int) {
        when (clickedItem) {
            is SingleChoiceSetting -> {
                val scSetting = clickedItem as SingleChoiceSetting
                val value = getValueForSingleChoiceSelection(scSetting, which)
                if (scSetting.selectedValue != value) {
                    settingsViewModel.shouldSave = true
                }

                // Get the backing Setting, which may be null (if for example it was missing from the file)
                scSetting.selectedValue = value
                closeDialog()
            }

            is StringSingleChoiceSetting -> {
                val scSetting = clickedItem as StringSingleChoiceSetting
                val value = scSetting.getValueAt(which)
                if (scSetting.selectedValue != value) settingsViewModel.shouldSave = true
                scSetting.selectedValue = value!!
                closeDialog()
            }

            is SliderSetting -> {
                val sliderSetting = clickedItem as SliderSetting
                if (sliderSetting.selectedValue != sliderProgress) {
                    settingsViewModel.shouldSave = true
                }
                when (sliderSetting.setting) {
                    is ByteSetting -> {
                        val value = sliderProgress.toByte()
                        sliderSetting.selectedValue = value.toInt()
                    }

                    is ShortSetting -> {
                        val value = sliderProgress.toShort()
                        sliderSetting.selectedValue = value.toInt()
                    }

                    is FloatSetting -> {
                        val value = sliderProgress.toFloat()
                        sliderSetting.selectedValue = value.toInt()
                    }

                    else -> {
                        sliderSetting.selectedValue = sliderProgress
                    }
                }
                closeDialog()
            }
        }
        clickedItem = null
        sliderProgress = -1
    }

    fun onLongClick(setting: AbstractSetting, position: Int): Boolean {
        MaterialAlertDialogBuilder(context)
            .setMessage(R.string.reset_setting_confirmation)
            .setPositiveButton(android.R.string.ok) { _: DialogInterface, _: Int ->
                setting.reset()
                notifyItemChanged(position)
                settingsViewModel.shouldSave = true
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()

        return true
    }

    fun closeDialog() {
        if (dialog != null) {
            if (clickedPosition != -1) {
                notifyItemChanged(clickedPosition)
                clickedPosition = -1
            }
            dialog!!.dismiss()
            dialog = null
        }
    }

    private fun getValueForSingleChoiceSelection(item: SingleChoiceSetting, which: Int): Int {
        val valuesId = item.valuesId
        return if (valuesId > 0) {
            val valuesArray = context.resources.getIntArray(valuesId)
            valuesArray[which]
        } else {
            which
        }
    }

    private fun getSelectionForSingleChoiceValue(item: SingleChoiceSetting): Int {
        val value = item.selectedValue
        val valuesId = item.valuesId
        if (valuesId > 0) {
            val valuesArray = context.resources.getIntArray(valuesId)
            for (index in valuesArray.indices) {
                val current = valuesArray[index]
                if (current == value) {
                    return index
                }
            }
        } else {
            return value
        }
        return -1
    }
}
