package org.yuzu.yuzu_emu

import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import org.yuzu.yuzu_emu.model.GameDatabase
import org.yuzu.yuzu_emu.utils.DirectoryInitialization.start
import org.yuzu.yuzu_emu.utils.DocumentsTree
import org.yuzu.yuzu_emu.utils.GpuDriverHelper

class YuzuApplication : Application() {
    private fun createNotificationChannel() {
        // Create the NotificationChannel, but only on API 26+ because
        // the NotificationChannel class is new and not in the support library
        val name: CharSequence = getString(R.string.app_notification_channel_name)
        val description = getString(R.string.app_notification_channel_description)
        val channel = NotificationChannel(
            getString(R.string.app_notification_channel_id),
            name,
            NotificationManager.IMPORTANCE_LOW
        )
        channel.description = description
        channel.setSound(null, null)
        channel.vibrationPattern = null
        // Register the channel with the system; you can't change the importance
        // or other notification behaviors after this
        val notificationManager = getSystemService(NotificationManager::class.java)
        notificationManager.createNotificationChannel(channel)
    }

    override fun onCreate() {
        super.onCreate()
        application = this
        documentsTree = DocumentsTree()
        start(applicationContext)
        GpuDriverHelper.initializeDriverParameters(applicationContext)
        NativeLibrary.LogDeviceInfo()

        // TODO(bunnei): Disable notifications until we support app suspension.
        //createNotificationChannel();
        databaseHelper = GameDatabase(this)
    }

    companion object {
        var databaseHelper: GameDatabase? = null

        @JvmField
        var documentsTree: DocumentsTree? = null
        private var application: YuzuApplication? = null

        @JvmStatic
        val appContext: Context
            get() = application!!.applicationContext
    }
}
