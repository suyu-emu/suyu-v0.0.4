// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.network

import android.content.Context
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.StringSetting
import java.net.InetAddress

object NetDataValidators {
    fun roomName(s: String): Boolean {
        return s.length in 3..20
    }

    fun notEmpty(s: String): Boolean {
        return s.isNotEmpty()
    }

    fun token(s: String?): Boolean {
        return s?.matches(Regex("[a-z]{48}")) == true
    }

    fun token(): Boolean {
        return token(StringSetting.WEB_TOKEN.getString())
    }

    fun roomVisibility(s: String, context: Context): Boolean {
        if (s != context.getString(R.string.multiplayer_public_visibility)) {
            return true
        }

        return token()
    }

    fun ipAddress(s: String): Boolean {
        return try {
            InetAddress.getByName(s)
            s.length >= 7
        } catch (_: Exception) {
            false
        }
    }

    fun username(s: String?): Boolean {
        return s?.matches(Regex("^[ a-zA-Z0-9._-]{4,20}$")) == true
    }

    fun username(): Boolean {
        return username(StringSetting.WEB_USERNAME.getString())
    }

    fun port(s: String): Boolean {
        return s.toIntOrNull() in 1..65535
    }
}
