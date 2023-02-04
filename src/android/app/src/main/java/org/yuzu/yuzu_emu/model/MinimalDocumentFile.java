package org.yuzu.yuzu_emu.model;

import android.net.Uri;
import android.provider.DocumentsContract;

public class MinimalDocumentFile {
    private final String filename;
    private final Uri uri;
    private final String mimeType;

    public MinimalDocumentFile(String filename, String mimeType, Uri uri) {
        this.filename = filename;
        this.mimeType = mimeType;
        this.uri = uri;
    }

    public String getFilename() {
        return filename;
    }

    public Uri getUri() {
        return uri;
    }

    public boolean isDirectory() {
        return mimeType.equals(DocumentsContract.Document.MIME_TYPE_DIR);
    }
}
