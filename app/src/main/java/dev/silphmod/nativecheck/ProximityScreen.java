package dev.silphmod.nativecheck;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorManager;
import android.hardware.display.DisplayManager;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.os.SystemClock;
import android.view.Display;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.function.LongSupplier;
import java.util.concurrent.TimeUnit;

final class ProximityScreen {
    private final PowerManager.WakeLock wake;
    private final CheckLog log;
    private final DisplayManager displays;
    private final PowerManager power;
    private final LongSupplier progress;
    private final Handler main = new Handler(Looper.getMainLooper());
    private volatile Process root;
    private volatile OutputStream input;
    private volatile boolean rootReady, rootHeld, rootBlanked, rootFailed, closed;
    private boolean desired, disabled, watching, screenKnown, screenOff;
    private long lastProgress, progressAt;

    static boolean supported(Context context) {
        SensorManager sensors = context.getSystemService(SensorManager.class);
        PowerManager power = context.getSystemService(PowerManager.class);
        return sensors != null && sensors.getDefaultSensor(Sensor.TYPE_PROXIMITY) != null && power != null && power.isWakeLockLevelSupported(PowerManager.PROXIMITY_SCREEN_OFF_WAKE_LOCK);
    }

    ProximityScreen(Context context, CheckLog log, boolean enabled, LongSupplier progress) {
        this.log = log;
        this.progress = progress;
        displays = context.getSystemService(DisplayManager.class);
        power = context.getSystemService(PowerManager.class);
        PowerManager.WakeLock lock = null;
        if (enabled && supported(context)) try {
            lock = power.newWakeLock(PowerManager.PROXIMITY_SCREEN_OFF_WAKE_LOCK, "SilphMod:proximity");
            lock.setReferenceCounted(false);
        } catch (RuntimeException error) {
            log.write("proximity_screen", "unavailable=" + error.getClass().getSimpleName());
        }
        wake = lock;
        log.write("proximity_screen", "enabled=" + enabled + " available=" + (wake != null));
        if (wake != null) {
            String apk = context.getApplicationInfo().sourceDir;
            new Thread(() -> startRoot(apk), "silph-screen").start();
            main.postDelayed(() -> {
                if (!rootReady && !closed) {
                    rootFailed = true;
                    Process process = root;
                    if (process != null) process.destroy();
                    active(desired);
                }
            }, 5000);
        }
    }

    private void startRoot(String apk) {
        String path = "'" + apk.replace("'", "'\\''") + "'";
        String command = "CLASSPATH=" + path + " app_process /system/bin --nice-name=silph-screen dev.silphmod.nativecheck.ProximityHost";
        try {
            Process process = new ProcessBuilder("su", "-c", command).redirectErrorStream(true).start();
            root = process;
            if (closed || rootFailed) {
                process.destroy();
                return;
            }
            input = process.getOutputStream();
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream(), StandardCharsets.UTF_8))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    if (line.equals("READY")) {
                        rootReady = true;
                        main.post(() -> {
                            log.write("proximity_screen", "backend=root");
                            active(desired);
                        });
                    } else if (line.equals("ON")) {
                        rootHeld = true;
                        log.write("proximity_screen", "held=true backend=root");
                    } else if (line.equals("OFF")) {
                        if (rootHeld) log.write("proximity_screen", "held=false backend=root");
                        rootHeld = false;
                    } else if (line.equals("FACE_DOWN_ON") || line.equals("FACE_DOWN_OFF")) {
                        rootBlanked = line.equals("FACE_DOWN_ON");
                        log.write("proximity_screen", "faceDown=" + rootBlanked);
                    } else if (line.startsWith("FACE_DOWN_")) {
                        log.write("proximity_screen", line.toLowerCase(java.util.Locale.ROOT));
                    } else if (line.equals("UNAVAILABLE")) break;
                }
            }
        } catch (Exception error) {
            if (!closed)
                log.write("proximity_screen", "root_failed=" + error.getClass().getSimpleName());
        } finally {
            rootReady = false;
            rootHeld = false;
            input = null;
            rootFailed = true;
            Process process = root;
            if (process != null) process.destroy();
            if (rootBlanked) try {
                Process restore = new ProcessBuilder("su", "-c", command + " restore").redirectErrorStream(true).start();
                try {
                    if (restore.waitFor(3, TimeUnit.SECONDS)) {
                        byte[] bytes = new byte[512];
                        int count = restore.getInputStream().read(bytes);
                        log.write("proximity_screen", "panel_restore=" + new String(bytes, 0, Math.max(count, 0), StandardCharsets.UTF_8).trim());
                    }
                } finally {
                    restore.destroy();
                }
            } catch (Exception error) {
                log.write("proximity_screen", "restore_failed=" + error.getClass().getSimpleName());
            }
            rootBlanked = false;
            if (!closed) main.post(() -> active(desired));
        }
    }

    private void send(int value) {
        OutputStream pipe = input;
        if (pipe != null) try {
            pipe.write(value);
            pipe.flush();
        } catch (Exception error) {
            log.write("proximity_screen", "command_failed=" + error.getClass().getSimpleName());
        }
    }

    private boolean held() {
        return rootReady ? rootHeld : wake != null && wake.isHeld();
    }

    void active(boolean active) {
        desired = active;
        if (wake == null || (disabled || closed) && active) return;
        if (!active && !disabled && !closed && held() && power.isInteractive() && displayOff()) {
            disabled = true;
            log.write("proximity_screen", "disabled_for_session=game_focus_paused");
        }
        if (rootReady) {
            if (wake.isHeld()) wake.release();
            send(active ? '1' : '0');
        } else if (rootFailed) try {
            if (active && !wake.isHeld()) wake.acquire(12_000L);
            else if (!active && wake.isHeld()) wake.release();
        } catch (RuntimeException error) {
            log.write("proximity_screen", "state_failed=" + error.getClass().getSimpleName());
        }
        if (active && (rootReady || rootFailed) && !watching) {
            watching = true;
            lastProgress = progress.getAsLong();
            progressAt = SystemClock.elapsedRealtime();
            main.postDelayed(watch, 1000);
        }
        if (!active) {
            watching = false;
            screenKnown = false;
            main.removeCallbacks(watch);
        }
    }

    private final Runnable watch = new Runnable() {
        public void run() {
            if (closed || disabled || !desired) {
                watching = false;
                return;
            }
            if (rootReady) send('1');
            else if (rootFailed && wake != null) try {
                wake.acquire(12_000L);
            } catch (RuntimeException error) {
                log.write("proximity_screen", "renew_failed=" + error.getClass().getSimpleName());
            }
            long now = SystemClock.elapsedRealtime(), sample = progress.getAsLong();
            if (sample != lastProgress) {
                lastProgress = sample;
                progressAt = now;
            }
            boolean off = rootBlanked || displayOff();
            if (!screenKnown || screenOff != off) {
                screenKnown = true;
                screenOff = off;
                log.write("proximity_screen", "screenOff=" + off + " sequence=" + sample);
            }
            if (off && now - progressAt >= 6000) {
                disabled = true;
                active(false);
                log.write("proximity_screen", "disabled_for_session=frame_progress_paused");
                return;
            }
            main.postDelayed(this, 1000);
        }
    };

    private boolean displayOff() {
        Display display = displays == null ? null : displays.getDisplay(Display.DEFAULT_DISPLAY);
        return display != null && display.getState() == Display.STATE_OFF;
    }

    void close() {
        if (closed) return;
        closed = true;
        main.removeCallbacksAndMessages(null);
        active(false);
        send('Q');
        OutputStream pipe = input;
        if (pipe != null) try {
            pipe.close();
        } catch (Exception ignored) {
        }
    }
}
