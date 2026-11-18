package dev.silphmod.nativecheck;

final class RaidSearchArea {
    static String defaultPoint(int country) {
        return RaidLocations.get(country).reference();
    }

    final double latitude, longitude, radius;

    RaidSearchArea(String point, int kilometers) {
        String[] parts = point.split(",", -1);
        if (parts.length != 2 || kilometers < 1 || kilometers > 100)
            throw new IllegalArgumentException("Invalid raid reference area");
        latitude = Double.parseDouble(parts[0].trim());
        longitude = Double.parseDouble(parts[1].trim());
        radius = kilometers * 1000.0;
        if (!Double.isFinite(latitude) || !Double.isFinite(longitude) || latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180)
            throw new IllegalArgumentException("Invalid raid reference coordinates");
    }

    double distance(RaidFeed.Candidate item) {
        return RaidFeed.distanceMeters(latitude, longitude, item.latitude, item.longitude);
    }

    boolean includes(RaidFeed.Candidate item) {
        return distance(item) <= radius;
    }
}
