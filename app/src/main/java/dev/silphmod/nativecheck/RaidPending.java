package dev.silphmod.nativecheck;

import java.io.StringReader;

import org.json.JSONObject;

final class RaidPending {
    final RaidFeed.Candidate candidate;
    final String gym;

    RaidPending(RaidFeed.Candidate candidate, String gym) {
        if (candidate == null || gym == null || !gym.matches("[A-Za-z0-9_.-]{1,160}"))
            throw new IllegalArgumentException("Invalid pending raid");
        this.candidate = candidate;
        this.gym = gym;
    }

    String encode() throws Exception {
        RaidFeed.Candidate c = candidate;
        JSONObject row = new JSONObject().put("id", c.providerId).put("name", c.name).put("city", c.city).put("latitude", c.latitude).put("longitude", c.longitude).put("end_time", Long.toString(c.expiresMs)).put("pokemon_id", c.species).put("level", c.level).put("form", c.form).put("costume", c.costume).put("gender", c.gender).put("temp_evolution_id", c.temporaryEvolution).put("type", "gym");
        return new JSONObject().put("country", c.country).put("gym", gym).put("raid", row).toString();
    }

    static RaidPending decode(String text, long now) throws Exception {
        if (text == null || text.isEmpty()) return null;
        if (text.length() > 8192) throw new IllegalArgumentException("Invalid pending raid");
        JSONObject root = new JSONObject(text);
        RaidFeed.Snapshot feed = RaidFeed.read(new StringReader("[" + root.getJSONObject("raid") + "]"), root.getInt("country"), now, 0);
        return feed.candidates.size() == 1 ? new RaidPending(feed.candidates.get(0), root.getString("gym")) : null;
    }
}
