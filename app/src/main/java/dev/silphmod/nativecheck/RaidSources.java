package dev.silphmod.nativecheck;

final class RaidSources {
    static String url(int location) {
        if (!RaidLocations.valid(location))
            throw new IllegalArgumentException("Invalid raid location");
        return BuildConfig.RAID_API_BASE + "/v1/raids/" + location;
    }

    static String clock() {
        return BuildConfig.RAID_API_BASE + "/v1/time";
    }

    private RaidSources() {
    }
}
