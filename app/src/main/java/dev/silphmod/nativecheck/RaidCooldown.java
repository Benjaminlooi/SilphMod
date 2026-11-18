package dev.silphmod.nativecheck;

final class RaidCooldown {
    private static final long[] SECONDS = {30, 60, 204, 300, 420, 480, 720, 900, 1140, 1320, 1680, 2100, 2400, 2700, 3300, 3480, 3720, 3900, 4140, 4680, 5040, 5520, 5940, 6420, 6840, 7020, 7200};
    private static final double[] KM = {1, 2, 5, 7, 10, 12, 18, 26, 42, 65, 81, 100, 220, 250, 374, 410, 460, 500, 546, 683, 780, 885, 983, 1087, 1184, 1262, 1300};
    private static final long MARGIN_MS = 1500;

    static double allowedMeters(long elapsed) {
        if (elapsed < 0) return 0;
        if (elapsed >= 7200000 + MARGIN_MS) return Double.POSITIVE_INFINITY;
        double meters = 50;
        for (int i = 0; i < SECONDS.length; i++) {
            if (elapsed < SECONDS[i] * 1000 + MARGIN_MS) break;
            meters = KM[i] * 1000;
        }
        return meters;
    }

    static long remainingMillis(double meters, long elapsed) {
        if (!Double.isFinite(meters) || meters < 0) return 7200000 + MARGIN_MS;
        long required = 0;
        if (meters > 50) {
            required = 7200000 + MARGIN_MS;
            for (int i = 0; i < KM.length; i++)
                if (meters <= KM[i] * 1000) {
                    required = SECONDS[i] * 1000 + MARGIN_MS;
                    break;
                }
        }
        return elapsed >= required ? 0 : required - Math.max(-7200000, elapsed);
    }

    private RaidCooldown() {
    }
}
