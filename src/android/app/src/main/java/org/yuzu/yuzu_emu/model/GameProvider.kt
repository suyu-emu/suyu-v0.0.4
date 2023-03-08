package org.yuzu.yuzu_emu.model

import android.content.ContentProvider
import android.content.ContentValues
import android.database.Cursor
import android.database.sqlite.SQLiteDatabase
import android.net.Uri
import org.yuzu.yuzu_emu.BuildConfig
import org.yuzu.yuzu_emu.utils.Log

/**
 * Provides an interface allowing Activities to interact with the SQLite database.
 * CRUD methods in this class can be called by Activities using getContentResolver().
 */
class GameProvider : ContentProvider() {
    private var mDbHelper: GameDatabase? = null
    override fun onCreate(): Boolean {
        Log.info("[GameProvider] Creating Content Provider...")
        mDbHelper = GameDatabase(context!!)
        return true
    }

    override fun query(
        uri: Uri,
        projection: Array<String>?,
        selection: String?,
        selectionArgs: Array<String>?,
        sortOrder: String?
    ): Cursor? {
        Log.info("[GameProvider] Querying URI: $uri")
        val db = mDbHelper!!.readableDatabase
        val table = uri.lastPathSegment
        if (table == null) {
            Log.error("[GameProvider] Badly formatted URI: $uri")
            return null
        }
        val cursor = db.query(table, projection, selection, selectionArgs, null, null, sortOrder)
        cursor.setNotificationUri(context!!.contentResolver, uri)
        return cursor
    }

    override fun getType(uri: Uri): String? {
        Log.verbose("[GameProvider] Getting MIME type for URI: $uri")
        val lastSegment = uri.lastPathSegment
        if (lastSegment == null) {
            Log.error("[GameProvider] Badly formatted URI: $uri")
            return null
        }
        if (lastSegment == GameDatabase.TABLE_NAME_FOLDERS) {
            return MIME_TYPE_FOLDER
        } else if (lastSegment == GameDatabase.TABLE_NAME_GAMES) {
            return MIME_TYPE_GAME
        }
        Log.error("[GameProvider] Unknown MIME type for URI: $uri")
        return null
    }

    override fun insert(uri: Uri, values: ContentValues?): Uri {
        var realUri = uri
        Log.info("[GameProvider] Inserting row at URI: $realUri")
        val database = mDbHelper!!.writableDatabase
        val table = realUri.lastPathSegment
        if (table != null) {
            if (table == RESET_LIBRARY) {
                mDbHelper!!.resetDatabase(database)
                return realUri
            }
            if (table == REFRESH_LIBRARY) {
                Log.info(
                    "[GameProvider] URI specified table REFRESH_LIBRARY. No insertion necessary; refreshing library contents..."
                )
                mDbHelper!!.scanLibrary(database)
                return realUri
            }
            val id =
                database.insertWithOnConflict(table, null, values, SQLiteDatabase.CONFLICT_IGNORE)

            // If insertion was successful...
            if (id > 0) {
                // If we just added a folder, add its contents to the game list.
                if (table == GameDatabase.TABLE_NAME_FOLDERS) {
                    mDbHelper!!.scanLibrary(database)
                }

                // Notify the UI that its contents should be refreshed.
                context!!.contentResolver.notifyChange(realUri, null)
                realUri = Uri.withAppendedPath(realUri, id.toString())
            } else {
                Log.error("[GameProvider] Row already exists: $realUri id: $id")
            }
        } else {
            Log.error("[GameProvider] Badly formatted URI: $realUri")
        }
        database.close()
        return realUri
    }

    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<String>?): Int {
        Log.error("[GameProvider] Delete operations unsupported. URI: $uri")
        return 0
    }

    override fun update(
        uri: Uri, values: ContentValues?, selection: String?,
        selectionArgs: Array<String>?
    ): Int {
        Log.error("[GameProvider] Update operations unsupported. URI: $uri")
        return 0
    }

    companion object {
        const val REFRESH_LIBRARY = "refresh"
        const val RESET_LIBRARY = "reset"
        private const val AUTHORITY = "content://${BuildConfig.APPLICATION_ID}.provider"

        @JvmField
        val URI_FOLDER: Uri = Uri.parse("$AUTHORITY/${GameDatabase.TABLE_NAME_FOLDERS}/")

        @JvmField
        val URI_REFRESH: Uri = Uri.parse("$AUTHORITY/$REFRESH_LIBRARY/")

        @JvmField
        val URI_RESET: Uri = Uri.parse("$AUTHORITY/$RESET_LIBRARY/")
        const val MIME_TYPE_FOLDER = "vnd.android.cursor.item/vnd.yuzu.folder"
        const val MIME_TYPE_GAME = "vnd.android.cursor.item/vnd.yuzu.game"
    }
}
