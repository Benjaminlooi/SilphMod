package dev.silphmod.nativecheck;

import android.content.SharedPreferences;

final class RaidLocationPrefs {
    static int selected(SharedPreferences settings) {
        int chosen = settings.getInt("raid.location", 0);
        if (RaidLocations.valid(chosen)) return chosen;
        int old = settings.getInt("raid.countries", 1);
        for (int id = 1; id <= RaidLocations.COUNT; id++)
            if ((old & (1 << (id - 1))) != 0) return id;
        return 1;
    }

    static String reference(SharedPreferences settings, int id) {
        return settings.getString("raid.reference" + id, RaidSearchArea.defaultPoint(id));
    }

    static int radius(SharedPreferences settings, int id) {
        return settings.getInt("raid.radiusKm." + id, settings.getInt("raid.radiusKm", 5));
    }

    static void select(SharedPreferences settings, int id) {
        RaidLocations.get(id);
        settings.edit().putInt("raid.location", id).putInt("raid.countries", 1 << (id - 1)).apply();
    }

    private RaidLocationPrefs() {
    }
}
