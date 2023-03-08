package org.yuzu.yuzu_emu.model

import android.content.Context
import android.database.Cursor
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import android.net.Uri
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.utils.FileUtil
import org.yuzu.yuzu_emu.utils.Log
import rx.Observable
import rx.Subscriber
import java.io.File
import java.util.*

/**
 * A helper class that provides several utilities simplifying interaction with
 * the SQLite database.
 */
class GameDatabase(private val context: Context) :
    SQLiteOpenHelper(context, "games.db", null, DB_VERSION) {
    override fun onCreate(database: SQLiteDatabase) {
        Log.debug("[GameDatabase] GameDatabase - Creating database...")
        execSqlAndLog(database, SQL_CREATE_GAMES)
        execSqlAndLog(database, SQL_CREATE_FOLDERS)
    }

    override fun onDowngrade(database: SQLiteDatabase, oldVersion: Int, newVersion: Int) {
        Log.verbose("[GameDatabase] Downgrades not supporting, clearing databases..")
        execSqlAndLog(database, SQL_DELETE_FOLDERS)
        execSqlAndLog(database, SQL_CREATE_FOLDERS)
        execSqlAndLog(database, SQL_DELETE_GAMES)
        execSqlAndLog(database, SQL_CREATE_GAMES)
    }

    override fun onUpgrade(database: SQLiteDatabase, oldVersion: Int, newVersion: Int) {
        Log.info(
            "[GameDatabase] Upgrading database from schema version $oldVersion to $newVersion"
        )

        // Delete all the games
        execSqlAndLog(database, SQL_DELETE_GAMES)
        execSqlAndLog(database, SQL_CREATE_GAMES)
    }

    fun resetDatabase(database: SQLiteDatabase) {
        execSqlAndLog(database, SQL_DELETE_FOLDERS)
        execSqlAndLog(database, SQL_CREATE_FOLDERS)
        execSqlAndLog(database, SQL_DELETE_GAMES)
        execSqlAndLog(database, SQL_CREATE_GAMES)
    }

    fun scanLibrary(database: SQLiteDatabase) {
        // Before scanning known folders, go through the game table and remove any entries for which the file itself is missing.
        val fileCursor = database.query(
            TABLE_NAME_GAMES,
            null,  // Get all columns.
            null,  // Get all rows.
            null,
            null,  // No grouping.
            null,
            null
        ) // Order of games is irrelevant.

        // Possibly overly defensive, but ensures that moveToNext() does not skip a row.
        fileCursor.moveToPosition(-1)
        while (fileCursor.moveToNext()) {
            val gamePath = fileCursor.getString(GAME_COLUMN_PATH)
            val game = File(gamePath)
            if (!game.exists()) {
                database.delete(
                    TABLE_NAME_GAMES,
                    "$KEY_DB_ID = ?",
                    arrayOf(fileCursor.getLong(COLUMN_DB_ID).toString())
                )
            }
        }

        // Get a cursor listing all the folders the user has added to the library.
        val folderCursor = database.query(
            TABLE_NAME_FOLDERS,
            null,  // Get all columns.
            null,  // Get all rows.
            null,
            null,  // No grouping.
            null,
            null
        ) // Order of folders is irrelevant.


        // Possibly overly defensive, but ensures that moveToNext() does not skip a row.
        folderCursor.moveToPosition(-1)

        // Iterate through all results of the DB query (i.e. all folders in the library.)
        while (folderCursor.moveToNext()) {
            val folderPath = folderCursor.getString(FOLDER_COLUMN_PATH)
            val folderUri = Uri.parse(folderPath)
            // If the folder is empty because it no longer exists, remove it from the library.
            if (FileUtil.listFiles(context, folderUri).isEmpty()) {
                Log.error(
                    "[GameDatabase] Folder no longer exists. Removing from the library: $folderPath"
                )
                database.delete(
                    TABLE_NAME_FOLDERS,
                    "$KEY_DB_ID = ?",
                    arrayOf(folderCursor.getLong(COLUMN_DB_ID).toString())
                )
            }
            addGamesRecursive(database, folderUri, Game.extensions, 3)
        }
        fileCursor.close()
        folderCursor.close()
        database.close()
    }

    private fun addGamesRecursive(
        database: SQLiteDatabase,
        parent: Uri,
        allowedExtensions: Set<String>,
        depth: Int
    ) {
        if (depth <= 0)
            return

        // Ensure keys are loaded so that ROM metadata can be decrypted.
        NativeLibrary.ReloadKeys()
        val children = FileUtil.listFiles(context, parent)
        for (file in children) {
            if (file.isDirectory) {
                addGamesRecursive(database, file.uri, Game.extensions, depth - 1)
            } else {
                val filename = file.uri.toString()
                val extensionStart = filename.lastIndexOf('.')
                if (extensionStart > 0) {
                    val fileExtension = filename.substring(extensionStart)

                    // Check that the file has an extension we care about before trying to read out of it.
                    if (allowedExtensions.contains(fileExtension.lowercase(Locale.getDefault()))) {
                        attemptToAddGame(database, filename)
                    }
                }
            }
        }
    }
    // Pass the result cursor to the consumer.

    // Tell the consumer we're done; it will unsubscribe implicitly.
    val games: Observable<Cursor?>
        get() = Observable.create { subscriber: Subscriber<in Cursor?> ->
            Log.info("[GameDatabase] Reading games list...")
            val database = readableDatabase
            val resultCursor = database.query(
                TABLE_NAME_GAMES,
                null,
                null,
                null,
                null,
                null,
                "$KEY_GAME_TITLE ASC"
            )

            // Pass the result cursor to the consumer.
            subscriber.onNext(resultCursor)

            // Tell the consumer we're done; it will unsubscribe implicitly.
            subscriber.onCompleted()
        }

    private fun execSqlAndLog(database: SQLiteDatabase, sql: String) {
        Log.verbose("[GameDatabase] Executing SQL: $sql")
        database.execSQL(sql)
    }

    companion object {
        const val COLUMN_DB_ID = 0
        const val GAME_COLUMN_PATH = 1
        const val GAME_COLUMN_TITLE = 2
        const val GAME_COLUMN_DESCRIPTION = 3
        const val GAME_COLUMN_REGIONS = 4
        const val GAME_COLUMN_GAME_ID = 5
        const val GAME_COLUMN_CAPTION = 6
        const val FOLDER_COLUMN_PATH = 1
        const val KEY_DB_ID = "_id"
        const val KEY_GAME_PATH = "path"
        const val KEY_GAME_TITLE = "title"
        const val KEY_GAME_DESCRIPTION = "description"
        const val KEY_GAME_REGIONS = "regions"
        const val KEY_GAME_ID = "game_id"
        const val KEY_GAME_COMPANY = "company"
        const val KEY_FOLDER_PATH = "path"
        const val TABLE_NAME_FOLDERS = "folders"
        const val TABLE_NAME_GAMES = "games"
        private const val DB_VERSION = 2
        private const val TYPE_PRIMARY = " INTEGER PRIMARY KEY"
        private const val TYPE_INTEGER = " INTEGER"
        private const val TYPE_STRING = " TEXT"
        private const val CONSTRAINT_UNIQUE = " UNIQUE"
        private const val SEPARATOR = ", "
        private const val SQL_CREATE_GAMES = ("CREATE TABLE " + TABLE_NAME_GAMES + "("
                + KEY_DB_ID + TYPE_PRIMARY + SEPARATOR
                + KEY_GAME_PATH + TYPE_STRING + SEPARATOR
                + KEY_GAME_TITLE + TYPE_STRING + SEPARATOR
                + KEY_GAME_DESCRIPTION + TYPE_STRING + SEPARATOR
                + KEY_GAME_REGIONS + TYPE_STRING + SEPARATOR
                + KEY_GAME_ID + TYPE_STRING + SEPARATOR
                + KEY_GAME_COMPANY + TYPE_STRING + ")")
        private const val SQL_CREATE_FOLDERS = ("CREATE TABLE " + TABLE_NAME_FOLDERS + "("
                + KEY_DB_ID + TYPE_PRIMARY + SEPARATOR
                + KEY_FOLDER_PATH + TYPE_STRING + CONSTRAINT_UNIQUE + ")")
        private const val SQL_DELETE_FOLDERS = "DROP TABLE IF EXISTS $TABLE_NAME_FOLDERS"
        private const val SQL_DELETE_GAMES = "DROP TABLE IF EXISTS $TABLE_NAME_GAMES"
        private fun attemptToAddGame(database: SQLiteDatabase, filePath: String) {
            var name = NativeLibrary.GetTitle(filePath)

            // If the game's title field is empty, use the filename.
            if (name.isEmpty()) {
                name = filePath.substring(filePath.lastIndexOf("/") + 1)
            }
            var gameId = NativeLibrary.GetGameId(filePath)

            // If the game's ID field is empty, use the filename without extension.
            if (gameId.isEmpty()) {
                gameId = filePath.substring(
                    filePath.lastIndexOf("/") + 1,
                    filePath.lastIndexOf(".")
                )
            }
            val game = Game.asContentValues(
                name,
                NativeLibrary.GetDescription(filePath).replace("\n", " "),
                NativeLibrary.GetRegions(filePath),
                filePath,
                gameId,
                NativeLibrary.GetCompany(filePath)
            )

            // Try to update an existing game first.
            val rowsMatched = database.update(
                TABLE_NAME_GAMES,  // Which table to update.
                game,  // The values to fill the row with.
                "$KEY_GAME_ID = ?", arrayOf(
                    game.getAsString(
                        KEY_GAME_ID
                    )
                )
            )
            // The ? in WHERE clause is replaced with this,
            // which is provided as an array because there
            // could potentially be more than one argument.

            // If update fails, insert a new game instead.
            if (rowsMatched == 0) {
                Log.verbose("[GameDatabase] Adding game: " + game.getAsString(KEY_GAME_TITLE))
                database.insert(TABLE_NAME_GAMES, null, game)
            } else {
                Log.verbose("[GameDatabase] Updated game: " + game.getAsString(KEY_GAME_TITLE))
            }
        }
    }
}
