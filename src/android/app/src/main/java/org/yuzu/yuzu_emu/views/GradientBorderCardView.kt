// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.views

import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import com.google.android.material.card.MaterialCardView
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.Settings
import androidx.preference.PreferenceManager

class GradientBorderCardView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : MaterialCardView(context, attrs, defStyleAttr) {

    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 6f
    }

    private val borderPath = Path()
    private val borderRect = RectF()
    private var showGradientBorder = false
    private var isEdenTheme = false

    init {
        setWillNotDraw(false)
        updateThemeState()
    }

    private fun updateThemeState() {
        val prefs = PreferenceManager.getDefaultSharedPreferences(context)
        val themeIndex = try {
            prefs.getInt(Settings.PREF_STATIC_THEME_COLOR, 0)
        } catch (e: Exception) {
            0 // Default to Eden theme if error
        }
        isEdenTheme = themeIndex == 0
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)

        // Update border style based on theme
        if (isEdenTheme) {
            // Gradient for Eden theme
            borderPaint.shader = LinearGradient(
                0f,
                0f,
                w.toFloat(),
                h.toFloat(),
                context.getColor(R.color.eden_border_gradient_start),
                context.getColor(R.color.eden_border_gradient_end),
                Shader.TileMode.CLAMP
            )
        } else {
            // Solid color for other themes
            borderPaint.shader = null
            val typedValue = android.util.TypedValue()
            context.theme.resolveAttribute(
                com.google.android.material.R.attr.colorPrimary,
                typedValue,
                true
            )
            borderPaint.color = typedValue.data
        }

        // Update border rect with padding for stroke
        val halfStroke = borderPaint.strokeWidth / 2
        borderRect.set(
            halfStroke,
            halfStroke,
            w - halfStroke,
            h - halfStroke
        )

        // Update path with rounded corners
        borderPath.reset()
        borderPath.addRoundRect(
            borderRect,
            radius,
            radius,
            Path.Direction.CW
        )
    }

    override fun onFocusChanged(gainFocus: Boolean, direction: Int, previouslyFocusedRect: Rect?) {
        super.onFocusChanged(gainFocus, direction, previouslyFocusedRect)
        showGradientBorder = gainFocus
        invalidate()
    }

    override fun setSelected(selected: Boolean) {
        super.setSelected(selected)
        showGradientBorder = selected
        invalidate()
    }

    override fun setPressed(pressed: Boolean) {
        super.setPressed(pressed)
        if (pressed) {
            showGradientBorder = true
            invalidate()
        }
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (showGradientBorder && !isPressed) {
            canvas.drawPath(borderPath, borderPaint)
        }
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        updateThemeState()
        requestLayout()
    }
}
