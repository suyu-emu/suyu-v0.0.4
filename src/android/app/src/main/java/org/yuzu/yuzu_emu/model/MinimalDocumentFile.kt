package org.yuzu.yuzu_emu.model

import android.net.Uri
import android.provider.DocumentsContract

class MinimalDocumentFile(val filename: String, mimeType: String, val uri: Uri) {
    val isDirectory: Boolean = mimeType == DocumentsContract.Document.MIME_TYPE_DIR
}
