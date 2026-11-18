package dev.silphmod.nativecheck;

import java.util.LinkedHashMap;
import java.util.Map;

final class RaidEventPolicy {
    static boolean group(int species) {
        return species == 68;
    }

    static String key(String value) {
        int[] p = RaidTargetPlan.parts(value);
        return group(p[0]) ? p[0] + ":" + p[1] + ":0:0:" + p[4] : value;
    }

    static boolean matches(RaidFeed.Candidate c, int[] p) {
        return p != null && c.species == p[0] && c.level == p[1] && c.temporaryEvolution == p[4] && (group(c.species) || c.form == p[2] && c.costume == p[3]);
    }

    static Map<String, Integer> grouped(Map<String, Integer> values) {
        Map<String, Integer> result = new LinkedHashMap<>();
        for (Map.Entry<String, Integer> row : values.entrySet())
            result.merge(key(row.getKey()), row.getValue(), Integer::sum);
        return result;
    }
}
