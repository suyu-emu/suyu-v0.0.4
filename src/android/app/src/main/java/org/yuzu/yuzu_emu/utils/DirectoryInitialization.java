package org.yuzu.yuzu_emu.utils;

import android.content.Context;
import android.content.Intent;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import org.yuzu.yuzu_emu.NativeLibrary;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicBoolean;

public final class DirectoryInitialization {
    public static final String BROADCAST_ACTION = "org.yuzu.yuzu_emu.BROADCAST";
    public static final String EXTRA_STATE = "directoryState";
    private static volatile DirectoryInitializationState directoryState = null;
    private static String userPath;
    private static AtomicBoolean isDirectoryInitializationRunning = new AtomicBoolean(false);

    public static void start(Context context) {
        // Can take a few seconds to run, so don't block UI thread.
        ((Runnable) () -> init(context)).run();
    }

    private static void init(Context context) {
        if (!isDirectoryInitializationRunning.compareAndSet(false, true))
            return;

        if (directoryState != DirectoryInitializationState.YUZU_DIRECTORIES_INITIALIZED) {
            initializeInternalStorage(context);
            NativeLibrary.InitializeEmulation();
            directoryState = DirectoryInitializationState.YUZU_DIRECTORIES_INITIALIZED;
        }

        isDirectoryInitializationRunning.set(false);
        sendBroadcastState(directoryState, context);
    }

    public static boolean areDirectoriesReady() {
        return directoryState == DirectoryInitializationState.YUZU_DIRECTORIES_INITIALIZED;
    }

    public static String getUserDirectory() {
        if (directoryState == null) {
            throw new IllegalStateException("DirectoryInitialization has to run at least once!");
        } else if (isDirectoryInitializationRunning.get()) {
            throw new IllegalStateException(
                    "DirectoryInitialization has to finish running first!");
        }
        return userPath;
    }

    public static void initializeInternalStorage(Context context) {
        try {
            userPath = context.getExternalFilesDir(null).getCanonicalPath();
            NativeLibrary.SetAppDirectory(userPath);
        } catch(IOException e) {
            e.printStackTrace();
        }
    }

    private static void sendBroadcastState(DirectoryInitializationState state, Context context) {
        Intent localIntent =
                new Intent(BROADCAST_ACTION)
                        .putExtra(EXTRA_STATE, state);
        LocalBroadcastManager.getInstance(context).sendBroadcast(localIntent);
    }

    public enum DirectoryInitializationState {
        YUZU_DIRECTORIES_INITIALIZED,
        CANT_FIND_EXTERNAL_STORAGE
    }
}
