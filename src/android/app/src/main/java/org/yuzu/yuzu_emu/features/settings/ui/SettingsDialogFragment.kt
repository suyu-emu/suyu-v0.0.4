// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.MotionEvent
import android.os.Handler
import android.os.Looper
import androidx.appcompat.app.AlertDialog
import androidx.core.view.isVisible
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.activityViewModels
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.slider.Slider
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.DialogEditTextBinding
import org.yuzu.yuzu_emu.databinding.DialogSliderBinding
import org.yuzu.yuzu_emu.databinding.DialogSpinboxBinding
import org.yuzu.yuzu_emu.features.input.NativeInput
import org.yuzu.yuzu_emu.features.input.model.AnalogDirection
import org.yuzu.yuzu_emu.features.settings.model.view.AnalogInputSetting
import org.yuzu.yuzu_emu.features.settings.model.view.ButtonInputSetting
import org.yuzu.yuzu_emu.features.settings.model.view.IntSingleChoiceSetting
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem
import org.yuzu.yuzu_emu.features.settings.model.view.SingleChoiceSetting
import org.yuzu.yuzu_emu.features.settings.model.view.SliderSetting
import org.yuzu.yuzu_emu.features.settings.model.view.SpinBoxSetting
import org.yuzu.yuzu_emu.features.settings.model.view.StringInputSetting
import org.yuzu.yuzu_emu.features.settings.model.view.StringSingleChoiceSetting
import org.yuzu.yuzu_emu.utils.ParamPackage
import org.yuzu.yuzu_emu.utils.collect

class SettingsDialogFragment : DialogFragment(), DialogInterface.OnClickListener {
    private var type = 0
    private var position = 0

    private var defaultCancelListener =
        DialogInterface.OnClickListener { _: DialogInterface?, _: Int -> closeDialog() }

    private val settingsViewModel: SettingsViewModel by activityViewModels()

    private lateinit var sliderBinding: DialogSliderBinding
    private lateinit var stringInputBinding: DialogEditTextBinding
    private lateinit var spinboxBinding: DialogSpinboxBinding

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
                        when (val item = settingsViewModel.clickedItem) {
                            is AnalogInputSetting -> {
                                val stickParam = NativeInput.getStickParam(
                                    item.playerIndex,
                                    item.nativeAnalog
                                )
                                if (stickParam.get("engine", "") == "analog_from_button") {
                                    when (item.analogDirection) {
                                        AnalogDirection.Up -> stickParam.erase("up")
                                        AnalogDirection.Down -> stickParam.erase("down")
                                        AnalogDirection.Left -> stickParam.erase("left")
                                        AnalogDirection.Right -> stickParam.erase("right")
                                    }
                                    NativeInput.setStickParam(
                                        item.playerIndex,
                                        item.nativeAnalog,
                                        stickParam
                                    )
                                    settingsViewModel.setAdapterItemChanged(position)
                                } else {
                                    NativeInput.setStickParam(
                                        item.playerIndex,
                                        item.nativeAnalog,
                                        ParamPackage()
                                    )
                                    settingsViewModel.setDatasetChanged(true)
                                }
                            }

                            is ButtonInputSetting -> {
                                NativeInput.setButtonParam(
                                    item.playerIndex,
                                    item.nativeButton,
                                    ParamPackage()
                                )
                                settingsViewModel.setAdapterItemChanged(position)
                            }

                            else -> {
                                settingsViewModel.clickedItem!!.setting.reset()
                                settingsViewModel.setAdapterItemChanged(position)
                            }
                        }
                    }
                    .setNegativeButton(android.R.string.cancel, null)
                    .create()
            }

            SettingsItem.TYPE_SINGLE_CHOICE -> {
                val item = settingsViewModel.clickedItem as SingleChoiceSetting
                val value = getSelectionForSingleChoiceValue(item)

                MaterialAlertDialogBuilder(requireContext())
                    .setTitle(item.title)
                    .setSingleChoiceItems(item.choicesId, value, this)
                    .create()
            }

            SettingsItem.TYPE_SLIDER -> {
                sliderBinding = DialogSliderBinding.inflate(layoutInflater)
                val item = settingsViewModel.clickedItem as SliderSetting

                settingsViewModel.setSliderTextValue(item.getSelectedValue().toFloat(), item.units)
                sliderBinding.slider.apply {
                    stepSize = 1.0f
                    valueFrom = item.min.toFloat()
                    valueTo = item.max.toFloat()
                    value = settingsViewModel.sliderProgress.value.toFloat()
                    addOnChangeListener { _: Slider, value: Float, _: Boolean ->
                        settingsViewModel.setSliderTextValue(value, item.units)
                    }
                }

                MaterialAlertDialogBuilder(requireContext())
                    .setTitle(item.title)
                    .setView(sliderBinding.root)
                    .setPositiveButton(android.R.string.ok, this)
                    .setNegativeButton(android.R.string.cancel, defaultCancelListener)
                    .create()
            }

            SettingsItem.TYPE_SPINBOX -> {
                spinboxBinding = DialogSpinboxBinding.inflate(layoutInflater)
                val item = settingsViewModel.clickedItem as SpinBoxSetting

                val currentValue = item.getSelectedValue()
                spinboxBinding.editValue.setText(currentValue.toString())
                spinboxBinding.textInputLayout.hint = getString(item.valueHint)

                val dialog = MaterialAlertDialogBuilder(requireContext())
                    .setTitle(item.title)
                    .setView(spinboxBinding.root)
                    .setPositiveButton(android.R.string.ok, this)
                    .setNegativeButton(android.R.string.cancel, defaultCancelListener)
                    .create()

                val updateButtonState = { enabled: Boolean ->
                    dialog.setOnShowListener { dialogInterface ->
                        (dialogInterface as AlertDialog).getButton(DialogInterface.BUTTON_POSITIVE)?.isEnabled = enabled
                    }
                    if (dialog.isShowing) {
                        dialog.getButton(DialogInterface.BUTTON_POSITIVE)?.isEnabled = enabled
                    }
                }

                val updateValidity = { value: Int ->
                    val isValid = value in item.min..item.max
                    if (isValid) {
                        spinboxBinding.textInputLayout.error = null
                    } else {
                        spinboxBinding.textInputLayout.error = getString(
                            if (value < item.min) R.string.value_too_low else R.string.value_too_high,
                            if (value < item.min) item.min else item.max
                        )
                    }
                    updateButtonState(isValid)
                }

                spinboxBinding.buttonDecrement.setOnClickListener {
                    val current = spinboxBinding.editValue.text.toString().toIntOrNull() ?: currentValue
                    val newValue = current - 1
                    spinboxBinding.editValue.setText(newValue.toString())
                    updateValidity(newValue)
                }

                spinboxBinding.buttonIncrement.setOnClickListener {
                    val current = spinboxBinding.editValue.text.toString().toIntOrNull() ?: currentValue
                    val newValue = current + 1
                    spinboxBinding.editValue.setText(newValue.toString())
                    updateValidity(newValue)
                }

                fun attachRepeat(button: View, delta: Int) {
                    val handler = Handler(Looper.getMainLooper())
                    var runnable: Runnable? = null
                    val initialDelay = 400L
                    val minDelay = 40L
                    val accelerationFactor = 0.75f

                    button.setOnTouchListener { v, event ->
                        when (event.action) {
                            MotionEvent.ACTION_DOWN -> {
                                val current = spinboxBinding.editValue.text.toString().toIntOrNull() ?: currentValue
                                val newValue = (current + delta)
                                spinboxBinding.editValue.setText(newValue.toString())
                                updateValidity(newValue)

                                var delay = initialDelay
                                runnable = object : Runnable {
                                    override fun run() {
                                        val curr = spinboxBinding.editValue.text.toString().toIntOrNull() ?: currentValue
                                        val next = curr + delta
                                        spinboxBinding.editValue.setText(next.toString())
                                        updateValidity(next)
                                        // accelerate
                                        delay = (delay * accelerationFactor).toLong().coerceAtLeast(minDelay)
                                        handler.postDelayed(this, delay)
                                    }
                                }
                                handler.postDelayed(runnable!!, initialDelay)
                                true
                            }

                            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                                if (runnable != null) {
                                    handler.removeCallbacks(runnable!!)
                                    runnable = null
                                }
                                v.performClick()
                                true
                            }

                            else -> false
                        }
                    }
                }

                attachRepeat(spinboxBinding.buttonDecrement, -1)
                attachRepeat(spinboxBinding.buttonIncrement, 1)

                spinboxBinding.editValue.addTextChangedListener(object : TextWatcher {
                    override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
                    override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
                    override fun afterTextChanged(s: Editable?) {
                        val value = s.toString().toIntOrNull()
                        if (value != null) {
                            updateValidity(value)
                        } else {
                            spinboxBinding.textInputLayout.error = getString(R.string.invalid_value)
                            updateButtonState(false)
                        }
                    }
                })

                updateValidity(currentValue)

                dialog
            }

            SettingsItem.TYPE_STRING_INPUT -> {
                stringInputBinding = DialogEditTextBinding.inflate(layoutInflater)
                val item = settingsViewModel.clickedItem as StringInputSetting
                stringInputBinding.editText.setText(item.getSelectedValue())

                val onGenerate = item.onGenerate
                stringInputBinding.generate.isVisible = onGenerate != null

                if (onGenerate != null) {
                    stringInputBinding.generate.setOnClickListener {
                        stringInputBinding.editText.setText(onGenerate())
                    }
                }

                val validator = item.validator

                if (validator != null) {
                    val watcher = object : TextWatcher {
                        override fun beforeTextChanged(
                            s: CharSequence?,
                            start: Int,
                            count: Int,
                            after: Int
                        ) {
                        }

                        override fun onTextChanged(
                            s: CharSequence?,
                            start: Int,
                            before: Int,
                            count: Int
                        ) {
                        }

                        override fun afterTextChanged(s: Editable?) {
                            val isValid = validator(s.toString())
                            stringInputBinding.editTextLayout.isErrorEnabled = !isValid
                            stringInputBinding.editTextLayout.error = if (isValid) {
                                null
                            } else {
                                requireContext().getString(
                                    item.errorId
                                )
                            }
                        }
                    }

                    stringInputBinding.editText.addTextChangedListener(watcher)
                    watcher.afterTextChanged(stringInputBinding.editText.text)
                }

                MaterialAlertDialogBuilder(requireContext())
                    .setTitle(item.title)
                    .setView(stringInputBinding.root)
                    .setPositiveButton(android.R.string.ok, this)
                    .setNegativeButton(android.R.string.cancel, defaultCancelListener)
                    .create()
            }

            SettingsItem.TYPE_STRING_SINGLE_CHOICE -> {
                val item = settingsViewModel.clickedItem as StringSingleChoiceSetting
                MaterialAlertDialogBuilder(requireContext())
                    .setTitle(item.title)
                    .setSingleChoiceItems(item.choices, item.selectedValueIndex, this)
                    .create()
            }

            SettingsItem.TYPE_INT_SINGLE_CHOICE -> {
                val item = settingsViewModel.clickedItem as IntSingleChoiceSetting
                MaterialAlertDialogBuilder(requireContext())
                    .setTitle(item.title)
                    .setSingleChoiceItems(item.choices, item.selectedValueIndex, this)
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
            SettingsItem.TYPE_STRING_INPUT -> stringInputBinding.root
            else -> super.onCreateView(inflater, container, savedInstanceState)
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        when (type) {
            SettingsItem.TYPE_SLIDER -> {
                settingsViewModel.sliderTextValue.collect(viewLifecycleOwner) {
                    sliderBinding.textValue.text = it
                }
                settingsViewModel.sliderProgress.collect(viewLifecycleOwner) {
                    sliderBinding.slider.value = it.toFloat()
                }
            }
        }
    }

    override fun onClick(dialog: DialogInterface, which: Int) {
        when (settingsViewModel.clickedItem) {
            is SingleChoiceSetting -> {
                val scSetting = settingsViewModel.clickedItem as SingleChoiceSetting
                val value = getValueForSingleChoiceSelection(scSetting, which)

                if (value in scSetting.warnChoices) {
                    MaterialAlertDialogBuilder(requireContext())
                        .setTitle(R.string.warning)
                        .setMessage(scSetting.warningMessage)
                        .setPositiveButton(R.string.ok, null)
                        .create()
                        .show()
                }
                scSetting.setSelectedValue(value)

                if (scSetting.setting.key == "app_language") {
                    settingsViewModel.setShouldRecreateForLanguageChange(true)
                    // recreate page apply language change instantly
                    requireActivity().recreate()
                }
            }

            is StringSingleChoiceSetting -> {
                val scSetting = settingsViewModel.clickedItem as StringSingleChoiceSetting
                val value = scSetting.getValueAt(which)
                scSetting.setSelectedValue(value)
            }

            is IntSingleChoiceSetting -> {
                val scSetting = settingsViewModel.clickedItem as IntSingleChoiceSetting
                val value = scSetting.getValueAt(which)
                scSetting.setSelectedValue(value)
            }

            is SliderSetting -> {
                val sliderSetting = settingsViewModel.clickedItem as SliderSetting
                sliderSetting.setSelectedValue(settingsViewModel.sliderProgress.value)
            }

            is SpinBoxSetting -> {
                val spinBoxSetting = settingsViewModel.clickedItem as SpinBoxSetting
                val value = spinboxBinding.editValue.text.toString().toIntOrNull()
                if (value != null && value in spinBoxSetting.min..spinBoxSetting.max) {
                    spinBoxSetting.setSelectedValue(value)
                }
            }

            is StringInputSetting -> {
                val stringInputSetting = settingsViewModel.clickedItem as StringInputSetting
                stringInputSetting.setSelectedValue(
                    (stringInputBinding.editText.text ?: "").toString()
                )
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
