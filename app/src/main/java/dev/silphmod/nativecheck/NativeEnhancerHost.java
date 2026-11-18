package dev.silphmod.nativecheck;

import android.content.Context;
import android.content.Intent;
import android.content.ComponentName;
import android.content.pm.ResolveInfo;

import dev.rootcatch.nativeengine.NativeSessionWire;
import dev.rootcatch.root.RootShell;
import dev.rootcatch.ugphone.KecleonMovement;

import java.util.Locale;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.function.BooleanSupplier;

final class NativeEnhancerHost implements AutoCloseable {
    private final RootShell joystick = new RootShell();
    private final CheckLog log;
    private final BooleanSupplier active;
    private final Context context;
    private ComponentName destination;
    private boolean directChecked;
    private final ThreadPoolExecutor worker = new ThreadPoolExecutor(1, 1, 0, TimeUnit.MILLISECONDS, new ArrayBlockingQueue<>(1), r -> new Thread(r, "silph-map-gesture"), new ThreadPoolExecutor.DiscardOldestPolicy());
    private volatile boolean closed;
    private long seen;

    NativeEnhancerHost(Context context, CheckLog log, BooleanSupplier active) {
        this.context = context.getApplicationContext();
        this.log = log;
        this.active = active;
    }

    private boolean direct(double latitude, double longitude, String prefix) {
        Intent intent = new Intent("theappninjas.gpsjoystick.TELEPORT");
        if (!directChecked) {
            directChecked = true;
            ResolveInfo info = context.getPackageManager().resolveService(intent, 0);
            if (info != null && info.serviceInfo != null && info.serviceInfo.enabled && info.serviceInfo.exported && info.serviceInfo.permission == null)
                destination = new ComponentName(info.serviceInfo.packageName, info.serviceInfo.name);
        }
        if (destination == null) return false;
        try {
            return context.startForegroundService(intent.setComponent(destination).putExtra("lat", (float) latitude).putExtra("lng", (float) longitude)) != null;
        } catch (RuntimeException error) {
            destination = null;
            log.write(prefix + "_route", "Direct delivery unavailable; existing adapter retained");
            return false;
        }
    }

    synchronized void accept(NativeSessionWire.Snapshot value) {
        if (closed) return;
        boolean farmer = value.mode == NativeSessionWire.FARMER;
        long next = farmer ? value.tapRequest : value.candidateRequest;
        if (farmer ? (next == 0 || next == seen) : (value.mode != NativeSessionWire.ENHANCER || next <= seen))
            return;
        seen = next;
        long request = seen, received = android.os.SystemClock.elapsedRealtime();
        double lat = value.latitude, lon = value.longitude;
        String prefix = farmer ? "farmer_map" : "enhancer_map";
        if (!Double.isFinite(lat) || !Double.isFinite(lon) || Math.abs(lat) > 90 || Math.abs(lon) > 180)
            return;
        worker.execute(() -> {
            if (closed || !active.getAsBoolean() || android.os.SystemClock.elapsedRealtime() - received > 2500)
                return;
            try {
                if (direct(lat, lon, prefix)) {
                    log.write(prefix + "_gesture", "request=" + request + " accepted=true route=direct ms=" + (android.os.SystemClock.elapsedRealtime() - received));
                    return;
                }
                RootShell.Result result = joystick.execute(KecleonMovement.command(lat, lon), 3000);
                String lower = result.output.toLowerCase(Locale.ROOT);
                log.write(prefix + "_gesture", "request=" + request + " accepted=" + (result.code == 0 && !lower.contains("error") && !lower.contains("exception") && !lower.contains("not found")) + " route=adapter ms=" + (android.os.SystemClock.elapsedRealtime() - received));
            } catch (Exception error) {
                if (!closed) log.write(prefix + "_gesture_error", error.toString());
            }
        });
    }

    @Override
    public synchronized void close() {
        closed = true;
        worker.shutdownNow();
        joystick.close();
    }
}
