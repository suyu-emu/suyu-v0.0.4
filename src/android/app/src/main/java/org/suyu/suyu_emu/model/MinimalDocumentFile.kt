// SPDX-FileCopyrightText: 2023 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.suyu.suyu_emu.model

import android.net.Uri
import android.provider.DocumentsContract

class MinimalDocumentFile(val filename: String, mimeType: String, val uri: Uri) {
    val isDirectory: Boolean = mimeType == DocumentsContract.Document.MIME_TYPE_DIR
}
