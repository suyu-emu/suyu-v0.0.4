// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.suyu.suyu_emu.features.input.model

// Must match the corresponding enum in input_common/main.h
enum class InputType(val int: Int) {
    None(0),
    Button(1),
    Stick(2),
    Motion(3),
    Touch(4)
}
