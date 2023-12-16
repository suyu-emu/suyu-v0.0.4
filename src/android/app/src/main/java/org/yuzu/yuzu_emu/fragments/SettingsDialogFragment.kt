// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.slider.Slider
import kotlinx.coroutines.launch
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.DialogSliderBinding
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem
import org.yuzu.yuzu_emu.features.settings.model.view.SingleChoiceSetting
import org.yuzu.yuzu_emu.features.settings.model.view.SliderSetting
import org.yuzu.yuzu_emu.features.settings.model.view.StringSingleChoiceSetting
import org.yuzu.yuzu_emu.model.SettingsViewModel

class SettingsDialogFragment : DialogFragment(), DialogInterface.OnClickListener {
    private var type = 0
    private var position = 0

    private var defaultCancelListener =
        DialogInterface.OnClickListener { _: DialogInterface?, _: Int -> closeDialog() }

    private val settingsViewModel: SettingsViewModel by activityViewModels()

    private lateinit var sliderBinding: DialogSliderBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        type = requireArguments().getInt(TYPE)
        position = requireArguments().getInt(POSITION)

        if (settingsViewModel.clickedItem == null) dismiss()
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return when (type) {
            TYPE_RESET_SETTING -> {
                MaterialAlertDialogBuilder(requireContext())
                    .setMessage(R.string.reset_setting_confirmation)
                    .setPositiveButton(android.R.string.ok) { _: DialogInterface, _: Int ->
                        settingsViewModel.clickedItem!!.setting.reset()
                        settingsViewModel.setAdapterItemChanged(position)
                    }
                    .setNegativeButton(android.R.string.cancel, null)
                    .create()
            }

            SettingsItem.TYPE_SINGLE_CHOICE -> {
                val item = settingsViewModel.clickedItem as SingleChoiceSetting
                val value = getSelectionForSingleChoiceValue(item)
                MaterialAlertDialogBuilder(requireContext())
                    .setTitle(item.nameId)
                    .setSingleChoiceItems(item.choicesId, value, this)
                    .create()
            }

            SettingsItem.TYPE_SLIDER -> {
                sliderBinding = DialogSliderBinding.inflate(layoutInflater)
                val item = settingsViewModel.clickedItem as SliderSetting

                settingsViewModel.setSliderTextValue(item.getSelectedValue().toFloat(), item.units)
                sliderBinding.slider.apply {
                    valueFrom = item.min.toFloat()
                    valueTo = item.max.toFloat()
                    value = settingsViewModel.sliderProgress.value.toFloat()
                    addOnChangeListener { _: Slider, value: Float, _: Boolean ->
                        settingsViewModel.setSliderTextValue(value, item.units)
                    }
                }

                MaterialAlertDialogBuilder(requireContext())
                    .setTitle(item.nameId)
                    .setView(sliderBinding.root)
                    .setPositiveButton(android.R.string.ok, this)
                    .setNegativeButton(android.R.string.cancel, defaultCancelListener)
                    .create()
            }

            SettingsItem.TYPE_STRING_SINGLE_CHOICE -> {
                val item = settingsViewModel.clickedItem as StringSingleChoiceSetting
                MaterialAlertDialogBuilder(requireContext())
                    .setTitle(item.nameId)
                    .setSingleChoiceItems(item.choices, item.selectValueIndex, this)
                    .create()
            }

            else -> super.onCreateDialog(savedInstanceState)
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return when (type) {
            SettingsItem.TYPE_SLIDER -> sliderBinding.root
            else -> super.onCreateView(inflater, container, savedInstanceState)
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        when (type) {
            SettingsItem.TYPE_SLIDER -> {
                viewLifecycleOwner.lifecycleScope.launch {
                    repeatOnLifecycle(Lifecycle.State.CREATED) {
                        settingsViewModel.sliderTextValue.collect {
                            sliderBinding.textValue.text = it
                        }
                    }
                    repeatOnLifecycle(Lifecycle.State.CREATED) {
                        settingsViewModel.sliderProgress.collect {
                            sliderBinding.slider.value = it.toFloat()
                        }
                    }
                }
            }
        }
    }

    override fun onClick(dialog: DialogInterface, which: Int) {
        when (settingsViewModel.clickedItem) {
            is SingleChoiceSetting -> {
                val scSetting = settingsViewModel.clickedItem as SingleChoiceSetting
                val value = getValueForSingleChoiceSelection(scSetting, which)
                scSetting.setSelectedValue(value)
            }

            is StringSingleChoiceSetting -> {
                val scSetting = settingsViewModel.clickedItem as StringSingleChoiceSetting
                val value = scSetting.getValueAt(which)
                scSetting.setSelectedValue(value)
            }

            is SliderSetting -> {
                val sliderSetting = settingsViewModel.clickedItem as SliderSetting
                sliderSetting.setSelectedValue(settingsViewModel.sliderProgress.value)
            }
        }
        closeDialog()
    }

    private fun closeDialog() {
        settingsViewModel.setAdapterItemChanged(position)
        settingsViewModel.clickedItem = null
        settingsViewModel.setSliderProgress(-1f)
        dismiss()
    }

    private fun getValueForSingleChoiceSelection(item: SingleChoiceSetting, which: Int): Int {
        val valuesId = item.valuesId
        return if (valuesId > 0) {
            val valuesArray = requireContext().resources.getIntArray(valuesId)
            valuesArray[which]
        } else {
            which
        }
    }

    private fun getSelectionForSingleChoiceValue(item: SingleChoiceSetting): Int {
        val value = item.getSelectedValue()
        val valuesId = item.valuesId
        if (valuesId > 0) {
            val valuesArray = requireContext().resources.getIntArray(valuesId)
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

    companion object {
        const val TAG = "SettingsDialogFragment"

        const val TYPE_RESET_SETTING = -1

        const val TITLE = "Title"
        const val TYPE = "Type"
        const val POSITION = "Position"

        fun newInstance(
            settingsViewModel: SettingsViewModel,
            clickedItem: SettingsItem,
            type: Int,
            position: Int
        ): SettingsDialogFragment {
            when (type) {
                SettingsItem.TYPE_HEADER,
                SettingsItem.TYPE_SWITCH,
                SettingsItem.TYPE_SUBMENU,
                SettingsItem.TYPE_DATETIME_SETTING,
                SettingsItem.TYPE_RUNNABLE ->
                    throw IllegalArgumentException("[SettingsDialogFragment] Incompatible type!")

                SettingsItem.TYPE_SLIDER -> settingsViewModel.setSliderProgress(
                    (clickedItem as SliderSetting).getSelectedValue().toFloat()
                )
            }
            settingsViewModel.clickedItem = clickedItem

            val args = Bundle()
            args.putInt(TYPE, type)
            args.putInt(POSITION, position)
            val fragment = SettingsDialogFragment()
            fragment.arguments = args
            return fragment
        }
    }
}
