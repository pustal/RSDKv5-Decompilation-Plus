package org.rems.rsdkv5;

import android.database.Cursor;
import android.database.MatrixCursor;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import android.provider.DocumentsContract.Root;
import android.provider.DocumentsProvider;
import android.webkit.MimeTypeMap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

/**
 * The game's own folder (the app's external files folder,
 * Android/data/org.rems.rsdkv5/files) offered as a document tree, for devices
 * without the system folder picker: many Android TVs have no app that answers
 * ACTION_OPEN_DOCUMENT_TREE. The game reads and writes its files through a tree
 * URI (see RSDK.java), so this provider gives it one, over plain files.
 *
 * Document ids are paths: "game" is the folder, "game/Data.rsdk" a file in it,
 * as the system's storage provider does ("primary:path"), because RSDK.java
 * builds a file's URI by appending its path to the folder's document id.
 */
public class GameFolderProvider extends DocumentsProvider {

    /** This provider's authority (see AndroidManifest.xml). */
    public static final String AUTHORITY = "org.rems.rsdkv5.gamefolder";
    /** The document id of the game's folder. */
    public static final String ROOT_ID = "game";

    private static final String[] DEFAULT_ROOT_PROJECTION = {
            Root.COLUMN_ROOT_ID, Root.COLUMN_DOCUMENT_ID, Root.COLUMN_TITLE, Root.COLUMN_FLAGS, Root.COLUMN_ICON,
    };
    private static final String[] DEFAULT_DOCUMENT_PROJECTION = {
            Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME, Document.COLUMN_MIME_TYPE, Document.COLUMN_SIZE,
            Document.COLUMN_LAST_MODIFIED, Document.COLUMN_FLAGS,
    };

    /** The tree URI of the game's folder, as the folder picker would have returned it. */
    public static android.net.Uri treeUri() {
        return DocumentsContract.buildTreeDocumentUri(AUTHORITY, ROOT_ID);
    }

    /** The game's folder on the device (created when missing). */
    public static File folder(@NonNull android.content.Context context) {
        File dir = context.getExternalFilesDir(null);
        if (dir == null)
            dir = context.getFilesDir();
        if (!dir.exists())
            //noinspection ResultOfMethodCallIgnored
            dir.mkdirs();
        return dir;
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    /** The file of the document @p documentId. */
    private File fileFor(String documentId) throws FileNotFoundException {
        if (documentId.equals(ROOT_ID))
            return folder(getContext());
        if (!documentId.startsWith(ROOT_ID + "/"))
            throw new FileNotFoundException("Not in the game's folder: " + documentId);
        String relative = documentId.substring(ROOT_ID.length() + 1);
        // No way out of the game's folder.
        for (String part : relative.split("/")) {
            if (part.equals(".."))
                throw new FileNotFoundException("Invalid path: " + documentId);
        }
        return new File(folder(getContext()), relative);
    }

    /** The document id of @p file (inside the game's folder). */
    private String idFor(File file) {
        String base = folder(getContext()).getAbsolutePath();
        String path = file.getAbsolutePath();
        if (path.equals(base))
            return ROOT_ID;
        return ROOT_ID + "/" + path.substring(base.length() + 1);
    }

    /** Adds one row describing @p file to @p cursor. */
    private void addRow(MatrixCursor cursor, File file) {
        boolean dir = file.isDirectory();
        int flags = Document.FLAG_SUPPORTS_DELETE | Document.FLAG_SUPPORTS_RENAME;
        flags |= dir ? Document.FLAG_DIR_SUPPORTS_CREATE : Document.FLAG_SUPPORTS_WRITE;
        String mime = dir ? Document.MIME_TYPE_DIR : mimeFor(file.getName());
        cursor.newRow()
                .add(Document.COLUMN_DOCUMENT_ID, idFor(file))
                .add(Document.COLUMN_DISPLAY_NAME, file.getName())
                .add(Document.COLUMN_MIME_TYPE, mime)
                .add(Document.COLUMN_SIZE, dir ? null : file.length())
                .add(Document.COLUMN_LAST_MODIFIED, file.lastModified())
                .add(Document.COLUMN_FLAGS, flags);
    }

    /** A MIME type for @p name, from its extension (octet-stream when unknown). */
    private static String mimeFor(String name) {
        int dot = name.lastIndexOf('.');
        if (dot >= 0) {
            String type = MimeTypeMap.getSingleton().getMimeTypeFromExtension(name.substring(dot + 1).toLowerCase());
            if (type != null)
                return type;
        }
        return "application/octet-stream";
    }

    @Override
    public Cursor queryRoots(String[] projection) {
        MatrixCursor cursor = new MatrixCursor(projection != null ? projection : DEFAULT_ROOT_PROJECTION);
        cursor.newRow()
                .add(Root.COLUMN_ROOT_ID, ROOT_ID)
                .add(Root.COLUMN_DOCUMENT_ID, ROOT_ID)
                .add(Root.COLUMN_TITLE, "RSDKv5")
                .add(Root.COLUMN_FLAGS, Root.FLAG_SUPPORTS_CREATE | Root.FLAG_LOCAL_ONLY | Root.FLAG_SUPPORTS_IS_CHILD)
                .add(Root.COLUMN_ICON, R.mipmap.ic_launcher);
        return cursor;
    }

    @Override
    public Cursor queryDocument(String documentId, String[] projection) throws FileNotFoundException {
        MatrixCursor cursor = new MatrixCursor(projection != null ? projection : DEFAULT_DOCUMENT_PROJECTION);
        File file = fileFor(documentId);
        if (!file.exists())
            throw new FileNotFoundException(documentId);
        addRow(cursor, file);
        return cursor;
    }

    @Override
    public Cursor queryChildDocuments(String parentDocumentId, String[] projection, String sortOrder)
            throws FileNotFoundException {
        MatrixCursor cursor = new MatrixCursor(projection != null ? projection : DEFAULT_DOCUMENT_PROJECTION);
        File[] children = fileFor(parentDocumentId).listFiles();
        if (children != null) {
            for (File child : children)
                addRow(cursor, child);
        }
        return cursor;
    }

    @Override
    public ParcelFileDescriptor openDocument(String documentId, String mode, @Nullable CancellationSignal signal)
            throws FileNotFoundException {
        File file = fileFor(documentId);
        File parent = file.getParentFile();
        if (!mode.equals("r") && parent != null && !parent.exists())
            //noinspection ResultOfMethodCallIgnored
            parent.mkdirs();
        return ParcelFileDescriptor.open(file, ParcelFileDescriptor.parseMode(mode));
    }

    @Override
    public String createDocument(String parentDocumentId, String mimeType, String displayName)
            throws FileNotFoundException {
        File file = new File(fileFor(parentDocumentId), displayName);
        try {
            if (Document.MIME_TYPE_DIR.equals(mimeType)) {
                if (!file.isDirectory() && !file.mkdirs())
                    throw new FileNotFoundException("Could not create " + displayName);
            } else if (!file.exists() && !file.createNewFile()) {
                throw new FileNotFoundException("Could not create " + displayName);
            }
        } catch (IOException e) {
            throw new FileNotFoundException("Could not create " + displayName + ": " + e.getMessage());
        }
        return idFor(file);
    }

    @Override
    public void deleteDocument(String documentId) throws FileNotFoundException {
        File file = fileFor(documentId);
        if (file.exists() && !deleteRecursively(file))
            throw new FileNotFoundException("Could not delete " + documentId);
    }

    @Override
    public String renameDocument(String documentId, String displayName) throws FileNotFoundException {
        File file = fileFor(documentId);
        File renamed = new File(file.getParentFile(), displayName);
        if (!file.renameTo(renamed))
            throw new FileNotFoundException("Could not rename " + documentId);
        return idFor(renamed);
    }

    @Override
    public boolean isChildDocument(String parentDocumentId, String documentId) {
        return documentId.startsWith(parentDocumentId + "/");
    }

    /** Deletes @p file, and everything in it when it is a folder. */
    private static boolean deleteRecursively(File file) {
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children)
                deleteRecursively(child);
        }
        return file.delete();
    }
}
