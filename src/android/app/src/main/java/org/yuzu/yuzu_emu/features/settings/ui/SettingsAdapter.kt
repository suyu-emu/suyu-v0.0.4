package org.yuzu.yuzu_emu.features.settings.ui

import android.content.Context
import android.content.DialogInterface
import android.icu.util.Calendar
import android.icu.util.TimeZone
import android.text.format.DateFormat
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.datepicker.MaterialDatePicker
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.slider.Slider
import com.google.android.material.timepicker.MaterialTimePicker
import com.google.android.material.timepicker.TimeFormat
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.FloatSetting
import org.yuzu.yuzu_emu.features.settings.model.view.*
import org.yuzu.yuzu_emu.features.settings.ui.viewholder.*

class SettingsAdapter(
    private val fragmentView: SettingsFragmentView,
    private val context: Context
) : RecyclerView.Adapter<SettingViewHolder?>(), DialogInterface.OnClickListener {
    private var settings: ArrayList<SettingsItem>? = null
    private var clickedItem: SettingsItem? = null
    private var clickedPosition: Int
    private var dialog: AlertDialog? = null
    private var sliderProgress = 0
    private var textSliderValue: TextView? = null

    private var defaultCancelListener =
        DialogInterface.OnClickListener { _: DialogInterface?, _: Int -> closeDialog() }

    init {
        clickedPosition = -1
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SettingViewHolder {
        val view: View
        val inflater = LayoutInflater.from(parent.context)
        return when (viewType) {
            SettingsItem.TYPE_HEADER -> {
                view = inflater.inflate(R.layout.list_item_settings_header, parent, false)
                HeaderViewHolder(view, this)
            }
            SettingsItem.TYPE_CHECKBOX -> {
                view = inflater.inflate(R.layout.list_item_setting_checkbox, parent, false)
                CheckBoxSettingViewHolder(view, this)
            }
            SettingsItem.TYPE_SINGLE_CHOICE, SettingsItem.TYPE_STRING_SINGLE_CHOICE -> {
                view = inflater.inflate(R.layout.list_item_setting, parent, false)
                SingleChoiceViewHolder(view, this)
            }
            SettingsItem.TYPE_SLIDER -> {
                view = inflater.inflate(R.layout.list_item_setting, parent, false)
                SliderViewHolder(view, this)
            }
            SettingsItem.TYPE_SUBMENU -> {
                view = inflater.inflate(R.layout.list_item_setting, parent, false)
                SubmenuViewHolder(view, this)
            }
            SettingsItem.TYPE_DATETIME_SETTING -> {
                view = inflater.inflate(R.layout.list_item_setting, parent, false)
                DateTimeViewHolder(view, this)
            }
            else -> {
                // TODO: Create an error view since we can't return null now
                view = inflater.inflate(R.layout.list_item_settings_header, parent, false)
                HeaderViewHolder(view, this)
            }
        }
    }

    override fun onBindViewHolder(holder: SettingViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    private fun getItem(position: Int): SettingsItem {
        return settings!![position]
    }

    override fun getItemCount(): Int {
        return if (settings != null) {
            settings!!.size
        } else {
            0
        }
    }

    override fun getItemViewType(position: Int): Int {
        return getItem(position).type
    }

    fun setSettings(settings: ArrayList<SettingsItem>?) {
        this.settings = settings
        notifyDataSetChanged()
    }

    fun onBooleanClick(item: CheckBoxSetting, position: Int, checked: Boolean) {
        val setting = item.setChecked(checked)
        notifyItemChanged(position)
        if (setting != null) {
            fragmentView.putSetting(setting)
        }
        fragmentView.onSettingChanged()
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
            .setSingleChoiceItems(item.choicesId, item.selectValueIndex, this)
            .show()
    }

    fun onStringSingleChoiceClick(item: StringSingleChoiceSetting, position: Int) {
        clickedPosition = position
        onStringSingleChoiceClick(item)
    }

    fun onDateTimeClick(item: DateTimeSetting, position: Int) {
        clickedItem = item
        clickedPosition = position
        val storedTime = java.lang.Long.decode(item.value) * 1000

        // Helper to extract hour and minute from epoch time
        val calendar: Calendar = Calendar.getInstance()
        calendar.timeInMillis = storedTime
        calendar.timeZone = TimeZone.getTimeZone("UTC")

        var timeFormat: Int = TimeFormat.CLOCK_12H
        if (DateFormat.is24HourFormat(fragmentView.fragmentActivity)) {
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
                fragmentView.fragmentActivity.supportFragmentManager,
                "TimePicker"
            )
        }
        timePicker.addOnPositiveButtonClickListener {
            var epochTime: Long = datePicker.selection!! / 1000
            epochTime += timePicker.hour.toLong() * 60 * 60
            epochTime += timePicker.minute.toLong() * 60
            val rtcString = epochTime.toString()
            if (item.value != rtcString) {
                notifyItemChanged(clickedPosition)
                fragmentView.onSettingChanged()
            }
            item.setSelectedValue(rtcString)
            clickedItem = null
        }
        datePicker.show(fragmentView.fragmentActivity.supportFragmentManager, "DatePicker")
    }

    fun onSliderClick(item: SliderSetting, position: Int) {
        clickedItem = item
        clickedPosition = position
        sliderProgress = item.selectedValue

        val inflater = LayoutInflater.from(context)
        val sliderLayout = inflater.inflate(R.layout.dialog_slider, null)
        val sliderView = sliderLayout.findViewById<Slider>(R.id.slider)

        textSliderValue = sliderLayout.findViewById(R.id.text_value)
        textSliderValue!!.text = sliderProgress.toString()
        val units = sliderLayout.findViewById<TextView>(R.id.text_units)
        units.text = item.units

        sliderView.apply {
            valueFrom = item.min.toFloat()
            valueTo = item.max.toFloat()
            value = sliderProgress.toFloat()
            addOnChangeListener { _: Slider, value: Float, _: Boolean ->
                sliderProgress = value.toInt()
                textSliderValue!!.text = sliderProgress.toString()
            }
        }

        dialog = MaterialAlertDialogBuilder(context)
            .setTitle(item.nameId)
            .setView(sliderLayout)
            .setPositiveButton(android.R.string.ok, this)
            .setNegativeButton(android.R.string.cancel, defaultCancelListener)
            .setNeutralButton(R.string.slider_default) { dialog: DialogInterface, which: Int ->
                sliderView.value = item.defaultValue.toFloat()
                onClick(dialog, which)
            }
            .show()
    }

    fun onSubmenuClick(item: SubmenuSetting) {
        fragmentView.loadSubMenu(item.menuKey)
    }

    override fun onClick(dialog: DialogInterface, which: Int) {
        when (clickedItem) {
            is SingleChoiceSetting -> {
                val scSetting = clickedItem as SingleChoiceSetting
                val value = getValueForSingleChoiceSelection(scSetting, which)
                if (scSetting.selectedValue != value) {
                    fragmentView.onSettingChanged()
                }

                // Get the backing Setting, which may be null (if for example it was missing from the file)
                val setting = scSetting.setSelectedValue(value)
                if (setting != null) {
                    fragmentView.putSetting(setting)
                }
                closeDialog()
            }
            is StringSingleChoiceSetting -> {
                val scSetting = clickedItem as StringSingleChoiceSetting
                val value = scSetting.getValueAt(which)
                if (scSetting.selectedValue != value) fragmentView.onSettingChanged()
                val setting = scSetting.setSelectedValue(value)
                if (setting != null) {
                    fragmentView.putSetting(setting)
                }
                closeDialog()
            }
            is SliderSetting -> {
                val sliderSetting = clickedItem as SliderSetting
                if (sliderSetting.selectedValue != sliderProgress) {
                    fragmentView.onSettingChanged()
                }
                if (sliderSetting.setting is FloatSetting) {
                    val value = sliderProgress.toFloat()
                    val setting = sliderSetting.setSelectedValue(value)
                    if (setting != null) {
                        fragmentView.putSetting(setting)
                    }
                } else {
                    val setting = sliderSetting.setSelectedValue(sliderProgress)
                    if (setting != null) {
                        fragmentView.putSetting(setting)
                    }
                }
                closeDialog()
            }
        }
        clickedItem = null
        sliderProgress = -1
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
