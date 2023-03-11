package org.yuzu.yuzu_emu.utils

import android.content.Context
import android.content.Intent
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import org.yuzu.yuzu_emu.NativeLibrary
import java.io.IOException
import java.util.concurrent.atomic.AtomicBoolean

object DirectoryInitialization {
    const val BROADCAST_ACTION = "org.yuzu.yuzu_emu.BROADCAST"
    const val EXTRA_STATE = "directoryState"

    @Volatile
    private var directoryState: DirectoryInitializationState? = null
    private var userPath: String? = null
    private val isDirectoryInitializationRunning = AtomicBoolean(false)

    @JvmStatic
    fun start(context: Context) {
        // Can take a few seconds to run, so don't block UI thread.
        Runnable { init(context) }.run()
    }

    private fun init(context: Context) {
        if (!isDirectoryInitializationRunning.compareAndSet(false, true)) return
        if (directoryState != DirectoryInitializationState.YUZU_DIRECTORIES_INITIALIZED) {
            initializeInternalStorage(context)
            NativeLibrary.InitializeEmulation()
            directoryState = DirectoryInitializationState.YUZU_DIRECTORIES_INITIALIZED
        }
        isDirectoryInitializationRunning.set(false)
        sendBroadcastState(directoryState, context)
    }

    fun areDirectoriesReady(): Boolean {
        return directoryState == DirectoryInitializationState.YUZU_DIRECTORIES_INITIALIZED
    }

    val userDirectory: String?
        get() {
            checkNotNull(directoryState) { "DirectoryInitialization has to run at least once!" }
            check(!isDirectoryInitializationRunning.get()) { "DirectoryInitialization has to finish running first!" }
            return userPath
        }

    private fun initializeInternalStorage(context: Context) {
        try {
            userPath = context.getExternalFilesDir(null)!!.canonicalPath
            NativeLibrary.SetAppDirectory(userPath)
        } catch (e: IOException) {
            e.printStackTrace()
        }
    }

    private fun sendBroadcastState(state: DirectoryInitializationState?, context: Context) {
        val localIntent = Intent(BROADCAST_ACTION)
            .putExtra(EXTRA_STATE, state)
        LocalBroadcastManager.getInstance(context).sendBroadcast(localIntent)
    }

    enum class DirectoryInitializationState {
        YUZU_DIRECTORIES_INITIALIZED,
        CANT_FIND_EXTERNAL_STORAGE
    }
}
