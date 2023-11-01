// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

object Log {
    external fun debug(message: String)

    external fun warning(message: String)

    external fun info(message: String)

    external fun error(message: String)

    external fun critical(message: String)
}
