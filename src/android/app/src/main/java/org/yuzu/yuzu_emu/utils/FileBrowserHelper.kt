package org.yuzu.yuzu_emu.utils

import android.content.Intent
import androidx.fragment.app.FragmentActivity

object FileBrowserHelper {
    fun openDirectoryPicker(activity: FragmentActivity, requestCode: Int, title: Int) {
        val i = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
        i.putExtra(Intent.EXTRA_TITLE, title)
        activity.startActivityForResult(i, requestCode)
    }

    fun openFilePicker(activity: FragmentActivity, requestCode: Int, title: Int) {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT)
        intent.addCategory(Intent.CATEGORY_OPENABLE)
        intent.flags =
            Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
        intent.putExtra(Intent.EXTRA_TITLE, title)
        intent.type = "*/*"
        activity.startActivityForResult(intent, requestCode)
    }

    fun getSelectedDirectory(result: Intent): String? {
        return result.dataString
    }
}
