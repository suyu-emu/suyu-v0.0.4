package org.yuzu.yuzu_emu.utils

import android.content.AsyncQueryHandler
import android.content.ContentValues
import android.content.Context
import android.net.Uri
import org.yuzu.yuzu_emu.model.GameDatabase
import org.yuzu.yuzu_emu.model.GameProvider

class AddDirectoryHelper(private val context: Context) {
    fun addDirectory(dir: String?, onAddUnit: () -> Unit) {
        val handler: AsyncQueryHandler = object : AsyncQueryHandler(context.contentResolver) {
            override fun onInsertComplete(token: Int, cookie: Any?, uri: Uri) {
                onAddUnit.invoke()
            }
        }

        val file = ContentValues()
        file.put(GameDatabase.KEY_FOLDER_PATH, dir)
        handler.startInsert(
            0,  // We don't need to identify this call to the handler
            null,  // We don't need to pass additional data to the handler
            GameProvider.URI_FOLDER,  // Tell the GameProvider we are adding a folder
            file
        )
    }
}
