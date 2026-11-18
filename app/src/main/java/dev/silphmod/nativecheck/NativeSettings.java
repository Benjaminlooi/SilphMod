package dev.silphmod.nativecheck;

import android.content.Context;
import android.content.SharedPreferences;

import dev.rootcatch.nativeengine.*;

import java.util.ArrayList;

final class NativeSettings {
    final SharedPreferences prefs;
    private final Context context;

    NativeSettings(Context context) {
        this.context = context.getApplicationContext();
        prefs = context.getSharedPreferences("native-settings", 0);
        migrate(context);
        RaidProgress.migrate(context, prefs);
    }

    private void migrate(Context context) {
        if (prefs.getBoolean("imported", false)) return;
        SharedPreferences old = context.getSharedPreferences("rootcatch", 0);
        java.util.Map<String, ?> values = old.getAll();
        SharedPreferences.Editor e = prefs.edit();
        String[][] booleans = {{"catches", "catch"}, {"spins", "spin"}, {"removeBuddy", "remove_buddy"}, {"transfer", "transfer"}, {"fixLocation", "resolve_error_12"}, {"berries", "use_berries"}, {"showIvNames", "show_iv_names"}, {"hideTrainerName", "hide_trainer_name"}, {"extendedManualTransfer", "extended_manual_transfer"}, {"fastGiftOpening", "fast_gift_opening"}};
        for (String[] keys : booleans)
            if (!prefs.contains(keys[0]) && values.get(keys[1]) instanceof Boolean)
                e.putBoolean(keys[0], (Boolean) values.get(keys[1]));
        String[][] numbers = {{"delay", "delay"}, {"catchesMax", "max_catches"}, {"spinsMax", "max_spins"}};
        for (String[] keys : numbers)
            if (!prefs.contains(keys[0]) && values.get(keys[1]) instanceof Number)
                e.putInt(keys[0], ((Number) values.get(keys[1])).intValue());
        if (!prefs.contains("species") && values.get("only_catch") instanceof String)
            e.putString("species", (String) values.get("only_catch"));
        if (!values.isEmpty() && !prefs.contains("keep")) {
            String[] keys = {"keep_shiny", "keep_hundo", "keep_dynamax", "keep_hatched", "keep_background", "keep_shiny_background", "keep_field_research"};
            int keep = 0;
            for (int i = 0; i < keys.length; i++)
                if (old.getBoolean(keys[i], i != 5)) keep |= 1 << i;
            e.putInt("keep", keep);
        }
        if (!prefs.contains("quality") && values.get("throw_quality") instanceof String) {
            String[] q = {"NORMAL", "NICE", "GREAT", "EXCELLENT"};
            for (int i = 0; i < q.length; i++)
                if (q[i].equals(values.get("throw_quality"))) e.putInt("quality", i + 1);
        }
        if (!prefs.contains("berriesOrder") && values.get("berry_order") instanceof String) {
            ArrayList<String> ids = new ArrayList<>();
            for (String name : dev.rootcatch.automation.BerryOptions.parse((String) values.get("berry_order")))
                for (ItemCatalog.Entry item : ItemCatalog.ENTRIES)
                    if (item.name().equals(name)) ids.add(Integer.toString(item.id()));
            e.putString("berriesOrder", String.join(",", ids));
        }
        if (!prefs.contains("limits") && values.get("discard_limits") instanceof String) {
            ArrayList<String> limits = new ArrayList<>();
            for (String pair : ((String) values.get("discard_limits")).split(",")) {
                String[] fields = pair.split("=");
                if (fields.length != 2) continue;
                for (ItemCatalog.Entry item : ItemCatalog.ENTRIES)
                    if (item.discardable() && item.name().equals(fields[0].trim()))
                        limits.add(item.id() + ":" + fields[1].trim());
            }
            if (!limits.isEmpty()) {
                e.putString("limits", String.join(",", limits));
                if (!prefs.contains("clean")) e.putBoolean("clean", old.getBoolean("clean", false));
            }
        }
        for (int mode = 1; mode <= 2; mode++) {
            SharedPreferences stats = context.getSharedPreferences(mode == 1 ? "farm_last_result" : "kecleon_last_result", 0);
            for (String key : new String[]{"caught", "fled", "spun", "discarded", "transferred", "encounters", "shinies", "xp", "stardust", "elapsed"})
                if (!prefs.contains("stats" + mode + "." + key) && stats.getAll().get(key) instanceof Number)
                    e.putLong("stats" + mode + "." + key, Math.max(0, ((Number) stats.getAll().get(key)).longValue()));
        }
        SharedPreferences k = context.getSharedPreferences("silph_kecleon", 0);
        if (!prefs.contains("encountersMax") && k.contains("maximum"))
            e.putInt("encountersMax", k.getInt("maximum", 1000));
        if (!prefs.contains("filters") && !k.getAll().isEmpty()) {
            int filter = 0;
            String[] keys = {"hundo", "nundo", "shiny", "shundo"};
            for (int i = 0; i < 4; i++) if (k.getBoolean(keys[i], false)) filter |= 1 << i;
            e.putInt("filters", filter);
        }
        if (!e.putBoolean("imported", true).commit())
            throw new IllegalStateException("Cannot save settings");
    }

    private long clock;
    private boolean timing;

    long elapsed(int mode) {
        return prefs.getLong("stats" + mode + ".elapsed", 0);
    }

    void elapsedPulse(int mode, boolean active) {
        if (mode == 5) return;
        long now = android.os.SystemClock.elapsedRealtime();
        if (clock == 0) {
            clock = now;
            timing = active;
            return;
        }
        long delta = now - clock;
        if (delta >= 1000 || !active) {
            if (timing && delta > 0 && delta <= 10000) {
                long before = elapsed(mode);
                prefs.edit().putLong("stats" + mode + ".elapsed", before > Long.MAX_VALUE - delta ? Long.MAX_VALUE : before + delta).apply();
            }
            clock = now;
        }
        timing = active;
    }

    String berryNames() {
        return berryNames("berriesOrder", "701,705,703");
    }

    String buddyBerryNames() {
        return berryNames("buddyBerriesOrder", "703,701,705");
    }

    String berryNames(String key, String fallback) {
        ArrayList<String> names = new ArrayList<>();
        for (int id : integers(prefs.getString(key, fallback)))
            names.add(dev.rootcatch.nativeengine.ItemCatalog.ENTRIES.stream().filter(e -> e.id() == id).findFirst().orElseThrow().name());
        return String.join(",", names);
    }

    void saveBerries(String names) {
        saveBerries(names, "berriesOrder");
    }

    void saveBuddyBerries(String names) {
        saveBerries(names, "buddyBerriesOrder");
    }

    private void saveBerries(String names, String key) {
        ArrayList<String> ids = new ArrayList<>();
        for (String name : dev.rootcatch.automation.BerryOptions.parse(names))
            ids.add(Integer.toString(dev.rootcatch.nativeengine.ItemCatalog.ENTRIES.stream().filter(e -> e.name().equals(name)).findFirst().orElseThrow().id()));
        android.content.SharedPreferences.Editor e = prefs.edit().putString(key, String.join(",", ids));
        if (key.equals("berriesOrder")) e.putBoolean("berries", !ids.isEmpty());
        e.apply();
    }

    void saveEnhancerBerries(String names, boolean buddy) {
        saveBerries(names, buddy ? "enh.buddyBerriesOrder" : "enh.berriesOrder");
    }

    void saveEventBerries(String names) {
        saveBerries(names, "event.berries");
    }

    void saveRaidBerries(String names) {
        saveBerries(names, "raid.berriesOrder");
    }

    boolean fixLocation(int mode) {
        return prefs.getBoolean(mode == 6 ? "raid.fixLocation" : mode == 5 ? "enh.fixLocation" : "fixLocation", (mode == 5 || mode == 6) && prefs.getBoolean("fixLocation", false));
    }

    boolean hideName(int mode) {
        return mode == 6 || prefs.getBoolean(mode == 5 ? "enh.hideTrainerName" : "hideTrainerName", true);
    }

    NativeSessionState.Totals totals(int mode) {
        if (mode == 5) return new NativeSessionState.Totals();
        String p = "stats" + mode + ".";
        return new NativeSessionState.Totals(prefs.getLong(p + "caught", 0), prefs.getLong(p + "fled", 0), prefs.getLong(p + "spun", 0), prefs.getLong(p + "discarded", 0), prefs.getLong(p + "transferred", 0), prefs.getLong(p + "encounters", 0), prefs.getLong(p + "shinies", 0), prefs.getLong(p + "xp", 0), prefs.getLong(p + "stardust", 0));
    }

    void save(int mode, NativeSessionState.Totals t) {
        if (mode == 5) return;
        String p = "stats" + mode + ".";
        prefs.edit().putLong(p + "caught", t.caught).putLong(p + "fled", t.fled).putLong(p + "spun", t.spun).putLong(p + "discarded", t.discarded).putLong(p + "transferred", t.transferred).putLong(p + "encounters", t.encounters).putLong(p + "shinies", t.shinies).putLong(p + "xp", t.xp).putLong(p + "stardust", t.stardust).apply();
    }

    NativeSessionWire.Config config(int mode, boolean readonly) {
        if (mode == 6) return raidConfig(readonly);
        if (mode == NativeSessionWire.ENHANCER) return enhancerConfig(readonly);
        if (mode < 1 || mode > 3) throw new IllegalArgumentException("Mode unavailable");
        NativeSessionWire.Config c = new NativeSessionWire.Config();
        c.mode = mode;
        c.delayMs = prefs.getInt("delay", 100);
        c.durationMs = readonly ? 300000 : 0;
        if (readonly) {
            if (prefs.getBoolean("fixLocation", false))
                throw new IllegalArgumentException("Turn off Read-only check to use Fix error 12");
            c.flags = NativeSessionWire.READ_ONLY;
            return c;
        }
        c.flags = mode != 1 ? NativeSessionWire.CATCH : (prefs.getBoolean("catches", true) ? 1 : 0) | (prefs.getBoolean("spins", true) ? 2 : 0);
        if ((c.flags & 1) != 0 && prefs.getBoolean("berries", true)) {
            c.flags |= 16;
            c.berries = integers(prefs.getString("berriesOrder", "701,705,703"));
        }
        if (prefs.getBoolean("curve", true)) c.flags |= 32;
        c.quality = prefs.getInt("quality", 4);
        c.keep = prefs.getInt("keep", 95) | (prefs.getBoolean("keepGoPass", true) ? 128 : 0);
        if (prefs.getBoolean("hideTrainerName", true)) c.flags |= NativeSessionWire.TRAINER_NAME;
        if (prefs.getBoolean("showIvNames", true)) c.flags |= NativeSessionWire.IV_NAMES;
        if (prefs.getBoolean("fixLocation", false)) c.flags |= NativeSessionWire.FIX_LOCATION;
        c.flags |= NativeSessionWire.SCENES;
        if (mode == 1 && prefs.getBoolean("automaticEventActions", true))
            c.flags |= NativeSessionWire.EVENT_ACTIONS;
        if (prefs.getBoolean("extendedManualTransfer", false))
            c.flags |= NativeSessionWire.MANUAL_TRANSFER;
        if (prefs.getBoolean("fastGiftOpening", true)) c.flags |= NativeSessionWire.FAST_GIFTS;
        if (mode != 3 && prefs.getBoolean("autoOpenGifts", false))
            c.flags |= NativeSessionWire.AUTO_OPEN_GIFTS;
        if (mode != 3 && prefs.getBoolean("autoSendGifts", false))
            c.flags |= NativeSessionWire.AUTO_SEND_GIFTS;
        if (prefs.getBoolean("removeBuddy", false)) c.flags |= NativeSessionWire.REMOVE_BUDDY;
        if (mode == 1 && (c.flags & 1) != 0 && prefs.getBoolean("transfer", false)) c.flags |= 8;
        if (prefs.getBoolean("clean", false)) {
            String text = prefs.getString("limits", "").trim();
            ArrayList<int[]> limits = new ArrayList<>();
            if (!text.isEmpty()) for (String item : text.split(",")) {
                String[] pair = item.trim().split(":");
                if (pair.length != 2)
                    throw new IllegalArgumentException("Item limits: item ID:keep count");
                limits.add(new int[]{Integer.parseInt(pair[0].trim()), Integer.parseInt(pair[1].trim())});
            }
            if (limits.isEmpty()) throw new IllegalArgumentException("Add item limits first");
            c.flags |= 4;
            c.limits = limits.toArray(new int[0][]);
        }
        NativeSessionState.Totals t = totals(mode);
        if (mode == 1) {
            c.maxCatches = (c.flags & 1) != 0 ? remaining(prefs.getInt("catchesMax", 4800), t.caught, "Catch") : 0;
            c.maxSpins = (c.flags & 2) != 0 ? remaining(prefs.getInt("spinsMax", 2400), t.spun, "Spin") : 0;
            if (prefs.getBoolean("farmer.tapTeleport", false))
                c.flags |= NativeSessionWire.ENH_TELEPORT;
            for (int id : dev.rootcatch.automation.PokemonSelection.parse(prefs.getString("species", "")))
                c.select(id);
            if ((c.flags & NativeSessionWire.EVENT_ACTIONS) != 0) EventSettings.apply(prefs, c);
            for (int id : dev.rootcatch.automation.PokemonSelection.parse(prefs.getString("keepSpecies", dev.rootcatch.automation.PokemonSelection.KEEP_DEFAULT)))
                c.retain(id);
            if (!prefs.getBoolean("removeBuddy", false)) {
                if (prefs.getBoolean("feedBuddy", false)) {
                    c.buddyBerries = integers(prefs.getString("buddyBerriesOrder", "703,701,705"));
                    if (c.buddyBerries.length > 0) c.buddyFlags |= 1;
                }
                if (prefs.getBoolean("megaBuddy", false)) c.buddyFlags |= 2;
            }
        } else if (mode == 3) {
            RocketTypes.checked(prefs.getInt("rocketTypes", RocketTypes.ALL));
            if (prefs.getBoolean("rocketRecommendedTeam", true))
                c.flags |= NativeSessionWire.RECOMMENDED_TEAM;
            c.maxEncounters = remaining(prefs.getInt("rocketsMax", 700), t.spun, "Rocket");
            c.filters = prefs.getBoolean("rocketCatchAll", true) ? 0 : prefs.getInt("rocketFilters", 0);
            c.potionOrder = RocketCareOrder.parse(prefs.getString("rocketPotionOrder", ""), false);
            c.reviveOrder = RocketCareOrder.parse(prefs.getString("rocketReviveOrder", ""), true);
            if (!prefs.getBoolean("rocketCatchAll", true) && c.filters == 0)
                throw new IllegalArgumentException("Select a catch filter");
            String location = prefs.getString("rocketCooldownLocation", "").trim();
            if (!location.isEmpty()) dev.rootcatch.ugphone.KecleonFeed.parseCoordinates(location);
        } else {
            c.maxEncounters = remaining(prefs.getInt("encountersMax", 1000), t.encounters, "Encounter");
            c.filters = prefs.getInt("filters", 0);
        }
        return c;
    }

    private boolean raidPending() {
        SharedPreferences history = context.getSharedPreferences("raid-history", 0);
        String account = history.getString("activeAccount", "");
        if (account.isEmpty()) return false;
        String pending = history.getString("pending." + account, "");
        if (pending.isEmpty()) return false;
        long now = RaidTime.now();
        if (now < 0) return true;
        try {
            return RaidPending.decode(pending, now) != null;
        } catch (Exception error) {
            return false;
        }
    }

    private NativeSessionWire.Config raidConfig(boolean readonly) {
        int location = RaidLocationPrefs.selected(prefs);
        RaidLocations.get(location);
        boolean resuming = raidPending();
        if (!resuming && (RaidTargetPlan.parse(prefs.getString("raid.targets", "")).limits.isEmpty() || RaidProgress.remaining(prefs) == 0))
            throw new IllegalArgumentException("Set pending quantities in Select raid targets");
        int players = prefs.getInt("raid.minPlayers", 2);
        if (players < 1 || players > 20) throw new IllegalArgumentException("Invalid raid limits");
        int wait = prefs.getInt("raid.exitRemainingSeconds", 20);
        if (wait < 5 || wait > 110)
            throw new IllegalArgumentException("Lobby countdown limit must be 5–110 seconds");
        new RaidSearchArea(RaidLocationPrefs.reference(prefs, location), RaidLocationPrefs.radius(prefs, location));
        int filters = prefs.getInt("raid.filters", 0);
        if (!prefs.getBoolean("raid.catchAll", true) && (filters < 1 || filters > 31))
            throw new IllegalArgumentException("Select a catch filter");
        NativeSessionWire.Config c = new NativeSessionWire.Config();
        c.mode = 5;
        c.delayMs = 100;
        c.quality = prefs.getInt("raid.quality", 4);
        if (readonly) {
            c.flags = NativeSessionWire.READ_ONLY;
            c.durationMs = 300000;
            return c;
        }
        c.flags = NativeSessionWire.CATCH | NativeSessionWire.SCENES | NativeSessionWire.TRAINER_NAME | NativeSessionWire.IV_NAMES | NativeSessionWire.ENH_AUTORAID | NativeSessionWire.ENH_RAID | NativeSessionWire.ENH_RAID_HEAL | NativeSessionWire.ENH_FAST | NativeSessionWire.ENH_INTRO | NativeSessionWire.ENH_THROW;
        if (prefs.getBoolean("raid.lightGraphics", false))
            c.flags |= NativeSessionWire.ENH_LIGHT_GRAPHICS;
        if (fixLocation(6)) c.flags |= NativeSessionWire.FIX_LOCATION;
        if (prefs.getBoolean("raid.curve", true)) c.flags |= NativeSessionWire.CURVE;
        if (prefs.getBoolean("raid.berries", true)) {
            c.berries = integers(prefs.getString("raid.berriesOrder", "706,708,701,705,703"));
            if (c.berries.length > 0) c.flags |= NativeSessionWire.BERRIES;
        }
        if (prefs.getBoolean("raid.transfer", false)) c.flags |= NativeSessionWire.ENH_TRANSFER;
        c.keep = prefs.getInt("raid.keep", prefs.getInt("keep", 95)) | 128;
        for (int id : dev.rootcatch.automation.PokemonSelection.parse(prefs.getString("keepSpecies", dev.rootcatch.automation.PokemonSelection.KEEP_DEFAULT)))
            c.retain(id);
        return c;
    }

    private NativeSessionWire.Config enhancerConfig(boolean readonly) {
        NativeSessionWire.Config c = new NativeSessionWire.Config();
        c.mode = NativeSessionWire.ENHANCER;
        c.delayMs = 100;
        c.quality = prefs.getInt("enh.quality", 4);
        if (readonly) {
            c.flags = NativeSessionWire.READ_ONLY;
            c.durationMs = 300000;
            return c;
        }
        c.flags = NativeSessionWire.CATCH;
        String[] keys = {"fastCatch", "skipIntro", "autoRocket", "tapTeleport", "throwAssist", "aimAssist", "autoRaid", "encounterInfo"};
        int[] bits = {NativeSessionWire.ENH_FAST, NativeSessionWire.ENH_INTRO, NativeSessionWire.ENH_ROCKET, NativeSessionWire.ENH_TELEPORT, NativeSessionWire.ENH_THROW, NativeSessionWire.ENH_AIM, NativeSessionWire.ENH_RAID, NativeSessionWire.ENH_INFO};
        boolean[] defaults = {true, true, false, false, true, true, false, true};
        for (int i = 0; i < keys.length; i++)
            if (prefs.getBoolean("enh." + keys[i], defaults[i])) c.flags |= bits[i];
        if (prefs.getBoolean("enh.curve", true)) c.flags |= NativeSessionWire.CURVE;
        if (prefs.getBoolean("enh.berries", false)) {
            c.berries = integers(prefs.getString("enh.berriesOrder", "701,705,703"));
            if (c.berries.length > 0) c.flags |= NativeSessionWire.BERRIES;
        }
        String[] tools = {"fixLocation", "hideTrainerName", "showIvNames", "skipScenes", "extendedManualTransfer", "fastGiftOpening", "autoOpenGifts", "autoSendGifts", "recommendedTeam"};
        int[] flags = {NativeSessionWire.FIX_LOCATION, NativeSessionWire.TRAINER_NAME, NativeSessionWire.IV_NAMES, NativeSessionWire.SCENES, NativeSessionWire.MANUAL_TRANSFER, NativeSessionWire.FAST_GIFTS, NativeSessionWire.AUTO_OPEN_GIFTS, NativeSessionWire.AUTO_SEND_GIFTS, NativeSessionWire.RECOMMENDED_TEAM};
        boolean[] fallback = {prefs.getBoolean("fixLocation", false), true, true, true, false, true, false, false, true};
        for (int i = 0; i < tools.length; i++)
            if (prefs.getBoolean("enh." + tools[i], fallback[i])) c.flags |= flags[i];
        if (prefs.getBoolean("enh.removeBuddy", false)) c.flags |= NativeSessionWire.REMOVE_BUDDY;
        else {
            if (prefs.getBoolean("enh.feedBuddy", false)) {
                c.buddyBerries = integers(prefs.getString("enh.buddyBerriesOrder", "703,701,705"));
                if (c.buddyBerries.length > 0) c.buddyFlags |= 1;
            }
            if (prefs.getBoolean("enh.megaBuddy", false)) c.buddyFlags |= 2;
        }
        if (prefs.getBoolean("enh.healRaidTeam", false)) c.flags |= NativeSessionWire.ENH_RAID_HEAL;
        if (prefs.getBoolean("enh.transfer", false)) c.flags |= NativeSessionWire.ENH_TRANSFER;
        c.keep = prefs.getInt("enh.keep", 95) | (prefs.getBoolean("enh.keepGoPass", true) ? 128 : 0);
        for (int id : dev.rootcatch.automation.PokemonSelection.parse(prefs.getString("enh.keepSpecies", dev.rootcatch.automation.PokemonSelection.KEEP_DEFAULT)))
            c.retain(id);
        return c;
    }

    static int[] integers(String s) {
        if (s == null || s.trim().isEmpty()) return new int[0];
        String[] values = s.split(",");
        int[] out = new int[values.length];
        for (int i = 0; i < values.length; i++) out[i] = Integer.parseInt(values[i].trim());
        return out;
    }

    private static int remaining(int maximum, long current, String name) {
        if (maximum < 1 || maximum > 50000)
            throw new IllegalArgumentException(name + " limit must be 1–50000");
        if (current >= maximum)
            throw new IllegalArgumentException(name + " limit reached; clear statistics or increase limit");
        return (int) (maximum - current);
    }
}
