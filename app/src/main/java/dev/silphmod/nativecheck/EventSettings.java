package dev.silphmod.nativecheck;

import android.content.SharedPreferences;

import dev.rootcatch.nativeengine.NativeSessionWire;

import java.util.*;

final class EventSettings {
    static final int[] KEYS = {1048, 212, 330, 365, 376, 395, 462, 534, 612, 184, 230, 306, 598, 635, 706, 715, 959, 980, 380, 381, 1054, 1053, 1052};
    static final String DEFAULT_SPECIES = "1048,212,330,365,376,395,462,534,612,184,230,306,598,635,706,715,959,980,380,381,1054,1053,1052";

    static Set<Integer> allowed() {
        Set<Integer> values = new HashSet<>();
        for (int id : KEYS) values.add(id);
        return values;
    }

    static void apply(SharedPreferences prefs, NativeSessionWire.Config c) {
        byte[] p = c.species;
        int at = 232;
        p[at] = (byte) 0xe7;
        p[at + 1] = 1;
        p[at + 2] = (byte) (prefs.getBoolean("event.catchAll", false) ? 4 : (prefs.getBoolean("event.shiny", true) ? 1 : 0) | (prefs.getBoolean("event.hundo", true) ? 2 : 0));
        List<Integer> selected = dev.rootcatch.automation.PokemonSelection.parse(prefs.getString("event.species", DEFAULT_SPECIES));
        int mask = 0;
        for (int i = 0; i < KEYS.length; i++) if (selected.contains(KEYS[i])) mask |= 1 << i;
        for (int i = 0; i < 4; i++) p[at + 4 + i] = (byte) (mask >>> (8 * i));
        int[] balls = ballOrder(prefs.getString("event.balls", "7,3,2,1"));
        p[at + 8] = (byte) balls.length;
        for (int i = 0; i < balls.length; i++) p[at + 9 + i] = (byte) balls[i];
        int[] berries = NativeSettings.integers(prefs.getString("event.berries", "706,708,701,705,703")), ids = {701, 703, 705, 706, 708};
        if (berries.length > 5) throw new IllegalArgumentException("Invalid event berry priority");
        p[at + 13] = (byte) berries.length;
        for (int i = 0; i < berries.length; i++) {
            int code = 0;
            for (int k = 0; k < ids.length; k++) if (ids[k] == berries[i]) code = k + 1;
            if (code == 0) throw new IllegalArgumentException("Invalid event berry");
            for (int k = 0; k < i; k++)
                if (berries[k] == berries[i])
                    throw new IllegalArgumentException("Duplicate event berry");
            p[at + 14 + i] = (byte) code;
        }
    }

    static int[] ballOrder(String value) {
        int[] ids = NativeSettings.integers(value);
        if (ids.length < 1 || ids.length > 4)
            throw new IllegalArgumentException("Select event balls");
        Set<Integer> seen = new HashSet<>();
        for (int id : ids)
            if ((id != 1 && id != 2 && id != 3 && id != 7) || !seen.add(id))
                throw new IllegalArgumentException("Invalid event ball priority");
        return ids;
    }

    static String ballName(int id) {
        return switch (id) {
            case 7 -> "GO Safari Ball";
            case 3 -> "Ultra Ball";
            case 2 -> "Great Ball";
            default -> "Poké Ball";
        };
    }

    static String ballNames(String value) {
        List<String> names = new ArrayList<>();
        for (int id : ballOrder(value)) names.add(ballName(id));
        return String.join(" · ", names);
    }
}
