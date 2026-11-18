package dev.silphmod.nativecheck;

final class StartupRoutePolicy {
    static int preferred(int route) {
        return route >= 0 && route <= 3 ? route : 2;
    }

    static boolean stable(long connectedAt, long now, long frames, boolean active,
                          boolean stopping, boolean foreground, int error, int reason) {
        return connectedAt > 0 && now - connectedAt >= 10_000 && frames >= 10
                && active && !stopping && foreground && error == 0 && reason == 0;
    }
}
