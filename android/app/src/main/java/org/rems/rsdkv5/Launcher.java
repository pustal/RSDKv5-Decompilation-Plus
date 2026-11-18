package org.rems.rsdkv5;

import android.Manifest;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.UriPermission;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.CountDownTimer;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.util.Log;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.documentfile.provider.DocumentFile;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.concurrent.TimeUnit;

public class Launcher extends AppCompatActivity {

    private static final int RSDK_VER = 5;
    private static Uri basePath = null;

    public static Launcher instance = null;

    private static File basePathStore;

    private static ActivityResultLauncher<Intent> folderLauncher = null;
    private static ActivityResultLauncher<Intent> gameLauncher = null;

    private static int takeFlags = (Intent.FLAG_GRANT_WRITE_URI_PERMISSION |
            Intent.FLAG_GRANT_READ_URI_PERMISSION);

    // (Plus) The transfer page (Transfer.java).

    /** The transfer page's line in the "Game starting" dialog ("" for none). */
    private static String transferLine = "";

    /** Asks for the access the transfer page needs to a picked folder ("All files access", or the storage permission). */
    private ActivityResultLauncher<Intent> allFilesLauncher = null;
    private ActivityResultLauncher<String> storagePermissionLauncher = null;

    /** Where the launcher remembers that it asked for that access (it asks once). */
    private static final String PREFERENCES = "launcher";
    private static final String ASKED_FILE_ACCESS = "askedFileAccess";

    /** How often the "Add the game" dialog looks for the game's files, in milliseconds. */
    private static final long SETUP_POLL_MS = 1000;

    /** Runs the "Add the game" dialog's checks on the main thread. */
    private final Handler handler = new Handler(Looper.getMainLooper());

    /** The "Add the game" dialog while it shows, and its periodic check. */
    private AlertDialog setupDialog = null;
    private Runnable setupPoll = null;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        basePathStore = new File(getFilesDir(), "basePathStore");

        folderLauncher = registerForActivityResult(
                new ActivityResultContracts.StartActivityForResult(),
                result -> {
                    if (result.getResultCode() == Activity.RESULT_OK && result.getData() != null) {
                        basePath = result.getData().getData();
                    }
                    // (Plus) Nothing picked and no folder before: ask again
                    // instead of starting without a folder.
                    if (basePath == null) {
                        startGame(false);
                        return;
                    }
                    try {
                        Log.i("hi", String.format("%d", getContentResolver().openInputStream(
                                DocumentFile.fromTreeUri(this, basePath).findFile("Settings.ini").getUri()).read()));
                    } catch (FileNotFoundException e) {
                        e.printStackTrace();
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                    startGame(true);
                });

        gameLauncher = registerForActivityResult(
                new ActivityResultContracts.StartActivityForResult(),
                result -> {
                    quit(0);
                });

        // (Plus) Back from Android's file access screens: look at the folder again.
        allFilesLauncher = registerForActivityResult(
                new ActivityResultContracts.StartActivityForResult(),
                result -> startGame(true));
        storagePermissionLauncher = registerForActivityResult(
                new ActivityResultContracts.RequestPermission(),
                granted -> startGame(true));

        boolean canRun = true;

        if (RSDK_VER == 5) {
            if (((ActivityManager) getSystemService(Context.ACTIVITY_SERVICE))
                    .getDeviceConfigurationInfo().reqGlEsVersion < 0x20000) {
                canRun = false;
                new AlertDialog.Builder(this)
                        .setTitle("GLES 2.0 unsupported")
                        .setMessage("This device does not support GLES 2.0, which is required for running RSDKv5.")
                        .setNegativeButton("OK", (dialog, i) -> {
                            dialog.cancel();
                            quit(2);
                        })
                        .setCancelable(false)
                        .show();
            }
        }

        if (canRun)
            startGame(false);
    }

    private void quit(int code) {
        finishAffinity();
        System.exit(code);
    }

    static class DialogTimer extends CountDownTimer {
        public AlertDialog alert;

        public DialogTimer(long millisInFuture, long countDownInterval) {
            super(millisInFuture, countDownInterval);
        }

        @Override
        public void onTick(long l) {
            alert.setMessage(String.format(
                    "Game will start in %s in %d seconds...",
                    describe(alert.getContext(), basePath),
                    TimeUnit.MILLISECONDS.toSeconds(l) + 1) + transferLine);
        }

        @Override
        public void onFinish() {
            alert.getButton(AlertDialog.BUTTON_POSITIVE).callOnClick();
        }
    }

    public static Uri refreshStore() {
        if (basePathStore.exists() && basePath == null) {
            try {
                BufferedReader reader = new BufferedReader(new FileReader(basePathStore));
                String uri = reader.readLine();
                if (uri != null) {
                    basePath = Uri.parse(uri);
                }
                reader.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }

        if (basePath != null) {
            try {
                FileWriter writer = new FileWriter(basePathStore);
                writer.write(basePath.toString() + "\n");
                writer.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
        return basePath;
    }

    private void startGame(boolean fromPicker) {

        refreshStore();

        // (Plus) On a TV, the app's own folder until another is chosen: the
        // game's files arrive through the transfer page, and many TVs have no
        // folder picker. Phones and tablets are asked as before.
        if (basePath == null && isTelevision()) {
            basePath = GameFolderProvider.treeUri();
            refreshStore();
        }

        // (Plus) The app's own folder needs no permission from the system.
        boolean found = isAppFolder(basePath);
        if (basePath != null && !found) {
            for (UriPermission uriPermission : getContentResolver().getPersistedUriPermissions()) {
                if (uriPermission.getUri().toString().matches(basePath.toString())) {
                    found = true;
                    break;
                }
            }
        }

        if (!found && !fromPicker) {
            new AlertDialog.Builder(this)
                    .setTitle("Path confirmation")
                    .setMessage(basePath != null ? "Please reconfirm the path the game should run in."
                            : "Please set the path the game should run in.")
                    .setPositiveButton("OK", (dialog, i) -> {
                        folderPicker();
                    })
                    .setNegativeButton("Cancel", (dialog, i) -> {
                        dialog.cancel();
                        quit(3);
                    })
                    // (Plus) For devices without a folder picker (many
                    // Android TVs), or to keep the game in the app's folder.
                    .setNeutralButton("App folder", (dialog, i) -> useAppFolder(false))
                    .setCancelable(false)
                    .show();
        } else {
            // (Plus) The transfer page on this folder; it may first ask for
            // file access, or wait for the game's files (then this runs again).
            if (!prepareTransfer())
                return;

            AlertDialog baseAlert = null;

            DialogTimer timer = new DialogTimer(5000, 100);

            baseAlert = new AlertDialog.Builder(this)
                    .setTitle("Game starting")
                    .setMessage("Game will start in...")
                    .setPositiveButton("Start", (dialog, i) -> {
                        timer.cancel();
                        // String p = Environment.getExternalStorageDirectory().getAbsolutePath() + "/"
                        // + basePath;
                        try {
                            if (DocumentFile.fromTreeUri(this, basePath).findFile(".nomedia") == null)
                                createFile(".nomedia");
                        } catch (Exception e) {
                        }

                        // (Plus) What the transfer page staged last time, before the game reads it.
                        Transfer.nativeApplyPendingFiles();

                        Intent intent = new Intent(this, RSDK.class);
                        intent.setData(basePath);
                        // (Plus) The app's own folder is the app's: there is
                        // no permission to grant or keep for it.
                        if (!isAppFolder(basePath)) {
                            intent.setFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION |
                                    Intent.FLAG_GRANT_READ_URI_PERMISSION |
                                    Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
                            grantUriPermission(getApplicationContext().getPackageName() + ".RSDK", basePath,
                                    Intent.FLAG_GRANT_WRITE_URI_PERMISSION |
                                            Intent.FLAG_GRANT_READ_URI_PERMISSION |
                                            Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);

                            getContentResolver().takePersistableUriPermission(basePath, takeFlags);
                        }

                        instance = this;

                        gameLauncher.launch(intent);
                    })
                    .setNeutralButton("Change Path", (dialog, i) -> {
                        timer.cancel();
                        changeFolder();
                    })
                    .create();

            timer.alert = baseAlert;
            baseAlert.setOnShowListener(dialog -> timer.start());

            baseAlert.show();
        }
    }

    private void folderPicker() {
        refreshStore();
        Intent picker = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
                .addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION |
                        Intent.FLAG_GRANT_READ_URI_PERMISSION |
                        Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        if (basePath != null && !isAppFolder(basePath))
            picker.putExtra(DocumentsContract.EXTRA_INITIAL_URI, basePath);
        // (Plus) Many Android TVs have no folder picker: use the app's folder.
        try {
            folderLauncher.launch(picker);
        } catch (ActivityNotFoundException e) {
            useAppFolder(true);
        }
    }

    /** (Plus) Whether @p uri is the app's own folder (GameFolderProvider). */
    private static boolean isAppFolder(@Nullable Uri uri) {
        return uri != null && GameFolderProvider.AUTHORITY.equals(uri.getAuthority());
    }

    /** (Plus) A readable name of the folder @p uri, for the dialogs. */
    private static String describe(Context context, Uri uri) {
        if (isAppFolder(uri))
            return GameFolderProvider.folder(context).getAbsolutePath();
        return uri.getPath();
    }

    /**
     * (Plus) Runs the game in the app's own folder (GameFolderProvider), first
     * saying where it is and how to put the game's files there.
     *
     * @param noPicker true when this device has no folder picker.
     */
    private void useAppFolder(boolean noPicker) {
        String folder = GameFolderProvider.folder(this).getAbsolutePath();
        String message = (noPicker ? "This device has no folder picker, so the game runs in its own folder:\n\n"
                : "The game runs in its own folder:\n\n")
                + folder + "\n\n"
                + "Upload Data.rsdk (and the retro packs, mods...) from a phone or computer with the transfer "
                + "page shown next, or copy them there yourself, for example with "
                + "\"adb push Data.rsdk " + folder + "/\" from a computer. Android removes this folder "
                + "with the app.";
        new AlertDialog.Builder(this)
                .setTitle("Game folder")
                .setMessage(message)
                .setPositiveButton("OK", (dialog, i) -> {
                    basePath = GameFolderProvider.treeUri();
                    refreshStore();
                    startGame(true);
                })
                .setNegativeButton("Cancel", (dialog, i) -> {
                    dialog.cancel();
                    quit(3);
                })
                .setCancelable(false)
                .show();
    }

    /** (Plus) Picks another folder for the game (the picker, or the app's folder without one). */
    private void changeFolder() {
        if (basePath != null && !isAppFolder(basePath)) {
            try {
                getContentResolver().releasePersistableUriPermission(basePath, takeFlags);
            } catch (SecurityException e) {
                // Not held (the folder was only just picked): nothing to release.
            }
        }
        folderPicker();
    }

    /** (Plus) Whether this device is a TV (Android TV, Google TV). */
    private boolean isTelevision() {
        return getPackageManager().hasSystemFeature(PackageManager.FEATURE_LEANBACK);
    }

    /**
     * (Plus) Starts the transfer page on the game's folder, and sets the line
     * the "Game starting" dialog shows about it.
     *
     * @return false when it showed a dialog instead (asking for file access,
     *         or "Add the game" while the folder has nothing to play), which
     *         calls startGame() again when it is done.
     */
    private boolean prepareTransfer() {
        final String path = Transfer.folderPath(this, basePath);
        if (!Transfer.canUse(path)) {
            Transfer.nativeStop();
            if (path != null && canAskForFileAccess() && !preferences().getBoolean(ASKED_FILE_ACCESS, false)) {
                askForFileAccess();
                return false;
            }
            transferLine = path == null ? ""
                    : "\n\nThe transfer page needs access to this folder (\"All files access\" for the app in "
                      + "Android's settings), or the App folder (Change Path).";
            return true;
        }

        Transfer.nativeSetLanAddress(Transfer.lanAddress());
        if (!Transfer.nativeStart(path)) {
            transferLine = "\n\nThe transfer page could not start.";
            return true;
        }
        if (!Transfer.nativeHasGameData()) {
            showSetupDialog();
            return false;
        }

        final String page = Transfer.nativePageAddress();
        transferLine = page.isEmpty() ? "\n\nConnect to a network to use the transfer page."
                : "\n\nTransfer page: " + page + "  PIN " + Transfer.nativePin();
        return true;
    }

    /** (Plus) The launcher's own small settings. */
    private SharedPreferences preferences() {
        return getSharedPreferences(PREFERENCES, MODE_PRIVATE);
    }

    /** (Plus) Whether Android lets the app ask for direct access to the device's storage (it does not have it yet). */
    private boolean canAskForFileAccess() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
            return !Environment.isExternalStorageManager();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
            return checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED;
        return false;
    }

    /**
     * (Plus) Once: explains why the transfer page needs direct access to the
     * picked folder, and opens Android's screen for it. The game works without
     * it (the files are then copied by hand, or the App folder is used).
     */
    private void askForFileAccess() {
        preferences().edit().putBoolean(ASKED_FILE_ACCESS, true).apply();
        new AlertDialog.Builder(this)
                .setTitle("Transfer page")
                .setMessage("The transfer page lets a phone or computer on the same network upload the game's "
                        + "files (Data.rsdk, the retro packs, mods) and back up the saves. To write them into "
                        + "the folder you picked, the app needs access to it (\"All files access\").\n\n"
                        + "Without it the game works as before: copy the files yourself, or use the App folder.")
                .setPositiveButton("Allow", (dialog, i) -> {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                        Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                                Uri.parse("package:" + getPackageName()));
                        try {
                            allFilesLauncher.launch(intent);
                        } catch (ActivityNotFoundException e) {
                            allFilesLauncher.launch(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
                        }
                    } else {
                        storagePermissionLauncher.launch(Manifest.permission.WRITE_EXTERNAL_STORAGE);
                    }
                })
                .setNegativeButton("Not now", (dialog, i) -> startGame(true))
                .setCancelable(false)
                .show();
    }

    /** (Plus) The "Add the game" dialog's text: the transfer page's address and PIN, or how to get it. */
    private String setupMessage() {
        final String folder = describe(this, basePath);
        final String page = Transfer.nativePageAddress();
        final String reach = page.isEmpty()
                ? "Connect this device to a network to upload them from a phone or computer with the transfer page."
                : "On a phone or computer on the same network, open\n\n" + page + "\n\nenter the PIN "
                  + Transfer.nativePin() + " and upload them there. The game starts when they arrive.";
        return "Sonic Mania's Data.rsdk (or the retro packs) are not in the game's folder yet.\n\n" + reach
                + "\n\nOr copy them into " + folder + " yourself.";
    }

    /**
     * (Plus) Shown while the game's folder has nothing to play: the transfer
     * page's address and PIN, updated every second, until the files arrive
     * (then the game starts as usual).
     */
    private void showSetupDialog() {
        stopSetupPolling();
        setupDialog = new AlertDialog.Builder(this)
                .setTitle("Add the game")
                .setMessage(setupMessage())
                .setNeutralButton("Change Path", (dialog, i) -> {
                    stopSetupPolling();
                    changeFolder();
                })
                .setNegativeButton("Exit", (dialog, i) -> {
                    stopSetupPolling();
                    quit(0);
                })
                .setCancelable(false)
                .create();
        setupDialog.show();

        setupPoll = new Runnable() {
            @Override
            public void run() {
                if (setupDialog == null)
                    return;
                Transfer.nativeSetLanAddress(Transfer.lanAddress()); // The network may have changed.
                if (Transfer.nativeHasGameData()) {
                    stopSetupPolling();
                    startGame(true);
                    return;
                }
                setupDialog.setMessage(setupMessage());
                handler.postDelayed(this, SETUP_POLL_MS);
            }
        };
        handler.postDelayed(setupPoll, SETUP_POLL_MS);
    }

    /** (Plus) Closes the "Add the game" dialog and stops its checks. */
    private void stopSetupPolling() {
        if (setupPoll != null)
            handler.removeCallbacks(setupPoll);
        setupPoll = null;
        if (setupDialog != null && setupDialog.isShowing())
            setupDialog.dismiss();
        setupDialog = null;
    }

    public Uri createFile(String filename) throws FileNotFoundException {

        DocumentFile path = DocumentFile.fromTreeUri(getApplicationContext(), basePath);
        while (filename.indexOf('/') != -1) {
            String sub = filename.substring(0, filename.indexOf('/'));
            if (!sub.isEmpty()) {
                DocumentFile find = path.findFile(sub);
                if (find == null)
                    path = path.createDirectory(sub);
                else
                    path = find;    
            }
            filename = filename.substring(filename.indexOf('/') + 1);
        }

        DocumentFile find = path.findFile(filename);
        if (find == null)
            return path.createFile("application/octet-stream", filename).getUri();
        else
            return find.getUri();
    }

}
