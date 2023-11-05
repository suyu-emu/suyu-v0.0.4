// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.widget.ImageView
import androidx.core.graphics.drawable.toBitmap
import androidx.core.graphics.drawable.toDrawable
import coil.ImageLoader
import coil.decode.DataSource
import coil.executeBlocking
import coil.fetch.DrawableResult
import coil.fetch.FetchResult
import coil.fetch.Fetcher
import coil.key.Keyer
import coil.memory.MemoryCache
import coil.request.ImageRequest
import coil.request.Options
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.model.Game

class GameIconFetcher(
    private val game: Game,
    private val options: Options
) : Fetcher {
    override suspend fun fetch(): FetchResult {
        return DrawableResult(
            drawable = decodeGameIcon(game.path)!!.toDrawable(options.context.resources),
            isSampled = false,
            dataSource = DataSource.DISK
        )
    }

    private fun decodeGameIcon(uri: String): Bitmap? {
        val data = GameMetadata.getIcon(uri)
        return BitmapFactory.decodeByteArray(
            data,
            0,
            data.size,
            BitmapFactory.Options()
        )
    }

    class Factory : Fetcher.Factory<Game> {
        override fun create(data: Game, options: Options, imageLoader: ImageLoader): Fetcher =
            GameIconFetcher(data, options)
    }
}

class GameIconKeyer : Keyer<Game> {
    override fun key(data: Game, options: Options): String = data.path
}

object GameIconUtils {
    private val imageLoader = ImageLoader.Builder(YuzuApplication.appContext)
        .components {
            add(GameIconKeyer())
            add(GameIconFetcher.Factory())
        }
        .memoryCache {
            MemoryCache.Builder(YuzuApplication.appContext)
                .maxSizePercent(0.25)
                .build()
        }
        .build()

    fun loadGameIcon(game: Game, imageView: ImageView) {
        val request = ImageRequest.Builder(YuzuApplication.appContext)
            .data(game)
            .target(imageView)
            .error(R.drawable.default_icon)
            .build()
        imageLoader.enqueue(request)
    }

    fun getGameIcon(game: Game): Bitmap {
        val request = ImageRequest.Builder(YuzuApplication.appContext)
            .data(game)
            .error(R.drawable.default_icon)
            .build()
        return imageLoader.executeBlocking(request)
            .drawable!!.toBitmap(config = Bitmap.Config.ARGB_8888)
    }
}
