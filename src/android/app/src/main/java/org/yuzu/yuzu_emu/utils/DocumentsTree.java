package org.yuzu.yuzu_emu.utils;

import android.content.Context;
import android.net.Uri;

import androidx.annotation.Nullable;
import androidx.documentfile.provider.DocumentFile;

import org.yuzu.yuzu_emu.YuzuApplication;
import org.yuzu.yuzu_emu.model.MinimalDocumentFile;

import java.util.HashMap;
import java.util.Map;
import java.util.StringTokenizer;

public class DocumentsTree {
    private DocumentsNode root;
    private final Context context;
    public static final String DELIMITER = "/";

    public DocumentsTree() {
        context = YuzuApplication.getAppContext();
    }

    public void setRoot(Uri rootUri) {
        root = null;
        root = new DocumentsNode();
        root.uri = rootUri;
        root.isDirectory = true;
    }

    public int openContentUri(String filepath, String openmode) {
        DocumentsNode node = resolvePath(filepath);
        if (node == null) {
            return -1;
        }
        return FileUtil.openContentUri(context, node.uri.toString(), openmode);
    }

    public long getFileSize(String filepath) {
        DocumentsNode node = resolvePath(filepath);
        if (node == null || node.isDirectory) {
            return 0;
        }
        return FileUtil.getFileSize(context, node.uri.toString());
    }

    public boolean Exists(String filepath) {
        return resolvePath(filepath) != null;
    }

    @Nullable
    private DocumentsNode resolvePath(String filepath) {
        StringTokenizer tokens = new StringTokenizer(filepath, DELIMITER, false);
        DocumentsNode iterator = root;
        while (tokens.hasMoreTokens()) {
            String token = tokens.nextToken();
            if (token.isEmpty()) continue;
            iterator = find(iterator, token);
            if (iterator == null) return null;
        }
        return iterator;
    }

    @Nullable
    private DocumentsNode find(DocumentsNode parent, String filename) {
        if (parent.isDirectory && !parent.loaded) {
            structTree(parent);
        }
        return parent.children.get(filename);
    }

    /**
     * Construct current level directory tree
     * @param parent parent node of this level
     */
    private void structTree(DocumentsNode parent) {
        MinimalDocumentFile[] documents = FileUtil.listFiles(context, parent.uri);
        for (MinimalDocumentFile document: documents) {
            DocumentsNode node = new DocumentsNode(document);
            node.parent = parent;
            parent.children.put(node.name, node);
        }
        parent.loaded = true;
    }

    public static boolean isNativePath(String path) {
        if (path.length() > 0) {
            return path.charAt(0) == '/';
        }
        return false;
    }

    private static class DocumentsNode {
        private DocumentsNode parent;
        private final Map<String, DocumentsNode> children = new HashMap<>();
        private String name;
        private Uri uri;
        private boolean loaded = false;
        private boolean isDirectory = false;

        private DocumentsNode() {}
        private DocumentsNode(MinimalDocumentFile document) {
            name = document.getFilename();
            uri = document.getUri();
            isDirectory = document.isDirectory();
            loaded = !isDirectory;
        }
        private DocumentsNode(DocumentFile document, boolean isCreateDir) {
            name = document.getName();
            uri = document.getUri();
            isDirectory = isCreateDir;
            loaded = true;
        }

        private void rename(String name) {
            if (parent == null) {
                return;
            }
            parent.children.remove(this.name);
            this.name = name;
            parent.children.put(name, this);
        }
    }
}
