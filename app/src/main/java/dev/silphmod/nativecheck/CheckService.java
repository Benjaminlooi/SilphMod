package dev.silphmod.nativecheck;

import android.app.*;
import android.content.*;
import android.content.pm.ServiceInfo;
import android.os.*;

public final class CheckService extends Service {
    static final String START = "dev.silphmod.nativecheck.START", STOP = "dev.silphmod.nativecheck.STOP";

    static final class View {
        final boolean running;
        final String status, details;

        View(boolean running, String status, String details) {
            this.running = running;
            this.status = status;
            this.details = details;
        }
    }

    static volatile View view = new View(false, "Ready", "Pokémon GO 0.435.2 · ARM64");
    private final Handler main = new Handler(Looper.getMainLooper());
    private volatile NativeCheckRunner runner;
    private PowerManager.WakeLock wake;
    private ProximityScreen proximity;
    private FarmerActionOverlay farmerOverlay;
    private volatile boolean gameForeground;
    private long lastNotification;
    private String notificationStatus = "";
    private CheckLog log;
    private int mode = 1;

    @Override
    public void onCreate() {
        super.onCreate();
        NotificationManager manager = getSystemService(NotificationManager.class);
        manager.createNotificationChannel(new NotificationChannel("native-check", "SilphMod service", NotificationManager.IMPORTANCE_LOW));
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        String action = intent == null ? STOP : intent.getAction();
        if (STOP.equals(action)) {
            closeFarmerOverlay();
            if (proximity != null) proximity.close();
            NativeCheckRunner current = runner;
            if (current != null) current.stop();
            else stopSelf();
            return START_NOT_STICKY;
        }
        if (!START.equals(action) || runner != null) return START_NOT_STICKY;
        mode = intent.getIntExtra("mode", 1);
        view = new View(true, "Preparing…", "");
        if (Build.VERSION.SDK_INT >= 34)
            startForeground(1, notification(), ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
        else startForeground(1, notification());
        if (mode != 6 && !dev.rootcatch.nativeengine.NativeSessionWire.validMode(mode)) {
            view = new View(false, "Mode unavailable", "");
            stopForeground(STOP_FOREGROUND_REMOVE);
            stopSelf();
            return START_NOT_STICKY;
        }
        try {
            log = new CheckLog(this);
        } catch (Exception error) {
            publish(false, "Cannot save session log", error.toString(), true);
            stopSelf();
            return START_NOT_STICKY;
        }
        wake = getSystemService(PowerManager.class).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "SilphMod:service");
        wake.setReferenceCounted(false);
        wake.acquire(10 * 60_000L);
        main.postDelayed(renewWake, 5 * 60_000L);
        proximity = new ProximityScreen(this, log, mode != 5 && new NativeSettings(this).prefs.getBoolean("engine.proximity." + mode, false), () -> {
            NativeCheckRunner active = runner;
            return active == null ? 0 : active.progressSequence();
        });
        runner = new NativeCheckRunner(this, mode, log, (status, details) -> publish(true, status, details, false), this::foreground);
        if (mode == 1 && new NativeSettings(this).prefs.getBoolean(FarmerActionOverlay.ENABLED, false)) {
            farmerOverlay = new FarmerActionOverlay(this);
            main.post(refreshFarmerOverlay);
        }
        NativeCheckRunner active = runner;
        new Thread(() -> {
            NativeCheckRunner.Result result = active.run();
            log.write("finished", "reason=" + result.reason + " cleanupConfirmed=" + result.cleanup);
            publish(false, result.reason, "Cleanup: " + (result.cleanup ? "confirmed" : "not confirmed") + "\n" + active.details(), true);
            main.post(() -> {
                closeFarmerOverlay();
                runner = null;
                if (proximity != null) proximity.close();
                if (wake != null && wake.isHeld()) wake.release();
                stopForeground(STOP_FOREGROUND_DETACH);
                stopSelf();
            });
        }, "silph-service").start();
        return START_NOT_STICKY;
    }

    private void publish(boolean running, String status, String details, boolean force) {
        view = new View(running, status, details);
        long now = SystemClock.elapsedRealtime();
        if (!NativeStagePolicy.refreshNotification(force, notificationStatus, status, now - lastNotification))
            return;
        lastNotification = now;
        notificationStatus = status;
        main.post(() -> getSystemService(NotificationManager.class).notify(1, notification()));
    }

    private Notification notification() {
        PendingIntent open = PendingIntent.getActivity(this, 1, new Intent(this, MainActivity.class).putExtra("mode", mode), PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        Notification.Builder builder = new Notification.Builder(this, "native-check").setSmallIcon(R.drawable.ic_launcher).setContentTitle("SilphMod · " + view.status)
                .setContentText(view.details.replace('\n', ' ')).setStyle(new Notification.BigTextStyle().bigText(view.details)).setContentIntent(open).setOnlyAlertOnce(true).setOngoing(view.running);
        if (view.running)
            builder.addAction(new Notification.Action.Builder(null, "Stop", PendingIntent.getService(this, 2, new Intent(this, CheckService.class).setAction(STOP), PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE)).build());
        return builder.build();
    }

    private void foreground(boolean active) {
        if (gameForeground == active) return;
        gameForeground = active;
        main.post(() -> {
            if (proximity != null) proximity.active(gameForeground && runner != null);
        });
    }

    private final Runnable refreshFarmerOverlay = new Runnable() {
        @Override
        public void run() {
            if (farmerOverlay == null || runner == null) return;
            farmerOverlay.show(runner.farmerOverlaySnapshot());
            main.postDelayed(this, 250);
        }
    };

    private void closeFarmerOverlay() {
        main.removeCallbacks(refreshFarmerOverlay);
        if (farmerOverlay != null) {
            farmerOverlay.hide();
            farmerOverlay = null;
        }
    }

    private final Runnable renewWake = new Runnable() {
        public void run() {
            if (runner != null && wake != null) {
                wake.acquire(10 * 60_000L);
                main.postDelayed(this, 5 * 60_000L);
            }
        }
    };

    @Override
    public void onDestroy() {
        closeFarmerOverlay();
        main.removeCallbacks(renewWake);
        if (proximity != null) proximity.close();
        NativeCheckRunner active = runner;
        if (active != null) active.stop();
        if (wake != null && wake.isHeld()) wake.release();
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
