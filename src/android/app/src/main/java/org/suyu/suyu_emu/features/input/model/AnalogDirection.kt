// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.suyu.suyu_emu.features.input.model

enum class AnalogDirection(val int: Int, val param: String) {
    Up(0, "up"),
    Down(1, "down"),
    Left(2, "left"),
    Right(3, "right")
}
