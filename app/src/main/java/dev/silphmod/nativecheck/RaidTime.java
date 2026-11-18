package dev.silphmod.nativecheck;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.SystemClock;
import android.provider.Settings;

import java.util.Collections;
import java.time.Instant;
import java.time.ZonedDateTime;
import java.time.format.DateTimeFormatter;
import java.util.Locale;
import java.util.concurrent.Executors;
import java.util.concurrent.ExecutorService;

import javax.net.ssl.HttpsURLConnection;

final class RaidTime {
    private static final RaidClock CLOCK = new RaidClock(SystemClock::elapsedRealtime);
    private static final ExecutorService NETWORK = Executors.newSingleThreadExecutor(r -> new Thread(r, "silph-raid-clock"));
    private static final DateTimeFormatter TIME = DateTimeFormatter.ofPattern("HH:mm:ss · EEE, dd MMM", Locale.ENGLISH);
    private static SharedPreferences store;
    private static int boot = -1;
    private static boolean loading;
    private static long attempted = -60000, verified = -1;

    static synchronized void init(Context context) {
        if (store != null) return;
        RaidProxy.init(context);
        store = context.getApplicationContext().getSharedPreferences("raid-clock", 0);
        boot = Settings.Global.getInt(context.getContentResolver(), Settings.Global.BOOT_COUNT, -1);
        if (boot >= 0 && store.getInt("boot", -2) == boot)
            CLOCK.observe(store.getLong("utc", -1), store.getLong("elapsed", -1));
    }

    static long now() {
        return CLOCK.now();
    }

    static synchronized void observe(long server, long elapsed) {
        if (!CLOCK.observe(server, elapsed)) return;
        verified = elapsed;
        if (store != null && boot >= 0)
            store.edit().putLong("utc", server).putLong("elapsed", elapsed).putInt("boot", boot).apply();
    }

    static synchronized void refresh() {
        long elapsed = SystemClock.elapsedRealtime();
        if (store == null || loading || elapsed - attempted < 60000 || verified >= 0 && elapsed - verified < 60000)
            return;
        loading = true;
        attempted = elapsed;
        NETWORK.execute(() -> {
            HttpsURLConnection connection = null;
            try {
                connection = RaidProxy.open(RaidSources.clock(), 6000, Collections.emptyMap(), () -> false, c -> {
                });
                int status = connection.getResponseCode();
                if (status == 200)
                    observe(RaidFeedClient.serverTime(connection), SystemClock.elapsedRealtime());
            } catch (Exception ignored) {
            } finally {
                if (connection != null) connection.disconnect();
                synchronized (RaidTime.class) {
                    loading = false;
                }
            }
        });
    }

    static String local(RaidLocations.Location location) {
        long utc = now();
        if (utc < 0) return "Synchronizing time…";
        return TIME.format(Instant.ofEpochMilli(utc).atZone(location.zone));
    }

    static String zone(RaidLocations.Location location) {
        long utc = now();
        if (utc < 0) return location.timezone;
        ZonedDateTime time = Instant.ofEpochMilli(utc).atZone(location.zone);
        String offset = time.getOffset().getId();
        return location.timezone + " · UTC" + (offset.equals("Z") ? "+00:00" : offset);
    }

    static long manualAt(SharedPreferences settings) {
        long entered = settings.getLong("raid.lastActionElapsed", -1), age = settings.getLong("raid.lastActionAgeMs", -1), now = now(), elapsed = SystemClock.elapsedRealtime();
        if (now >= 0 && entered >= 0 && age >= 0 && boot >= 0 && settings.getInt("raid.lastActionBoot", -2) == boot && elapsed >= entered)
            return now - age - (elapsed - entered);
        return settings.getLong("raid.lastActionAt", 0);
    }

    static int manualMinutes(SharedPreferences settings) {
        long now = now(), at = manualAt(settings);
        if (at == 0) return 120;
        return now >= 0 ? (int) Math.min(120, Math.max(0, (now - at) / 60000)) : (int) Math.min(120, Math.max(0, settings.getLong("raid.lastActionAgeMs", 7200000) / 60000));
    }

    static void saveManual(SharedPreferences.Editor editor, String point, int minutes) {
        long utc = now();
        editor.putString("raid.lastActionCoordinates", point).putLong("raid.lastActionAt", point.isEmpty() || utc < 0 ? 0 : utc - minutes * 60000L)
                .putLong("raid.lastActionElapsed", point.isEmpty() ? -1 : SystemClock.elapsedRealtime()).putLong("raid.lastActionAgeMs", minutes * 60000L).putInt("raid.lastActionBoot", boot);
    }

    private RaidTime() {
    }
}
