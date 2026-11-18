package org.rems.rsdkv5;

import android.content.Context;
import android.net.Uri;
import android.os.Environment;
import android.provider.DocumentsContract;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.io.File;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.util.Collections;
import java.util.Enumeration;

/**
 * (Plus) The transfer page: a small web server, started by the launcher, that
 * lets a phone or computer on the same network upload the game's files (Sonic
 * Mania's Data.rsdk, the retro packs, Sonic Nexus, Sonic Origins' files, mods) and back up
 * or restore the saves, as SonicWebMania does on webOS TVs. The server is
 * native (dependencies/android/transfer, glue in androidTransfer.cpp) and
 * keeps running while the game plays, in the same process.
 *
 * It works on the game's folder by its file path, so the folder must be one
 * the app can reach directly: the app's own folder always is
 * (GameFolderProvider); a folder picked with Android's folder picker is when
 * it is on the device's storage and the app has "All files access" (or, up to
 * Android 10, the storage permission). See {@link #folderPath}.
 */
public final class Transfer {

    static {
        // The engine's library, which GameActivity loads again later (the same one, once per process).
        System.loadLibrary("RetroEngine");
    }

    private Transfer() {}

    /** Records the device's address on the local network ("" when offline), for the page's address. */
    public static native void nativeSetLanAddress(String address);

    /** Starts the server on the folder @p root (an absolute path). @return Whether it serves. */
    public static native boolean nativeStart(String root);

    /** Stops the server. */
    public static native void nativeStop();

    /** "http://address:port", or "" when the server is off or the device is offline. */
    public static native String nativePageAddress();

    /** The session's PIN, or "" when the server is off. */
    public static native String nativePin();

    /** Whether the folder holds something to play (Sonic Mania's Data.rsdk, a playable retro pack or a fan game). */
    public static native boolean nativeHasGameData();

    /** Moves the files the page staged during the last session into place. Before the game starts. */
    public static native int nativeApplyPendingFiles();

    /** The authority of Android's own documents provider for the device's storage (its folder picker's folders). */
    private static final String EXTERNAL_STORAGE_AUTHORITY = "com.android.externalstorage.documents";

    /**
     * The file path of the game's folder @p tree (a tree URI), or null when it
     * has none: the app's own folder, or a folder of the device's storage
     * picked with the folder picker ("primary:RSDK" is the internal storage's
     * RSDK folder, "1234-5678:RSDK" a memory card's). Whether the app may use
     * that path is {@link #canUse}'s question.
     */
    @Nullable
    public static String folderPath(@NonNull Context context, @Nullable Uri tree) {
        if (tree == null)
            return null;
        if (GameFolderProvider.AUTHORITY.equals(tree.getAuthority()))
            return GameFolderProvider.folder(context).getAbsolutePath();
        if (!EXTERNAL_STORAGE_AUTHORITY.equals(tree.getAuthority()))
            return null;

        final String documentId;
        try {
            documentId = DocumentsContract.getTreeDocumentId(tree);
        } catch (IllegalArgumentException e) {
            return null;
        }
        final int colon = documentId.indexOf(':');
        if (colon < 0)
            return null;
        final String volume   = documentId.substring(0, colon);
        final String relative = documentId.substring(colon + 1);
        final File base = "primary".equalsIgnoreCase(volume) ? Environment.getExternalStorageDirectory()
                                                             : new File("/storage/" + volume);
        return (relative.isEmpty() ? base : new File(base, relative)).getAbsolutePath();
    }

    /** Whether the app may read and write the folder at @p path directly (see the class notes). */
    public static boolean canUse(@Nullable String path) {
        if (path == null)
            return false;
        final File folder = new File(path);
        return folder.isDirectory() && folder.canRead() && folder.canWrite();
    }

    /**
     * The device's IPv4 address on the local network, or "" when it has none:
     * wired first, then Wi-Fi, then any other interface that is up (not the
     * loopback, not a mobile network's carrier-grade address).
     */
    @NonNull
    public static String lanAddress() {
        String best = "";
        int bestRank = Integer.MAX_VALUE;
        try {
            final Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces();
            if (interfaces == null)
                return best;
            for (NetworkInterface networkInterface : Collections.list(interfaces)) {
                if (!networkInterface.isUp() || networkInterface.isLoopback())
                    continue;
                final String name = networkInterface.getName();
                final int rank = name.startsWith("eth") ? 0 : name.startsWith("wlan") ? 1 : 2;
                if (rank >= bestRank)
                    continue;
                for (InetAddress address : Collections.list(networkInterface.getInetAddresses())) {
                    // Only private ranges: the page answers devices on the local network alone.
                    if (address instanceof Inet4Address && address.isSiteLocalAddress()) {
                        best     = address.getHostAddress();
                        bestRank = rank;
                        break;
                    }
                }
            }
        } catch (SocketException e) {
            return best;
        }
        return best;
    }
}
