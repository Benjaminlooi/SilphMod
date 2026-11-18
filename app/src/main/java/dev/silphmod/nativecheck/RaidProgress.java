package dev.silphmod.nativecheck;

import android.content.Context;
import android.content.SharedPreferences;

import java.util.*;

final class RaidProgress {
    static synchronized void migrate(Context context, SharedPreferences settings) {
        if (settings.getBoolean("raid.pendingQuantities", false)) return;
        SharedPreferences history = context.getSharedPreferences("raid-history", 0);
        String account = history.getString("activeAccount", "");
        Map<String, Integer> pending = new LinkedHashMap<>();
        for (Map.Entry<String, Integer> row : RaidTargetPlan.parse(settings.getString("raid.targets", "")).limits.entrySet())
            pending.put(row.getKey(), Math.max(0, row.getValue() - (account.isEmpty() ? 0 : history.getInt("wins." + account + "." + row.getKey(), 0))));
        if (!settings.edit().putString("raid.targets", RaidTargetPlan.encode(pending)).putBoolean("raid.pendingQuantities", true).commit())
            throw new IllegalStateException("Cannot save raid progress");
    }

    static synchronized boolean credit(SharedPreferences settings, String account, String raid, String target) {
        String key = account + "|" + raid;
        Set<String> credited = new LinkedHashSet<>(settings.getStringSet("raid.credited", Collections.emptySet()));
        if (credited.contains(key)) return false;
        Map<String, Integer> pending = new LinkedHashMap<>(RaidTargetPlan.parse(settings.getString("raid.targets", "")).limits);
        if (pending.containsKey(target)) pending.put(target, Math.max(0, pending.get(target) - 1));
        credited.add(key);
        while (credited.size() > 5000) credited.remove(credited.iterator().next());
        if (!settings.edit().putString("raid.targets", RaidTargetPlan.encode(pending)).putStringSet("raid.credited", credited).commit())
            throw new IllegalStateException("Cannot save raid progress");
        return true;
    }

    static long remaining(SharedPreferences settings) {
        long n = 0;
        for (int count : RaidTargetPlan.parse(settings.getString("raid.targets", "")).limits.values())
            n += count;
        return n;
    }
}
