package org.ziranlang.zx;

import android.app.AlertDialog;
import android.app.NativeActivity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.inputmethod.InputMethodManager;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

/**
 * Hosts ZX Runner's native side. The .zx to run is resolved before the
 * native activity starts, so the runner's first instruction already knows
 * its program; the native glue reads it through launchPath().
 */
public class RunnerActivity extends NativeActivity {
    private static final String TAG = "ZXRunner";
    public static final String EXTRA_PATH = "org.ziranlang.zx.PATH";

    private static String launch = "";

    /** The file the native runner starts with; read through JNI. */
    public static String launchPath() {
        return launch;
    }

    @Override
    protected void onCreate(Bundle state) {
        launch = resolve(getIntent());
        Log.i(TAG, "launch path: " + launch);
        super.onCreate(state);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        String path = resolve(intent);
        if (!path.isEmpty() && !path.equals(launch)) {
            // The native runner owns its window for one program; start a
            // fresh activity for a different file.
            Intent again = new Intent(this, RunnerActivity.class);
            again.putExtra(EXTRA_PATH, path);
            again.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
            startActivity(again);
            finish();
        }
    }

    /** A path from the launcher, or a content:// or file:// program URI. */
    private String resolve(Intent intent) {
        if (intent == null) return "";
        String direct = intent.getStringExtra(EXTRA_PATH);
        if (direct != null && !direct.isEmpty()) return direct;
        Uri uri = intent.getData();
        if (uri == null) {
            Object extra = intent.getParcelableExtra(Intent.EXTRA_STREAM);
            if (extra instanceof Uri) uri = (Uri) extra;
        }
        return copy(uri);
    }

    /** Content URIs are copied into the cache; a file path is used directly. */
    private String copy(Uri uri) {
        if (uri == null) return "";
        try {
            if ("file".equals(uri.getScheme())) {
                String path = uri.getPath();
                return path == null ? "" : path;
            }
            File target = new File(getCacheDir(), "opened.zx");
            try (InputStream in = getContentResolver().openInputStream(uri);
                 FileOutputStream out = new FileOutputStream(target)) {
                if (in == null) return "";
                byte[] buffer = new byte[1 << 16];
                int count;
                while ((count = in.read(buffer)) > 0) out.write(buffer, 0, count);
            }
            return target.getAbsolutePath();
        } catch (Exception error) {
            Log.e(TAG, "cannot open " + uri, error);
            return "";
        }
    }

    /** The native runner returned; close this activity, not just its window. */
    public void runnerFinished() {
        runOnUiThread(new Runnable() {
            @Override public void run() { finish(); }
        });
    }

    /** End the runner process after an error; the launcher has its own. */
    private void closeAfterError() {
        finish();
        System.exit(0);
    }

    /** A failed run with no terminal: show its output, then close. */
    public void showError(final String message) {
        runOnUiThread(new Runnable() {
            @Override public void run() {
                DialogInterface.OnClickListener close =
                    new DialogInterface.OnClickListener() {
                        @Override public void onClick(DialogInterface dialog, int which) {
                            closeAfterError();
                        }
                    };
                new AlertDialog.Builder(RunnerActivity.this)
                    .setTitle("ZX Runner")
                    .setMessage(message)
                    .setPositiveButton("OK", close)
                    .setOnCancelListener(new DialogInterface.OnCancelListener() {
                        @Override public void onCancel(DialogInterface dialog) { closeAfterError(); }
                    })
                    .show();
            }
        });
    }

    public void clipboardWrite(final String text) {
        runOnUiThread(new Runnable() {
            @Override public void run() {
                ClipboardManager manager =
                    (ClipboardManager)getSystemService(Context.CLIPBOARD_SERVICE);
                manager.setPrimaryClip(ClipData.newPlainText("ZX Runner", text));
            }
        });
    }

    public String clipboardRead() {
        ClipboardManager manager =
            (ClipboardManager)getSystemService(Context.CLIPBOARD_SERVICE);
        if (manager == null || !manager.hasPrimaryClip()) return "";
        ClipData clip = manager.getPrimaryClip();
        if (clip == null || clip.getItemCount() == 0) return "";
        CharSequence text = clip.getItemAt(0).coerceToText(this);
        return text == null ? "" : text.toString();
    }

    public void textInput(final boolean show) {
        runOnUiThread(new Runnable() {
            @Override public void run() {
                InputMethodManager manager =
                    (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
                if (manager == null) return;
                View view = getWindow().getDecorView();
                if (show) manager.showSoftInput(view, 0);
                else manager.hideSoftInputFromWindow(view.getWindowToken(), 0);
            }
        });
    }
}
