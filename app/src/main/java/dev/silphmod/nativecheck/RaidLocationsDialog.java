package dev.silphmod.nativecheck;

import android.app.Activity;
import android.app.Dialog;
import android.content.SharedPreferences;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.*;

import java.util.ArrayList;
import java.util.List;

final class RaidLocationsDialog {
    private final Activity activity;
    private final SharedPreferences settings;
    private final Handler main = new Handler(Looper.getMainLooper());
    private final List<RaidLocations.Location> visible = new ArrayList<>();
    private ModeUi ui;
    private Dialog dialog;
    private ListView list;
    private Adapter adapter;
    private TextView summary;
    private int selected;
    private boolean closed;
    private final Runnable clock = new Runnable() {
        public void run() {
            if (closed || activity.isFinishing()) return;
            if (dialog.getWindow() != null && dialog.getWindow().getDecorView().hasWindowFocus()) {
                RaidTime.refresh();
                updateClocks();
            }
            main.postDelayed(this, 1000);
        }
    };

    RaidLocationsDialog(Activity activity, SharedPreferences settings) {
        this.activity = activity;
        this.settings = settings;
        selected = RaidLocationPrefs.selected(settings);
    }

    void show(Runnable saved) {
        RaidTime.init(activity);
        RaidTime.refresh();
        dialog = new Dialog(activity);
        ui = new ModeUi(activity);
        LinearLayout page = ui.column();
        page.setPadding(ui.dp(18), ui.dp(14), ui.dp(18), ui.dp(14));
        page.setBackgroundColor(ModeUi.BG);
        page.addView(ui.text("Raid location", 23, ModeUi.TEXT));
        page.addView(ui.text("Choose one city for this session", 14, ModeUi.MUTED));
        summary = ui.text("", 14, ModeUi.ACCENT);
        page.addView(summary);
        EditText search = new EditText(activity);
        search.setSingleLine(true);
        search.setHint("Search city or country");
        search.setTextColor(ModeUi.TEXT);
        search.setHintTextColor(ModeUi.MUTED);
        page.addView(search);
        list = new ListView(activity);
        list.setDividerHeight(ui.dp(7));
        adapter = new Adapter();
        list.setAdapter(adapter);
        page.addView(list, new LinearLayout.LayoutParams(-1, 0, 1));
        list.setOnItemClickListener((parent, view, position, id) -> {
            selected = visible.get(position).id;
            adapter.notifyDataSetChanged();
            selectedLabel();
        });
        LinearLayout footer = ui.row();
        Button cancel = ui.button(footer, "Cancel", false), save = ui.button(footer, "Use location", true);
        for (int i = 0; i < 2; i++)
            footer.getChildAt(i).setLayoutParams(new LinearLayout.LayoutParams(0, ui.dp(52), 1));
        page.addView(footer);
        cancel.setOnClickListener(v -> dialog.dismiss());
        save.setOnClickListener(v -> {
            RaidLocationPrefs.select(settings, selected);
            dialog.dismiss();
            saved.run();
        });
        search.addTextChangedListener(new TextWatcher() {
            public void beforeTextChanged(CharSequence s, int a, int c, int f) {
            }

            public void afterTextChanged(Editable s) {
            }

            public void onTextChanged(CharSequence s, int a, int b, int c) {
                filter(s.toString());
            }
        });
        dialog.setOnDismissListener(v -> {
            closed = true;
            main.removeCallbacks(clock);
        });
        filter("");
        selectedLabel();
        dialog.setContentView(page);
        dialog.show();
        if (dialog.getWindow() != null)
            dialog.getWindow().setLayout(Math.min(activity.getResources().getDisplayMetrics().widthPixels - ui.dp(24), ui.dp(600)), (int) (activity.getResources().getDisplayMetrics().heightPixels * .87));
        main.post(clock);
    }

    private void selectedLabel() {
        RaidLocations.Location entry = RaidLocations.get(selected);
        summary.setText(entry.flag + "  " + entry.name + " · " + entry.country);
    }

    private void filter(String text) {
        visible.clear();
        String query = RaidLocations.normalize(text);
        for (RaidLocations.Location entry : RaidLocations.ALL)
            if (entry.search.contains(query)) visible.add(entry);
        adapter.notifyDataSetChanged();
    }

    private void updateClocks() {
        for (int i = 0; i < list.getChildCount(); i++) {
            Object tag = list.getChildAt(i).getTag();
            if (tag instanceof Holder h) h.update();
        }
    }

    private final class Holder {
        RaidLocations.Location entry;
        TextView time, zone;

        void update() {
            time.setText(RaidTime.local(entry));
            zone.setText(RaidTime.zone(entry));
        }
    }

    private final class Adapter extends BaseAdapter {
        public int getCount() {
            return visible.size();
        }

        public Object getItem(int position) {
            return visible.get(position);
        }

        public long getItemId(int position) {
            return visible.get(position).id;
        }

        public View getView(int position, View recycled, ViewGroup parent) {
            RaidLocations.Location entry = visible.get(position);
            LinearLayout row = ui.row();
            row.setGravity(Gravity.CENTER_VERTICAL);
            row.setPadding(ui.dp(12), ui.dp(8), ui.dp(12), ui.dp(8));
            GradientDrawable shape = new GradientDrawable();
            shape.setColor(ModeUi.PANEL);
            shape.setCornerRadius(ui.dp(14));
            if (entry.id == selected) shape.setStroke(ui.dp(2), ModeUi.ACCENT);
            row.setBackground(shape);
            TextView flag = ui.text(entry.flag, 27, ModeUi.TEXT);
            flag.setContentDescription(entry.country + " flag");
            row.addView(flag, new LinearLayout.LayoutParams(ui.dp(52), -2));
            LinearLayout labels = ui.column();
            TextView name = ui.text(entry.name, 17, ModeUi.TEXT);
            name.setTypeface(null, Typeface.BOLD);
            labels.addView(name);
            labels.addView(ui.text(entry.country, 12, ModeUi.MUTED));
            Holder holder = new Holder();
            holder.entry = entry;
            holder.time = ui.text("", 14, entry.id == selected ? ModeUi.ACCENT : ModeUi.TEXT);
            holder.zone = ui.text("", 11, ModeUi.MUTED);
            labels.addView(holder.time);
            labels.addView(holder.zone);
            row.addView(labels, new LinearLayout.LayoutParams(0, -2, 1));
            if (entry.id == selected) row.addView(ui.text("●", 20, ModeUi.ACCENT));
            row.setTag(holder);
            holder.update();
            return row;
        }
    }
}
