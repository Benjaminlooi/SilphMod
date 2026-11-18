package dev.silphmod.nativecheck;

import android.content.SharedPreferences;

import java.util.Map;
import java.util.Locale;

final class RaidActionHistory {
    static final class Action {
        final long at;
        final double latitude, longitude;

        Action(long at, double latitude, double longitude) {
            this.at = at;
            this.latitude = latitude;
            this.longitude = longitude;
        }
    }

    static void record(SharedPreferences history, String account, long at, double latitude, double longitude) {
        history.edit().putString("activeAccount", account).putLong("time." + account, at).putString("location." + account, String.format(Locale.ROOT, "%.8f,%.8f", latitude, longitude)).apply();
    }

    static Action read(SharedPreferences history) {
        String account = history.getString("activeAccount", "");
        if (account.isEmpty()) {
            long newest = 0;
            for (Map.Entry<String, ?> entry : history.getAll().entrySet())
                if (entry.getKey().startsWith("time.") && entry.getValue() instanceof Long && (Long) entry.getValue() > newest) {
                    newest = (Long) entry.getValue();
                    account = entry.getKey().substring(5);
                }
        }
        long at = history.getLong("time." + account, 0);
        if (at <= 0) return null;
        try {
            double[] point = coordinates(history.getString("location." + account, ""));
            return new Action(at, point[0], point[1]);
        } catch (IllegalArgumentException invalid) {
            return null;
        }
    }

    static double[] coordinates(String value) {
        if (value == null || value.length() > 96)
            throw new IllegalArgumentException("Invalid raid action coordinates");
        String[] parts = value.split(",", -1);
        if (parts.length != 2)
            throw new IllegalArgumentException("Invalid raid action coordinates");
        double latitude = Double.parseDouble(parts[0].trim()), longitude = Double.parseDouble(parts[1].trim());
        if (!Double.isFinite(latitude) || !Double.isFinite(longitude) || Math.abs(latitude) > 90 || Math.abs(longitude) > 180)
            throw new IllegalArgumentException("Invalid raid action coordinates");
        return new double[]{latitude, longitude};
    }

    static void clear(SharedPreferences history, SharedPreferences settings) {
        SharedPreferences.Editor edit = history.edit();
        for (String key : history.getAll().keySet())
            if (key.equals("activeAccount") || key.startsWith("done.") || key.startsWith("wins.") || key.startsWith("pending.") || key.startsWith("time.") || key.startsWith("location.") || key.startsWith("visits.") || key.startsWith("retry.") || key.startsWith("retryAt."))
                edit.remove(key);
        edit.apply();
        edit = settings.edit();
        for (String key : settings.getAll().keySet())
            if (key.startsWith("raid.lastAction")) edit.remove(key);
        edit.apply();
    }

    private RaidActionHistory() {
    }
}
