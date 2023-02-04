package org.yuzu.yuzu_emu.utils;

import android.content.Intent;
import androidx.fragment.app.FragmentActivity;

public final class FileBrowserHelper {
    public static void openDirectoryPicker(FragmentActivity activity, int requestCode, int title) {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        i.putExtra(Intent.EXTRA_TITLE, title);
        activity.startActivityForResult(i, requestCode);
    }

    public static void openFilePicker(FragmentActivity activity, int requestCode, int title) {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        intent.putExtra(Intent.EXTRA_TITLE, title);
        intent.setType("*/*");
        activity.startActivityForResult(intent, requestCode);
    }

    public static String getSelectedDirectory(Intent result) {
        return result.getDataString();
    }
}
