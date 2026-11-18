package dev.silphmod.nativecheck;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;

final class RaidTargetPlan {
    final Map<String, Integer> limits;

    private RaidTargetPlan(Map<String, Integer> values) {
        limits = Collections.unmodifiableMap(new LinkedHashMap<>(values));
    }

    static String key(RaidFeed.Candidate c) {
        return c.species + ":" + c.level + ":" + c.form + ":" + c.costume + ":" + c.temporaryEvolution;
    }

    static String levelLabel(int level) {
        if (level >= 1 && level <= 5) return level + "★";
        if (level >= 11 && level <= 15) return (level - 10) + "★ Shadow";
        if (level >= 20 && level <= 24) return (level - 19) + "★";
        if (level >= 25 && level <= 29) return (level - 24) + "★ Ultra Beast";
        return switch (level) {
            case 6 -> "Mega";
            case 7 -> "5★ Mega";
            case 8 -> "Ultra Beast";
            case 9 -> "Elite";
            case 10 -> "Primal";
            case 16 -> "4★ Mega";
            case 17 -> "5★ Mega";
            case 18, 19 -> "Coordinated raid";
            case 30, 31 -> "Ultra Burst";
            default -> "Special raid";
        };
    }

    static int[] parts(String key) {
        if (key == null || !key.matches("[0-9]{1,5}(:[0-9]{1,5}){4}"))
            throw new IllegalArgumentException("Invalid raid target");
        String[] tokens = key.split(":");
        int[] p = new int[5];
        for (int i = 0; i < 5; i++) p[i] = Integer.parseInt(tokens[i]);
        if (p[0] < 1 || p[0] >= 2048 || p[1] < 1 || p[1] > 100 || p[2] > 65535 || p[3] > 65535 || p[4] > 100)
            throw new IllegalArgumentException("Invalid raid target");
        return p;
    }

    static RaidTargetPlan parse(String value) {
        LinkedHashMap<String, Integer> result = new LinkedHashMap<>();
        if (value == null || value.isEmpty()) return new RaidTargetPlan(result);
        if (value.length() > 8192) throw new IllegalArgumentException("Too many raid targets");
        String[] rows = value.split(";", -1);
        if (rows.length > 128) throw new IllegalArgumentException("Too many raid targets");
        for (String row : rows) {
            String[] pair = row.split("=", -1);
            if (pair.length != 2) throw new IllegalArgumentException("Invalid raid quantity");
            parts(pair[0]);
            if (!pair[1].matches("[0-9]{1,5}"))
                throw new IllegalArgumentException("Invalid raid quantity");
            int n = Integer.parseInt(pair[1]);
            if (n < 0 || n > 50000 || result.put(pair[0], n) != null)
                throw new IllegalArgumentException("Invalid raid quantity");
        }
        return new RaidTargetPlan(result);
    }

    static String encode(Map<String, Integer> limits) {
        StringBuilder out = new StringBuilder();
        for (Map.Entry<String, Integer> row : limits.entrySet()) {
            if (out.length() > 0) out.append(';');
            out.append(row.getKey()).append('=').append(row.getValue());
        }
        String text = out.toString();
        parse(text);
        return text;
    }

    String current(Map<String, Integer> wins) {
        for (Map.Entry<String, Integer> row : limits.entrySet())
            if (row.getValue() > 0) return row.getKey();
        return "";
    }

    String target(RaidFeed.Candidate candidate) {
        for (Map.Entry<String, Integer> row : limits.entrySet())
            if (row.getValue() > 0 && RaidEventPolicy.matches(candidate, parts(row.getKey())))
                return row.getKey();
        return "";
    }

    boolean allows(RaidFeed.Candidate candidate, Map<String, Integer> wins) {
        return !target(candidate).isEmpty();
    }

    boolean reached(Map<String, Integer> wins) {
        if (limits.isEmpty()) return false;
        for (Map.Entry<String, Integer> row : limits.entrySet())
            if (row.getValue() > 0) return false;
        return true;
    }
}
