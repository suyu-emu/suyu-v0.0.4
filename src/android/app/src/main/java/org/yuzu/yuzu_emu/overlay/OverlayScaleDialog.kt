// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.overlay

import android.app.Dialog
import android.content.Context
import android.view.Gravity
import android.view.LayoutInflater
import android.view.WindowManager
import android.widget.TextView
import com.google.android.material.button.MaterialButton
import com.google.android.material.slider.Slider
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.overlay.model.OverlayControlData

class OverlayScaleDialog(
    context: Context,
    private val overlayControlData: OverlayControlData,
    private val onScaleChanged: (Float) -> Unit
) : Dialog(context) {

    private var currentScale = overlayControlData.individualScale
    private val originalScale = overlayControlData.individualScale
    private lateinit var scaleValueText: TextView
    private lateinit var scaleSlider: Slider

    init {
        setupDialog()
    }

    private fun setupDialog() {
        val view = LayoutInflater.from(context).inflate(R.layout.dialog_overlay_scale, null)
        setContentView(view)

        window?.setBackgroundDrawable(null)

        window?.apply {
            attributes = attributes.apply {
                flags = flags and WindowManager.LayoutParams.FLAG_DIM_BEHIND.inv()
                flags = flags or WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
            }
        }

        scaleValueText = view.findViewById(R.id.scaleValueText)
        scaleSlider = view.findViewById(R.id.scaleSlider)
        val resetButton = view.findViewById<MaterialButton>(R.id.resetButton)
        val confirmButton = view.findViewById<MaterialButton>(R.id.confirmButton)
        val cancelButton = view.findViewById<MaterialButton>(R.id.cancelButton)

        scaleValueText.text = String.format("%.1fx",  currentScale)
        scaleSlider.value = currentScale

        scaleSlider.addOnChangeListener { _, value, input ->
            if (input) {
                currentScale = value
                scaleValueText.text = String.format("%.1fx", currentScale)
            }
        }

        scaleSlider.addOnSliderTouchListener(object : Slider.OnSliderTouchListener {
            override fun onStartTrackingTouch(slider: Slider) {
                // pass
            }

            override fun onStopTrackingTouch(slider: Slider) {
                onScaleChanged(currentScale)
            }
        })

        resetButton.setOnClickListener {
            currentScale = 1.0f
            scaleSlider.value = 1.0f
            scaleValueText.text = String.format("%.1fx", currentScale)
            onScaleChanged(currentScale)
        }

        confirmButton.setOnClickListener {
            overlayControlData.individualScale = currentScale
            //slider value is already saved on touch dispatch but just to be sure
            onScaleChanged(currentScale)
            dismiss()
        }

        // both cancel button and back gesture should revert the scale change
        cancelButton.setOnClickListener {
            onScaleChanged(originalScale)
            dismiss()
        }

        setOnCancelListener {
            onScaleChanged(originalScale)
            dismiss()
        }
    }

    fun showDialog(anchorX: Int, anchorY: Int, anchorHeight: Int, anchorWidth: Int) {
        show()

        show()

        // TODO: this calculation is a bit rough, improve it later on
        window?.let { window ->
            val layoutParams = window.attributes
            layoutParams.gravity = Gravity.TOP or Gravity.START

            val density = context.resources.displayMetrics.density
            val dialogWidthPx = (320 * density).toInt()
            val dialogHeightPx = (400 * density).toInt() // set your estimated dialog height

            val screenHeight = context.resources.displayMetrics.heightPixels


            layoutParams.x = anchorX + anchorWidth / 2 - dialogWidthPx / 2
            layoutParams.y = anchorY + anchorHeight / 2 - dialogHeightPx / 2
            layoutParams.width = dialogWidthPx


            window.attributes = layoutParams
        }

    }

}