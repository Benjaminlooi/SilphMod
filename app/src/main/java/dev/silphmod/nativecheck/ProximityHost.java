package dev.silphmod.nativecheck;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.PowerManager;

import java.lang.reflect.Method;

public final class ProximityHost implements SensorEventListener {
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final PowerManager.WakeLock wake;
    private final SensorManager sensors;
    private final Sensor motion;
    private final Panel panel = new Panel();
    private boolean active, registered, blanked, down, failed;
    private float filtered;
    private boolean initialized;

    private ProximityHost(Context context) {
        PowerManager power = context.getSystemService(PowerManager.class);
        wake = power.newWakeLock(PowerManager.PROXIMITY_SCREEN_OFF_WAKE_LOCK, "SilphMod:proximity");
        wake.setReferenceCounted(false);
        sensors = context.getSystemService(SensorManager.class);
        Sensor sensor = sensors == null ? null : sensors.getDefaultSensor(Sensor.TYPE_GRAVITY);
        motion = sensor != null ? sensor : sensors == null ? null : sensors.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
    }

    private void command(int value) {
        if (value == '1') {
            if (!active) {
                active = true;
                initialized = false;
                if (motion != null && !failed)
                    registered = sensors.registerListener(this, motion, 200_000, handler);
                System.out.println("ON");
            }
            if (!blanked) wake.acquire(12_000L);
            handler.removeCallbacks(expire);
            handler.postDelayed(expire, 12_000L);
        } else {
            close();
            if (value == 'Q') System.exit(0);
        }
    }

    private final Runnable expire = () -> {
        close();
        System.exit(0);
    };
    private final Runnable confirm = () -> {
        if (active && down && !failed) setBlanked(true);
    };

    private void setBlanked(boolean value) {
        if (blanked == value) return;
        if (value) {
            if (wake.isHeld()) wake.release();
            if (!panel.set(true)) {
                failed = true;
                wake.acquire(12_000L);
                System.out.println("FACE_DOWN_UNAVAILABLE");
                return;
            }
            blanked = true;
            System.out.println("FACE_DOWN_ON");
        } else {
            if (!panel.set(false)) {
                System.out.println("FACE_DOWN_RESTORE_FAILED");
                return;
            }
            blanked = false;
            System.out.println("FACE_DOWN_OFF");
            if (active) wake.acquire(12_000L);
        }
    }

    private void close() {
        active = false;
        down = false;
        handler.removeCallbacks(expire);
        handler.removeCallbacks(confirm);
        if (registered) {
            sensors.unregisterListener(this);
            registered = false;
        }
        setBlanked(false);
        if (wake.isHeld()) wake.release();
        System.out.println("OFF");
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (!active || failed || event.values.length < 3) return;
        float z = event.values[2];
        if (motion.getType() == Sensor.TYPE_ACCELEROMETER) {
            filtered = initialized ? filtered * .7f + z * .3f : z;
            initialized = true;
            z = filtered;
        }
        boolean value = z < (blanked ? -5.0f : -8.0f);
        if (value != down) {
            down = value;
            handler.removeCallbacks(confirm);
            if (value) handler.postDelayed(confirm, 450);
            else setBlanked(false);
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
    }

    private static final class Panel {
        private Object owner, token;
        private Method method;
        private boolean booleanState, surface;

        private boolean resolve() {
            if (method != null) return true;
            try {
                Class<?> type = Class.forName("android.hardware.display.DisplayManagerGlobal");
                owner = type.getMethod("getInstance").invoke(null);
                try {
                    method = type.getMethod("requestDisplayPower", int.class, int.class);
                } catch (NoSuchMethodException missing) {
                    method = type.getMethod("requestDisplayPower", int.class, boolean.class);
                    booleanState = true;
                }
                return true;
            } catch (Exception unavailable) {
                owner = null;
                method = null;
            }
            try {
                Class<?> type = Class.forName("android.view.SurfaceControl");
                token = type.getMethod("getInternalDisplayToken").invoke(null);
                if (token == null) return false;
                method = type.getMethod("setDisplayPowerMode", IBinder.class, int.class);
                surface = true;
                return true;
            } catch (Exception unavailable) {
                return false;
            }
        }

        synchronized boolean set(boolean off) {
            if (!resolve()) return false;
            try {
                Object result = surface ? method.invoke(null, token, off ? 0 : 2) : booleanState ? method.invoke(owner, 0, !off) : method.invoke(owner, 0, off ? 1 : 0);
                return !(result instanceof Boolean) || ((Boolean) result);
            } catch (Exception unavailable) {
                return false;
            }
        }
    }

    @SuppressWarnings("deprecation")
    public static void main(String[] args) {
        try {
            Looper.prepareMainLooper();
            Class<?> type = Class.forName("android.app.ActivityThread");
            Method entry = type.getDeclaredMethod("systemMain");
            entry.setAccessible(true);
            Object thread = entry.invoke(null);
            Method contextMethod = type.getDeclaredMethod("getSystemContext");
            contextMethod.setAccessible(true);
            Context context = (Context) contextMethod.invoke(thread);
            if (args.length == 1 && args[0].equals("restore")) {
                System.out.println(new Panel().set(false) ? "RESTORED" : "UNAVAILABLE");
                return;
            }
            PowerManager power = context.getSystemService(PowerManager.class);
            if (power == null || !power.isWakeLockLevelSupported(PowerManager.PROXIMITY_SCREEN_OFF_WAKE_LOCK)) {
                System.out.println("UNAVAILABLE");
                return;
            }
            ProximityHost host = new ProximityHost(context);
            Runtime.getRuntime().addShutdownHook(new Thread(host::close, "silph-screen-release"));
            new Thread(() -> {
                try {
                    int command;
                    while ((command = System.in.read()) != -1) {
                        int value = command;
                        if (value != '1' && value != '0' && value != 'Q') continue;
                        host.handler.post(() -> {
                            try {
                                host.command(value);
                            } catch (RuntimeException error) {
                                host.close();
                                System.out.println("UNAVAILABLE");
                                System.exit(0);
                            }
                        });
                        if (value == 'Q') return;
                    }
                } catch (Exception ignored) {
                }
                host.handler.post(() -> {
                    host.close();
                    System.exit(0);
                });
            }, "silph-screen-input").start();
            System.out.println("READY");
            Looper.loop();
        } catch (Throwable error) {
            System.out.println("UNAVAILABLE");
        }
    }
}
