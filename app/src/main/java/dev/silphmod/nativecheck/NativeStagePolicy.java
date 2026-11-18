package dev.silphmod.nativecheck;

final class NativeStagePolicy {
    static boolean stableAppearance(java.util.Map<String, ?> values, String prefix, int version) {
        String current = prefix + version;
        if (values.containsKey(current)) return Boolean.TRUE.equals(values.get(current));
        for (java.util.Map.Entry<String, ?> entry : values.entrySet()) {
            if (!Boolean.TRUE.equals(entry.getValue()) || !entry.getKey().startsWith(prefix))
                continue;
            String suffix = entry.getKey().substring(prefix.length());
            if (!suffix.matches("[0-9]{1,9}")) continue;
            int prior = Integer.parseInt(suffix);
            if (prior > 0 && prior < version) return true;
        }
        return false;
    }

    static boolean managedReady(int mode, long stableMs, boolean presentationReady) {
        return stableMs >= 20_000 || (mode == 5 && presentationReady && stableMs >= 1500);
    }

    static boolean presentationReady(String output, int features) {
        if (output == null || (features != 128 && features != 640)) return false;
        java.util.regex.Pattern pattern = java.util.regex.Pattern.compile("I/SilphVisual\\s*\\(\\s*[0-9]+\\): startup presentation ready=1 features=" + features + " pointerOffset=([0-9]{1,3}) error=0");
        for (String line : output.split("\\R")) {
            java.util.regex.Matcher match = pattern.matcher(line.trim());
            if (match.matches() && Integer.parseInt(match.group(1)) <= 256) return true;
        }
        return false;
    }

    static boolean refreshNotification(boolean force, String previous, String next, long elapsedMs) {
        return force || !next.equals(previous) || elapsedMs >= 1000;
    }

    static int appearanceFeatures(boolean fixLocation) {
        return fixLocation ? 640 : 128;
    }

    static boolean presentationFailure(String stage, String message) {
        return ("appearance_bootstrap".equals(stage) || "wait_foreground".equals(stage)) && message != null
                && (message.contains("exited") || message.contains("Native startup fault"));
    }

    static boolean directory(String value) {
        return value != null && value.matches("/data/app/[A-Za-z0-9_./+=~-]+/lib/arm64") && !value.contains("/../") && !value.contains("/./");
    }

    static String observer(int mode, String hash) {
        if ((!dev.rootcatch.nativeengine.NativeSessionWire.validMode(mode)) || hash == null || !hash.matches("[a-f0-9]{64}"))
            throw new IllegalArgumentException("Invalid stage identity");
        return "libsilph-native-observer-" + mode + "-" + hash.substring(0, 16) + ".so";
    }

    static String bootstrapFailure(int exit, String stage, int error) {
        if ("preflight_rejected".equals(stage))
            return "Location startup rejected library identity (errno " + error + ")";
        if ("target_not_authorized".equals(stage))
            return "Location startup rejected game identity (errno " + error + ")";
        if ("invalid_arguments".equals(stage)) return "Location startup arguments rejected";
        return "Location bootstrap response missing (exit " + exit + ")";
    }

    static boolean namespaceRejected(String failure, String detail, boolean restored, boolean detached, boolean released, boolean restart, long actions) {
        return "LINKER".equals(failure) && detail != null && detail.contains("not accessible for the namespace")
                && restored && detached && released && !restart && actions == 0;
    }

    static int bootstrapRoute(int route, int attempt) {
        if (route < 0 || route > 3 || attempt < 0 || attempt > 7)
            throw new IllegalArgumentException("Invalid bootstrap route");
        return attempt % 2 == 0 ? route : route ^ 2;
    }

    static boolean retryBoundary(String failure, String stage, int error, boolean restored, boolean detached, boolean released, boolean restart, long actions, int returns) {
        return "ATTACH".equals(failure) && ("syscall_boundary".equals(stage) || "legacy_syscall_boundary".equals(stage)) && (error == 110 || error == 16)
                && restored && detached && released && !restart && actions == 0 && returns == 0;
    }

    static String startupFailure(String failure, String stage, int signal, int code) {
        if (signal > 0)
            return "Native startup fault: signal " + signal + "/code " + code + " at " + stage + "; export logs";
        if ("PROTOCOL".equals(failure)) return "Native profile rejected; export logs";
        if ("INITIALIZATION".equals(failure)) return "Native initialization failed; export logs";
        return null;
    }
}
