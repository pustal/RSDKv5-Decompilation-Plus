package org.rems.rsdkv5;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.UriPermission;
import android.net.Uri;
import android.os.Bundle;
import android.os.CountDownTimer;
import android.provider.DocumentsContract;
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
                    TimeUnit.MILLISECONDS.toSeconds(l) + 1));
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
                        if (!isAppFolder(basePath))
                            getContentResolver().releasePersistableUriPermission(basePath, takeFlags);
                        folderPicker();
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
                + "Put Data.rsdk (and any other files the game uses, such as mods) there, for example with "
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
