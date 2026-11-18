package dev.silphmod.nativecheck;

import android.app.Activity;
import android.app.Dialog;
import android.content.SharedPreferences;
import android.text.InputType;
import android.widget.*;

final class RaidLimitsDialog {
    private final Activity activity;
    private final SharedPreferences settings;
    private final ModeUi ui;

    RaidLimitsDialog(Activity activity, SharedPreferences settings) {
        this.activity = activity;
        this.settings = settings;
        ui = new ModeUi(activity);
    }

    void show(Runnable saved) {
        RaidTime.init(activity);
        RaidTime.refresh();
        int location = RaidLocationPrefs.selected(settings);
        RaidLocations.Location city = RaidLocations.get(location);
        Dialog dialog = new Dialog(activity);
        LinearLayout page = ui.column();
        page.setPadding(ui.dp(20), ui.dp(12), ui.dp(20), ui.dp(16));
        page.setBackgroundColor(ModeUi.BG);
        ScrollView scroll = new ScrollView(activity);
        scroll.setBackgroundColor(ModeUi.BG);
        scroll.setFillViewport(true);
        scroll.addView(page);
        page.addView(ui.text("Lobby & limits", 22, ModeUi.TEXT));
        page.addView(ui.text(city.flag + "  " + city.name + " · " + city.country, 17, ModeUi.ACCENT));
        String[] labels = {"Minimum trainers", "Leave if trainers are missing at (seconds remaining)", "Reference coordinates", "Search radius (km)"};
        String[] values = {Integer.toString(settings.getInt("raid.minPlayers", 2)), Integer.toString(settings.getInt("raid.exitRemainingSeconds", 20)), RaidLocationPrefs.reference(settings, location), Integer.toString(RaidLocationPrefs.radius(settings, location))};
        EditText[] inputs = new EditText[labels.length];
        for (int i = 0; i < labels.length; i++) {
            if (i == 2) page.addView(ui.text("Search area", 17, ModeUi.ACCENT));
            page.addView(ui.text(labels[i], 14, ModeUi.MUTED));
            EditText input = inputs[i] = new EditText(activity);
            input.setSingleLine(true);
            input.setTextColor(ModeUi.TEXT);
            input.setContentDescription(labels[i]);
            input.setInputType(i == 2 ? InputType.TYPE_CLASS_TEXT : InputType.TYPE_CLASS_NUMBER);
            input.setText(values[i]);
            page.addView(input);
            if (i == 2)
                ui.button(page, "Use suggested reference", false).setOnClickListener(v -> inputs[2].setText(city.reference()));
        }
        ui.button(page, "Save", true).setOnClickListener(v -> {
            try {
                int players = Integer.parseInt(inputs[0].getText().toString()), remaining = Integer.parseInt(inputs[1].getText().toString()), radius = Integer.parseInt(inputs[3].getText().toString());
                if (players < 1 || players > 20 || remaining < 5 || remaining > 110)
                    throw new IllegalArgumentException();
                String reference = inputs[2].getText().toString().trim();
                new RaidSearchArea(reference, radius);
                SharedPreferences.Editor edit = settings.edit().putInt("raid.minPlayers", players).putInt("raid.exitRemainingSeconds", remaining).putString("raid.reference" + location, reference).putInt("raid.radiusKm." + location, radius);
                edit.apply();
                dialog.dismiss();
                saved.run();
            } catch (Exception error) {
                Toast.makeText(activity, "Check the lobby, search area and cooldown values", Toast.LENGTH_SHORT).show();
            }
        });
        ui.button(page, "Cancel", false).setOnClickListener(v -> dialog.dismiss());
        dialog.setContentView(scroll);
        dialog.show();
        if (dialog.getWindow() != null)
            dialog.getWindow().setLayout(Math.min(activity.getResources().getDisplayMetrics().widthPixels - ui.dp(24), ui.dp(480)), Math.min((int) (activity.getResources().getDisplayMetrics().heightPixels * .87), ui.dp(760)));
    }
}
