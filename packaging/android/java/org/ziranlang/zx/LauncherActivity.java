package org.ziranlang.zx;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * The launcher: pick a .zx program or start one from the recent list.
 * Picked programs are copied into the app's private storage so a provider
 * grant does not expire before the runner reads them.
 */
public class LauncherActivity extends Activity {
    private static final String TAG = "ZXRunner";
    private static final int PICK = 1;
    private static final int RECENTS = 10;

    private final List<String> recentPaths = new ArrayList<>();
    private ArrayAdapter<String> adapter;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        loadRecents();
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        int padding = (int)(16 * getResources().getDisplayMetrics().density);
        page.setPadding(padding, padding, padding, padding);

        TextView heading = new TextView(this);
        heading.setText("ZX Runner");
        heading.setTextSize(24);
        page.addView(heading);

        Button open = new Button(this);
        open.setText("Open a .zx program");
        open.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View view) { pick(); }
        });
        page.addView(open, new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        TextView recent = new TextView(this);
        recent.setText("Recent");
        recent.setTextSize(18);
        page.addView(recent);

        adapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, new ArrayList<String>());
        ListView list = new ListView(this);
        list.setAdapter(adapter);
        list.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                run(recentPaths.get(position));
            }
        });
        list.setOnItemLongClickListener(new AdapterView.OnItemLongClickListener() {
            @Override public boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id) {
                forget(position);
                return true;
            }
        });
        page.addView(list, new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, 0, 1.0f));
        setContentView(page);
        showRecents();
        if (recentPaths.isEmpty()) pick();
    }

    private void pick() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_MIME_TYPES,
            new String[] {"application/x-ziran-bundle", "application/octet-stream"});
        startActivityForResult(intent, PICK);
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != PICK) return;
        if (result != RESULT_OK || data == null || data.getData() == null) {
            if (recentPaths.isEmpty()) finish();
            return;
        }
        String path = copy(data.getData());
        if (!path.isEmpty()) {
            remember(path);
            run(path);
        }
    }

    private String copy(Uri uri) {
        try {
            String name = uri.getLastPathSegment();
            if (name == null) name = "program.zx";
            name = new File(name).getName();
            File directory = new File(getFilesDir(), "programs");
            if (!directory.exists() && !directory.mkdirs()) return "";
            File target = new File(directory, name);
            try (InputStream in = getContentResolver().openInputStream(uri);
                 FileOutputStream out = new FileOutputStream(target)) {
                if (in == null) return "";
                byte[] buffer = new byte[1 << 16];
                int count;
                while ((count = in.read(buffer)) > 0) out.write(buffer, 0, count);
            }
            return target.getAbsolutePath();
        } catch (Exception error) {
            Log.e(TAG, "cannot copy " + uri, error);
            return "";
        }
    }

    private void run(String path) {
        Intent intent = new Intent(this, RunnerActivity.class);
        intent.putExtra(RunnerActivity.EXTRA_PATH, path);
        startActivity(intent);
    }

    private void loadRecents() {
        String saved = getSharedPreferences("recents", 0).getString("paths", "");
        if (saved == null) return;
        for (String path : saved.split("\n")) {
            if (!path.isEmpty() && new File(path).isFile()) recentPaths.add(path);
        }
    }

    private void saveRecents() {
        StringBuilder joined = new StringBuilder();
        for (String path : recentPaths) {
            if (joined.length() > 0) joined.append('\n');
            joined.append(path);
        }
        getSharedPreferences("recents", 0).edit().putString("paths", joined.toString()).apply();
    }

    private void remember(String path) {
        recentPaths.remove(path);
        recentPaths.add(0, path);
        while (recentPaths.size() > RECENTS) recentPaths.remove(recentPaths.size() - 1);
        saveRecents();
        showRecents();
    }

    private void forget(int position) {
        if (position < 0 || position >= recentPaths.size()) return;
        new File(recentPaths.get(position)).delete();
        recentPaths.remove(position);
        saveRecents();
        showRecents();
    }

    private void showRecents() {
        adapter.clear();
        for (String path : recentPaths) adapter.add(new File(path).getName());
        adapter.notifyDataSetChanged();
    }
}
