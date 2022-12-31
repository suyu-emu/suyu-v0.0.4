package org.yuzu.yuzu_emu.utils;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.yuzu.yuzu_emu.utils.DirectoryInitialization.DirectoryInitializationState;

public class DirectoryStateReceiver extends BroadcastReceiver {
    Action1<DirectoryInitializationState> callback;

    public DirectoryStateReceiver(Action1<DirectoryInitializationState> callback) {
        this.callback = callback;
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        DirectoryInitializationState state = (DirectoryInitializationState) intent
                .getSerializableExtra(DirectoryInitialization.EXTRA_STATE);
        callback.call(state);
    }
}
