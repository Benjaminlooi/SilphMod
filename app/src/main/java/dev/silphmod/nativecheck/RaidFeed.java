package dev.silphmod.nativecheck;

import android.util.JsonReader;
import android.util.JsonToken;

import java.io.IOException;
import java.io.Reader;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

public final class RaidFeed {
    public static final int MEXICO = 1, JAPAN = 2, ALL_COUNTRIES = (1 << RaidLocations.COUNT) - 1;
    public static final long REFRESH_MS = 60000, MAX_AGE_MS = 180000;

    public static final class Candidate {
        public final int country, species, level, form, costume, gender, temporaryEvolution;
        public final double latitude, longitude;
        public final long expiresMs;
        public final String providerId, name, city, key;

        private Candidate(int country, String id, String name, String city, double lat, double lon, long end, int species, int level, int form, int costume, int gender, int evolution) {
            this.country = country;
            providerId = id;
            this.name = name;
            this.city = city;
            latitude = lat;
            longitude = lon;
            expiresMs = end;
            this.species = species;
            this.level = level;
            this.form = form;
            this.costume = costume;
            this.gender = gender;
            temporaryEvolution = evolution;
            key = country + ":" + Math.round(lat * 1000000) + ":" + Math.round(lon * 1000000) + ":" + end + ":" + species + ":" + level + ":" + form + ":" + costume + ":" + gender + ":" + evolution;
        }
    }

    public static final class Snapshot {
        public final int country, rows, invalid, unsupported, expired, duplicates;
        public final long serverTimeMs, receivedElapsedMs;
        public final List<Candidate> candidates;

        private Snapshot(int country, long server, long received, int rows, int invalid, int unsupported, int expired, int duplicates, List<Candidate> entries) {
            this.country = country;
            serverTimeMs = server;
            receivedElapsedMs = received;
            this.rows = rows;
            this.invalid = invalid;
            this.unsupported = unsupported;
            this.expired = expired;
            this.duplicates = duplicates;
            candidates = Collections.unmodifiableList(new ArrayList<>(entries));
        }

        public long nowMs(long elapsed) {
            if (elapsed < receivedElapsedMs) return -1;
            long age = elapsed - receivedElapsedMs;
            return age > MAX_AGE_MS || serverTimeMs > Long.MAX_VALUE - age ? -1 : serverTimeMs + age;
        }

        Snapshot revalidated(long server, long elapsed) {
            if (server < serverTimeMs || elapsed < receivedElapsedMs)
                throw new IllegalArgumentException("Invalid raid feed clock");
            return new Snapshot(country, server, elapsed, rows, invalid, unsupported, expired, duplicates, candidates);
        }

        public Candidate select(int selectedCountries, int activeCountry, double latitude, double longitude, long elapsed, long remainingMs, Set<String> completed, Map<String, Long> retryAfter) {
            if (!countryMask(selectedCountries) || activeCountry != country || (selectedCountries & (1 << (country - 1))) == 0 || !coordinate(latitude, longitude) || remainingMs < 0 || completed == null || retryAfter == null)
                return null;
            long now = nowMs(elapsed);
            if (now < 0) return null;
            Candidate best = null;
            double nearest = Double.POSITIVE_INFINITY;
            for (Candidate candidate : candidates) {
                Long retry = retryAfter.get(candidate.key);
                if (candidate.expiresMs <= now || candidate.expiresMs - now < remainingMs || completed.contains(candidate.key) || (retry != null && retry > elapsed))
                    continue;
                double distance = distanceMeters(latitude, longitude, candidate.latitude, candidate.longitude);
                if (distance < nearest || (distance == nearest && (best == null || candidate.expiresMs < best.expiresMs))) {
                    best = candidate;
                    nearest = distance;
                }
            }
            return best;
        }
    }

    static Candidate local(int country, String gym, double latitude, double longitude, long end, int species, int level, int form, int costume, int evolution, long utc) {
        if (!RaidLocations.valid(country) || gym == null || !gym.matches("[a-zA-Z0-9_.-]{1,160}") || !coordinate(latitude, longitude) || end <= utc || end - utc > 21600000 || species < 1 || species >= 2048 || level < 1 || level > 100 || form < 0 || form > 65535 || costume < 0 || costume > 65535 || evolution < 0 || evolution > 100)
            throw new IllegalArgumentException("Invalid observed raid");
        return new Candidate(country, gym, "", "", latitude, longitude, end, species, level, form, costume, 0, evolution);
    }

    static Snapshot observed(int country, long utc, long elapsed, List<Candidate> entries) {
        return new Snapshot(country, utc, elapsed, entries.size(), 0, 0, 0, 0, entries);
    }

    private RaidFeed() {
    }

    public static boolean countryMask(int mask) {
        return mask >= 0 && (mask & ~ALL_COUNTRIES) == 0;
    }

    private static boolean coordinate(double latitude, double longitude) {
        return Double.isFinite(latitude) && Double.isFinite(longitude) && latitude >= -90 && latitude <= 90 && longitude >= -180 && longitude <= 180;
    }

    public static double distanceMeters(double latitude, double longitude, double targetLatitude, double targetLongitude) {
        if (!coordinate(latitude, longitude) || !coordinate(targetLatitude, targetLongitude))
            return Double.POSITIVE_INFINITY;
        double y = Math.sin(Math.toRadians(targetLatitude - latitude) * 0.5), x = Math.sin(Math.toRadians(targetLongitude - longitude) * 0.5);
        double a = y * y + Math.cos(Math.toRadians(latitude)) * Math.cos(Math.toRadians(targetLatitude)) * x * x;
        a = Math.max(0, Math.min(1, a));
        return 12742000 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
    }

    private static String text(JsonReader reader, int limit) throws IOException {
        if (reader.peek() != JsonToken.STRING) {
            reader.skipValue();
            return "";
        }
        String value = reader.nextString();
        return value.length() <= limit ? value : "";
    }

    private static double number(JsonReader reader) throws IOException {
        JsonToken kind = reader.peek();
        if (kind != JsonToken.NUMBER && kind != JsonToken.STRING) {
            reader.skipValue();
            return Double.NaN;
        }
        String value = reader.nextString();
        if (value.length() > 32) return Double.NaN;
        try {
            return Double.parseDouble(value);
        } catch (NumberFormatException invalid) {
            return Double.NaN;
        }
    }

    private static int integer(JsonReader reader) throws IOException {
        double n = number(reader);
        return Double.isFinite(n) && n >= 0 && n <= Integer.MAX_VALUE && n == Math.rint(n) ? (int) n : -1;
    }

    private static long timestamp(JsonReader reader) throws IOException {
        JsonToken kind = reader.peek();
        if (kind != JsonToken.STRING && kind != JsonToken.NUMBER) {
            reader.skipValue();
            return -1;
        }
        String text = reader.nextString();
        if (!text.matches("[0-9]{9,13}")) return -1;
        try {
            long value = Long.parseLong(text);
            if (value >= 1577836800L && value <= 4102444800L) return value * 1000;
            return value >= 1577836800000L && value <= 4102444800000L ? value : -1;
        } catch (NumberFormatException invalid) {
            return -1;
        }
    }

    public static Snapshot read(Reader input, int country, long serverTimeMs, long receivedElapsedMs) throws IOException {
        if (input == null || !RaidLocations.valid(country) || serverTimeMs < 1577836800000L || receivedElapsedMs < 0)
            throw new IllegalArgumentException("Invalid raid feed context");
        JsonReader reader = new JsonReader(input);
        reader.setLenient(false);
        ArrayList<Candidate> entries = new ArrayList<>();
        HashSet<String> keys = new HashSet<>();
        int rows = 0, invalid = 0, unsupported = 0, expired = 0, duplicates = 0;
        reader.beginArray();
        while (reader.hasNext()) {
            rows++;
            if (reader.peek() != JsonToken.BEGIN_OBJECT) {
                reader.skipValue();
                invalid++;
                continue;
            }
            String id = "", name = "", city = "", type = "";
            double lat = Double.NaN, lon = Double.NaN;
            long end = -1;
            int species = -1, level = -1, form = 0, costume = 0, gender = 0, evolution = 0, seen = 0;
            boolean duplicate = false;
            reader.beginObject();
            while (reader.hasNext()) {
                String field = reader.nextName();
                int bit = switch (field) {
                    case "id" -> 1;
                    case "latitude" -> 2;
                    case "longitude" -> 4;
                    case "end_time" -> 8;
                    case "pokemon_id" -> 16;
                    case "level" -> 32;
                    case "form" -> 64;
                    case "costume" -> 128;
                    case "gender" -> 256;
                    case "temp_evolution_id" -> 512;
                    case "type" -> 1024;
                    default -> 0;
                };
                if (bit != 0 && (seen & bit) != 0) duplicate = true;
                seen |= bit;
                switch (field) {
                    case "id":
                        id = text(reader, 160);
                        break;
                    case "name":
                        name = text(reader, 512);
                        break;
                    case "city":
                        city = text(reader, 128);
                        break;
                    case "latitude":
                        lat = number(reader);
                        break;
                    case "longitude":
                        lon = number(reader);
                        break;
                    case "end_time":
                        end = timestamp(reader);
                        break;
                    case "pokemon_id":
                        species = integer(reader);
                        break;
                    case "level":
                        level = integer(reader);
                        break;
                    case "form":
                        form = integer(reader);
                        break;
                    case "costume":
                        costume = integer(reader);
                        break;
                    case "gender":
                        gender = integer(reader);
                        break;
                    case "temp_evolution_id":
                        evolution = integer(reader);
                        break;
                    case "type":
                        type = text(reader, 32);
                        break;
                    default:
                        reader.skipValue();
                }
            }
            reader.endObject();
            if (!type.equals("gym") || species == 0) {
                unsupported++;
                continue;
            }
            if (duplicate || !coordinate(lat, lon) || end < 0 || species < 1 || species >= 2048 || level < 1 || level > 100 || form < 0 || costume < 0 || gender < 0 || evolution < 0) {
                invalid++;
                continue;
            }
            if (end <= serverTimeMs) {
                expired++;
                continue;
            }
            Candidate value = new Candidate(country, id, name, city, lat, lon, end, species, level, form, costume, gender, evolution);
            if (keys.add(value.key)) entries.add(value);
            else duplicates++;
        }
        reader.endArray();
        if (reader.peek() != JsonToken.END_DOCUMENT)
            throw new IOException("Trailing raid feed data");
        return new Snapshot(country, serverTimeMs, receivedElapsedMs, rows, invalid, unsupported, expired, duplicates, entries);
    }
}
