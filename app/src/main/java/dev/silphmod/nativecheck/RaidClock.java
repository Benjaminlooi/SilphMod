package dev.silphmod.nativecheck;

import java.util.function.LongSupplier;

final class RaidClock {
    private final LongSupplier elapsed;
    private long server = -1, received = -1;

    RaidClock(LongSupplier elapsed) {
        this.elapsed = elapsed;
    }

    synchronized boolean observe(long utc, long at) {
        if (utc < 1577836800000L || utc > 4102444800000L || at < 0 || at > elapsed.getAsLong())
            return false;
        server = utc;
        received = at;
        return true;
    }

    synchronized long now() {
        long at = elapsed.getAsLong();
        if (server < 0 || at < received || at - received > Long.MAX_VALUE - server) return -1;
        return server + at - received;
    }
}
