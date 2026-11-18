package dev.silphmod.nativecheck;

import java.util.*;

final class RaidLocalSearch {
    static final int MAX_POINTS = 16, MAX_CANDIDATES = 256;
    static final long SCAN_MS = 15000, POINT_MS = 12000, SWEEP_MS = 45000, OBSERVED_MS = 45000;

    private static final class Observed {
        final RaidFeed.Candidate candidate;
        final long seen;

        Observed(RaidFeed.Candidate candidate, long seen) {
            this.candidate = candidate;
            this.seen = seen;
        }
    }

    static final class Point {
        final double latitude, longitude;

        Point(double latitude, double longitude) {
            this.latitude = latitude;
            this.longitude = longitude;
        }
    }

    private final LinkedHashMap<String, Observed> raids = new LinkedHashMap<>();
    private final List<Point> visits = new ArrayList<>();
    private List<Point> sweep = Collections.emptyList();
    private int index;
    private long nextScan, nextMove, nextSweep;
    private final int location;
    private final RaidSearchArea area;

    RaidLocalSearch(int location, RaidSearchArea area) {
        this.location = location;
        this.area = area;
    }

    void load(String value) {
        if (value == null || value.length() > 4096) return;
        List<Point> loaded = new ArrayList<>();
        for (String row : value.split(";")) {
            try {
                double[] p = RaidActionHistory.coordinates(row);
                if (inArea(p[0], p[1])) loaded.add(new Point(p[0], p[1]));
            } catch (IllegalArgumentException ignored) {
            }
        }
        for (int i = Math.min(loaded.size(), MAX_POINTS) - 1; i >= 0; i--)
            visit(loaded.get(i).latitude, loaded.get(i).longitude);
    }

    void seed(Set<String> completed) {
        if (!visits.isEmpty()) return;
        List<String[]> rows = new ArrayList<>();
        for (String key : completed) {
            String[] p = key.split(":");
            if (p.length == 10) try {
                if (Integer.parseInt(p[0]) == location) {
                    Long.parseLong(p[3]);
                    rows.add(p);
                }
            } catch (NumberFormatException ignored) {
            }
        }
        rows.sort(Comparator.comparingLong(p -> Long.parseLong(p[3])));
        for (String[] p : rows)
            try {
                visit(Long.parseLong(p[1]) / 1000000.0, Long.parseLong(p[2]) / 1000000.0);
            } catch (NumberFormatException ignored) {
            }
    }

    String encode() {
        StringJoiner out = new StringJoiner(";");
        for (Point point : visits)
            out.add(String.format(Locale.ROOT, "%.8f,%.8f", point.latitude, point.longitude));
        return out.toString();
    }

    void visit(double latitude, double longitude) {
        if (!inArea(latitude, longitude)) return;
        visits.removeIf(p -> RaidFeed.distanceMeters(p.latitude, p.longitude, latitude, longitude) < 50);
        visits.add(0, new Point(latitude, longitude));
        if (visits.size() > MAX_POINTS) visits.remove(visits.size() - 1);
    }

    boolean inArea(double latitude, double longitude) {
        return RaidFeed.distanceMeters(area.latitude, area.longitude, latitude, longitude) <= area.radius;
    }

    void observe(RaidFeed.Candidate candidate, long elapsed) {
        if (candidate.country != location || !area.includes(candidate)) return;
        raids.put(candidate.key, new Observed(candidate, elapsed));
        while (raids.size() > MAX_CANDIDATES) raids.remove(raids.keySet().iterator().next());
    }

    RaidFeed.Snapshot snapshot(long utc, long elapsed, Set<String> completed) {
        raids.entrySet().removeIf(r -> elapsed < r.getValue().seen || elapsed - r.getValue().seen > OBSERVED_MS || r.getValue().candidate.expiresMs <= utc);
        List<RaidFeed.Candidate> entries = new ArrayList<>();
        for (Observed row : raids.values())
            if (!completed.contains(gymKey(row.candidate))) entries.add(row.candidate);
        return RaidFeed.observed(location, utc, elapsed, entries);
    }

    static String gymKey(RaidFeed.Candidate candidate) {
        return "gym:" + candidate.country + ":" + candidate.providerId + ":" + candidate.expiresMs / 1000;
    }

    static String gymKey(int location, String gym, long expires) {
        return "gym:" + location + ":" + gym + ":" + expires / 1000;
    }

    static String identityKey(RaidFeed.Candidate candidate) {
        return "place:" + candidate.country + ":" + Math.round(candidate.latitude * 100000) + ":" + Math.round(candidate.longitude * 100000) + ":" + candidate.expiresMs / 1000;
    }

    boolean scanDue(long elapsed) {
        return elapsed >= nextScan;
    }

    void scanned(long elapsed) {
        nextScan = elapsed + SCAN_MS;
    }

    void arrived(long elapsed) {
        nextScan = elapsed + 2500;
        nextMove = elapsed + POINT_MS;
    }

    Point next(double latitude, double longitude, long elapsed) {
        if (elapsed < nextMove) return null;
        if (!inArea(latitude, longitude)) {
            nextMove = elapsed + POINT_MS;
            return new Point(area.latitude, area.longitude);
        }
        if (index >= sweep.size()) {
            if (elapsed < nextSweep) return null;
            List<Point> points = new ArrayList<>();
            for (Point point : visits)
                if (RaidFeed.distanceMeters(latitude, longitude, point.latitude, point.longitude) > 60)
                    points.add(point);
            sweep = points;
            index = 0;
            nextSweep = elapsed + SWEEP_MS;
        }
        if (index >= sweep.size()) return null;
        nextMove = elapsed + POINT_MS;
        return sweep.get(index++);
    }
}
