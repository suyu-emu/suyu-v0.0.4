// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.content.Context
import android.icu.util.Calendar
import android.icu.util.TimeZone
import android.text.format.DateFormat
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.navigation.findNavController
import androidx.recyclerview.widget.AsyncDifferConfig
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import com.google.android.material.datepicker.MaterialDatePicker
import com.google.android.material.timepicker.MaterialTimePicker
import com.google.android.material.timepicker.TimeFormat
import kotlinx.coroutines.launch
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.SettingsNavigationDirections
import org.yuzu.yuzu_emu.databinding.ListItemSettingBinding
import org.yuzu.yuzu_emu.databinding.ListItemSettingSwitchBinding
import org.yuzu.yuzu_emu.databinding.ListItemSettingsHeaderBinding
import org.yuzu.yuzu_emu.features.settings.model.view.*
import org.yuzu.yuzu_emu.features.settings.ui.viewholder.*
import org.yuzu.yuzu_emu.fragments.SettingsDialogFragment
import org.yuzu.yuzu_emu.model.SettingsViewModel

class SettingsAdapter(
    private val fragment: Fragment,
    private val context: Context
) : ListAdapter<SettingsItem, SettingViewHolder>(
    AsyncDifferConfig.Builder(DiffCallback()).build()
) {
    private val settingsViewModel: SettingsViewModel
        get() = ViewModelProvider(fragment.requireActivity())[SettingsViewModel::class.java]

    init {
        fragment.viewLifecycleOwner.lifecycleScope.launch {
            fragment.repeatOnLifecycle(Lifecycle.State.STARTED) {
                settingsViewModel.adapterItemChanged.collect {
                    if (it != -1) {
                        notifyItemChanged(it)
                        settingsViewModel.setAdapterItemChanged(-1)
                    }
                }
            }
        }
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
        holder.bind(currentList[position])
    }

    override fun getItemCount(): Int = currentList.size

    override fun getItemViewType(position: Int): Int {
        return currentList[position].type
    }

    fun onBooleanClick(item: SwitchSetting, checked: Boolean, position: Int) {
        item.setChecked(checked)
        notifyItemChanged(position)
        settingsViewModel.setShouldReloadSettingsList(true)
    }

    fun onSingleChoiceClick(item: SingleChoiceSetting, position: Int) {
        SettingsDialogFragment.newInstance(
            settingsViewModel,
            item,
            SettingsItem.TYPE_SINGLE_CHOICE,
            position
        ).show(fragment.childFragmentManager, SettingsDialogFragment.TAG)
    }

    fun onStringSingleChoiceClick(item: StringSingleChoiceSetting, position: Int) {
        SettingsDialogFragment.newInstance(
            settingsViewModel,
            item,
            SettingsItem.TYPE_STRING_SINGLE_CHOICE,
            position
        ).show(fragment.childFragmentManager, SettingsDialogFragment.TAG)
    }

    fun onDateTimeClick(item: DateTimeSetting, position: Int) {
        val storedTime = item.getValue() * 1000

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
            if (item.getValue() != epochTime) {
                notifyItemChanged(position)
                item.setValue(epochTime)
            }
        }
        datePicker.show(
            fragment.childFragmentManager,
            "DatePicker"
        )
    }

    fun onSliderClick(item: SliderSetting, position: Int) {
        SettingsDialogFragment.newInstance(
            settingsViewModel,
            item,
            SettingsItem.TYPE_SLIDER,
            position
        ).show(fragment.childFragmentManager, SettingsDialogFragment.TAG)
    }

    fun onSubmenuClick(item: SubmenuSetting) {
        val action = SettingsNavigationDirections.actionGlobalSettingsFragment(item.menuKey, null)
        fragment.view?.findNavController()?.navigate(action)
    }

    fun onLongClick(item: SettingsItem, position: Int): Boolean {
        SettingsDialogFragment.newInstance(
            settingsViewModel,
            item,
            SettingsDialogFragment.TYPE_RESET_SETTING,
            position
        ).show(fragment.childFragmentManager, SettingsDialogFragment.TAG)

        return true
    }

    fun onClearClick(item: SettingsItem, position: Int) {
        item.setting.global = true
        notifyItemChanged(position)
        settingsViewModel.setShouldReloadSettingsList(true)
    }

    private class DiffCallback : DiffUtil.ItemCallback<SettingsItem>() {
        override fun areItemsTheSame(oldItem: SettingsItem, newItem: SettingsItem): Boolean {
            return oldItem.setting.key == newItem.setting.key
        }

        override fun areContentsTheSame(oldItem: SettingsItem, newItem: SettingsItem): Boolean {
            return oldItem.setting.key == newItem.setting.key
        }
    }
}
