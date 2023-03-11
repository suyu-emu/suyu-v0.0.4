// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.graphics.BitmapFactory
import com.squareup.picasso.Picasso
import com.squareup.picasso.Request
import com.squareup.picasso.RequestHandler
import org.yuzu.yuzu_emu.NativeLibrary

class GameIconRequestHandler : RequestHandler() {
    override fun canHandleRequest(data: Request): Boolean {
        return "content" == data.uri.scheme
    }

    override fun load(request: Request, networkPolicy: Int): Result {
        val gamePath = request.uri.toString()
        val data = NativeLibrary.GetIcon(gamePath)
        val options = BitmapFactory.Options()
        options.inMutable = true
        val bitmap = BitmapFactory.decodeByteArray(data, 0, data.size, options)
        return Result(bitmap, Picasso.LoadedFrom.DISK)
    }
}
