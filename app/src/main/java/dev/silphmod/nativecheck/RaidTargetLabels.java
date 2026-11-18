package dev.silphmod.nativecheck;

import android.content.Context;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;

final class RaidTargetLabels {
    private final Map<Integer, String> names = new HashMap<>(), forms = new HashMap<>(), costumes = new HashMap<>(), evolutions = new HashMap<>();

    RaidTargetLabels(Context context) {
        try (BufferedReader input = new BufferedReader(new InputStreamReader(context.getAssets().open("raids/names.tsv"), StandardCharsets.UTF_8))) {
            String line;
            while ((line = input.readLine()) != null) {
                String[] p = line.split("\t", 2);
                if (p.length == 2) names.put(Integer.parseInt(p[0]), p[1]);
            }
        } catch (IOException ignored) {
        }
        try (BufferedReader input = new BufferedReader(new InputStreamReader(context.getAssets().open("raids/variants.tsv"), StandardCharsets.UTF_8))) {
            String line;
            while ((line = input.readLine()) != null) {
                String[] p = line.split("\t", 3);
                if (p.length == 3)
                    (p[0].equals("f") ? forms : p[0].equals("c") ? costumes : evolutions).put(Integer.parseInt(p[1]), p[2]);
            }
        } catch (IOException ignored) {
        }
    }

    String label(String key) {
        if (key.isEmpty()) return "No pending targets";
        int[] p = RaidTargetPlan.parts(key);
        String name = names.getOrDefault(p[0], "Pokémon #" + p[0]);
        String form = forms.getOrDefault(p[2], "");
        if (!RaidEventPolicy.group(p[0]) && !form.isEmpty() && !form.equalsIgnoreCase(name + " Normal") && !form.equalsIgnoreCase(name)) {
            name = form;
        }
        String costume = costumes.getOrDefault(p[3], "");
        if (!RaidEventPolicy.group(p[0]) && !costume.isEmpty()) name += " · " + costume;
        if (p[4] > 0) name += " · " + evolutions.getOrDefault(p[4], "Mega Evolution");
        return name + " · " + RaidTargetPlan.levelLabel(p[1]);
    }
}
