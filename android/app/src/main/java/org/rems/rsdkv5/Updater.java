package org.rems.rsdkv5;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageInstaller;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;

/**
 * (Plus) "Check for updates" (the Plus menu): looks for a newer release of the
 * app on GitHub, and installs it when asked.
 *
 * <ol>
 * <li>The latest release of {@link #RELEASES_REPOSITORY} comes from GitHub's
 * API; its tag is the version ("1.10.0", or "v1.10.0"), compared with the
 * installed one.</li>
 * <li>When it is newer, a dialog offers it. Its Android asset (the .apk whose
 * name holds "Android") is downloaded into Android's installer
 * (PackageInstaller), with the progress shown.</li>
 * <li>Android then asks the player to confirm, as it always does, and the
 * first time to allow this app to install apps. The new version must be
 * signed with the same key as the installed one, or Android refuses it.</li>
 * </ol>
 *
 * Everything shows as Android dialogs over the game; the network work runs on
 * its own thread.
 */
final class Updater {

    /** Where the releases are published (owner/repository on GitHub). */
    static final String RELEASES_REPOSITORY = "pustal/Sonic-Mania-Decompilation-Plus";

    private static final String LATEST_RELEASE_URL = "https://api.github.com/repos/" + RELEASES_REPOSITORY + "/releases/latest";
    private static final String TAG = "RSDKv5-Update";
    /** The broadcast PackageInstaller sends this app with an installation's progress. */
    private static final String INSTALL_STATUS_ACTION = "org.rems.rsdkv5.INSTALL_STATUS";
    private static final int TIMEOUT_MS = 15000;

    private final Activity activity;
    private final Handler main = new Handler(Looper.getMainLooper());

    private Updater(@NonNull Activity activity) {
        this.activity = activity;
    }

    /** Checks for a newer release, showing what it finds. Any thread. */
    static void check(@NonNull Activity activity) {
        final Updater updater = new Updater(activity);
        activity.runOnUiThread(updater::start);
    }

    /** A release as GitHub describes it, reduced to what the update needs. */
    private static final class Release {
        String version = "";      ///< The tag without a leading "v".
        String apkName = "";      ///< The Android asset's file name ("" when the release has none).
        String apkUrl  = "";      ///< Its download address.
        long apkSize   = 0;       ///< Its size in bytes (0 when unknown).
    }

    private void start() {
        final AlertDialog checking = new AlertDialog.Builder(activity)
                .setTitle("Check for updates")
                .setMessage("Looking for a newer version on GitHub...")
                .setCancelable(true)
                .create();
        checking.show();

        new Thread(() -> {
            String error = null;
            Release release = null;
            try {
                release = fetchLatestRelease();
            } catch (IOException | JSONException e) {
                Log.w(TAG, "Could not check for updates", e);
                error = e.getMessage();
            }
            final Release found = release;
            final String failure = error;
            main.post(() -> {
                if (!checking.isShowing())
                    return; // Cancelled.
                checking.dismiss();
                if (found == null)
                    tell("Check for updates", "Could not reach GitHub" + (failure != null ? " (" + failure + ")" : "")
                            + ". Check the network connection and try again.");
                else
                    offer(found);
            });
        }, "UpdateCheck").start();
    }

    /** The latest release, from GitHub's API. Network thread. */
    @NonNull
    private static Release fetchLatestRelease() throws IOException, JSONException {
        final HttpURLConnection connection = (HttpURLConnection) new URL(LATEST_RELEASE_URL).openConnection();
        connection.setConnectTimeout(TIMEOUT_MS);
        connection.setReadTimeout(TIMEOUT_MS);
        connection.setRequestProperty("Accept", "application/vnd.github+json");
        connection.setRequestProperty("User-Agent", "RSDKv5-Plus-Android");
        try {
            final int code = connection.getResponseCode();
            if (code != HttpURLConnection.HTTP_OK)
                throw new IOException("HTTP " + code);
            final JSONObject json = new JSONObject(readAll(connection.getInputStream()));

            final Release release = new Release();
            final String tag = json.optString("tag_name", "");
            release.version = tag.startsWith("v") || tag.startsWith("V") ? tag.substring(1) : tag;
            final JSONArray assets = json.optJSONArray("assets");
            if (assets != null) {
                for (int i = 0; i < assets.length(); ++i) {
                    final JSONObject asset = assets.getJSONObject(i);
                    final String name = asset.optString("name", "");
                    if (name.toLowerCase().endsWith(".apk") && name.toLowerCase().contains("android")) {
                        release.apkName = name;
                        release.apkUrl  = asset.optString("browser_download_url", "");
                        release.apkSize = asset.optLong("size", 0);
                        break;
                    }
                }
            }
            return release;
        } finally {
            connection.disconnect();
        }
    }

    /** What the release means for the installed version: nothing new, or an offer. UI thread. */
    private void offer(@NonNull Release release) {
        final String installed = installedVersion();
        if (release.version.isEmpty() || compareVersions(release.version, installed) <= 0) {
            tell("Check for updates", "You have the latest version (" + installed + ").");
            return;
        }
        if (release.apkUrl.isEmpty()) {
            tell("Check for updates", "Version " + release.version + " is out, but its release has no Android app yet. "
                    + "You have " + installed + ".");
            return;
        }
        new AlertDialog.Builder(activity)
                .setTitle("Version " + release.version + " is available")
                .setMessage("You have " + installed + ". Download " + release.apkName
                        + (release.apkSize > 0 ? " (" + release.apkSize / (1024 * 1024) + " MB)" : "")
                        + " and install it? Android asks you to confirm the installation; your saves and settings stay.")
                .setPositiveButton("Update", (dialog, which) -> beginInstall(release))
                .setNegativeButton("Later", null)
                .show();
    }

    /**
     * Installs @p release, once Android lets this app install apps (from
     * Android 8 the player allows it, once, in Android's settings). UI thread.
     */
    private void beginInstall(@NonNull Release release) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && !activity.getPackageManager().canRequestPackageInstalls()) {
            new AlertDialog.Builder(activity)
                    .setTitle("Allow updates")
                    .setMessage("To install the update, allow this app to install apps (\"Install unknown apps\") in the "
                            + "screen that opens, then choose Check for updates again.")
                    .setPositiveButton("Open settings", (dialog, which) -> {
                        try {
                            activity.startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                                    Uri.parse("package:" + activity.getPackageName())));
                        } catch (android.content.ActivityNotFoundException e) {
                            tell("Allow updates", "This device has no screen for that setting. Install the update by "
                                    + "hand from the project's releases on GitHub.");
                        }
                    })
                    .setNegativeButton("Cancel", null)
                    .show();
            return;
        }
        download(release);
    }

    /** Downloads the APK into an installer session, showing the progress, then commits it. UI thread. */
    private void download(@NonNull Release release) {
        final AlertDialog progress = new AlertDialog.Builder(activity)
                .setTitle("Downloading " + release.version)
                .setMessage("Starting...")
                .setCancelable(false)
                .create();
        progress.show();

        new Thread(() -> {
            final PackageInstaller installer = activity.getPackageManager().getPackageInstaller();
            int sessionId = -1;
            try {
                final PackageInstaller.SessionParams params =
                        new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
                params.setAppPackageName(activity.getPackageName());
                if (release.apkSize > 0)
                    params.setSize(release.apkSize);
                sessionId = installer.createSession(params);

                try (PackageInstaller.Session session = installer.openSession(sessionId)) {
                    copyDownload(release, session, progress);
                    registerStatusReceiver();
                    final Intent status = new Intent(INSTALL_STATUS_ACTION).setPackage(activity.getPackageName());
                    int flags = PendingIntent.FLAG_UPDATE_CURRENT;
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
                        flags |= PendingIntent.FLAG_MUTABLE; // The installer adds its status to it.
                    final PendingIntent pending = PendingIntent.getBroadcast(activity, sessionId, status, flags);
                    session.commit(pending.getIntentSender());
                }
                main.post(progress::dismiss);
            } catch (IOException | RuntimeException e) {
                Log.w(TAG, "Could not download the update", e);
                if (sessionId >= 0)
                    installer.abandonSession(sessionId);
                main.post(() -> {
                    progress.dismiss();
                    tell("Update", "The download failed (" + e.getMessage() + "). Try again later.");
                });
            }
        }, "UpdateDownload").start();
    }

    /** Streams the APK from GitHub into @p session, updating @p progress. Network thread. */
    private void copyDownload(@NonNull Release release, @NonNull PackageInstaller.Session session, @NonNull AlertDialog progress)
            throws IOException {
        final HttpURLConnection connection = (HttpURLConnection) new URL(release.apkUrl).openConnection();
        connection.setConnectTimeout(TIMEOUT_MS);
        connection.setReadTimeout(TIMEOUT_MS);
        connection.setRequestProperty("User-Agent", "RSDKv5-Plus-Android");
        connection.setInstanceFollowRedirects(true); // GitHub sends release assets from another host.
        try {
            if (connection.getResponseCode() != HttpURLConnection.HTTP_OK)
                throw new IOException("HTTP " + connection.getResponseCode());
            final long total = connection.getContentLength() > 0 ? connection.getContentLength() : release.apkSize;
            try (InputStream in = connection.getInputStream();
                 OutputStream out = session.openWrite(release.apkName, 0, total > 0 ? total : -1)) {
                final byte[] buffer = new byte[64 * 1024];
                long done = 0;
                int lastPercent = -1;
                int read;
                while ((read = in.read(buffer)) > 0) {
                    out.write(buffer, 0, read);
                    done += read;
                    final int percent = total > 0 ? (int) (done * 100 / total) : -1;
                    if (percent != lastPercent) {
                        lastPercent = percent;
                        final long mb = done / (1024 * 1024);
                        main.post(() -> progress.setMessage(percent >= 0 ? percent + "%" : mb + " MB"));
                    }
                }
                session.fsync(out);
            }
        } finally {
            connection.disconnect();
        }
    }

    /**
     * Listens for the installer's answer: it needs the player's confirmation
     * (shown at once), or it failed (said in a dialog). On success Android
     * replaces the app, which closes; nothing is left to do.
     */
    private void registerStatusReceiver() {
        final BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                final int status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS, PackageInstaller.STATUS_FAILURE);
                if (status == PackageInstaller.STATUS_PENDING_USER_ACTION) {
                    final Intent confirm = intent.getParcelableExtra(Intent.EXTRA_INTENT);
                    if (confirm != null) {
                        confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        activity.startActivity(confirm);
                    }
                    return;
                }
                try {
                    activity.unregisterReceiver(this);
                } catch (IllegalArgumentException e) {
                    // Already unregistered.
                }
                if (status != PackageInstaller.STATUS_SUCCESS) {
                    final String message = intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE);
                    tell("Update", "The update was not installed" + (message != null ? " (" + message + ")" : "") + ".");
                }
            }
        };
        main.post(() -> {
            final IntentFilter filter = new IntentFilter(INSTALL_STATUS_ACTION);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
                activity.registerReceiver(receiver, filter, Context.RECEIVER_NOT_EXPORTED);
            else
                activity.registerReceiver(receiver, filter);
        });
    }

    /** A dialog with @p message and OK. UI thread. */
    private void tell(@NonNull String title, @NonNull String message) {
        new AlertDialog.Builder(activity).setTitle(title).setMessage(message).setPositiveButton("OK", null).show();
    }

    /** The installed version (versionName, "1.10.0"). */
    @NonNull
    private String installedVersion() {
        try {
            final PackageInfo info = activity.getPackageManager().getPackageInfo(activity.getPackageName(), 0);
            return info.versionName != null ? info.versionName : "0";
        } catch (PackageManager.NameNotFoundException e) {
            return "0";
        }
    }

    /**
     * Compares two versions number by number ("1.10.0" is newer than
     * "1.9.3"); missing or non-numeric parts count as 0.
     *
     * @return Negative, 0 or positive as @p a is older than, the same as or newer than @p b.
     */
    static int compareVersions(@NonNull String a, @NonNull String b) {
        final String[] left = a.split("\\.");
        final String[] right = b.split("\\.");
        for (int i = 0; i < Math.max(left.length, right.length); ++i) {
            final int l = i < left.length ? leadingNumber(left[i]) : 0;
            final int r = i < right.length ? leadingNumber(right[i]) : 0;
            if (l != r)
                return Integer.compare(l, r);
        }
        return 0;
    }

    /** The number @p part starts with ("3-beta" is 3), or 0. */
    private static int leadingNumber(@Nullable String part) {
        if (part == null)
            return 0;
        int value = 0;
        for (int i = 0; i < part.length() && Character.isDigit(part.charAt(i)); ++i)
            value = value * 10 + (part.charAt(i) - '0');
        return value;
    }

    /** The whole of @p in as UTF-8 text. */
    @NonNull
    private static String readAll(@NonNull InputStream in) throws IOException {
        try (InputStream stream = in; ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            final byte[] buffer = new byte[16 * 1024];
            int read;
            while ((read = stream.read(buffer)) > 0)
                out.write(buffer, 0, read);
            return new String(out.toByteArray(), StandardCharsets.UTF_8);
        }
    }
}
