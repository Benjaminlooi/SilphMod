package dev.silphmod.nativecheck;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

final class NativeExitReport {
    final String summary, crash, history, processLog;
    final boolean recoverable, intentional, unidentified;

    private NativeExitReport(String summary, String crash, String history, String processLog, boolean recoverable, boolean intentional, boolean unidentified) {
        this.summary = summary;
        this.crash = crash;
        this.history = history;
        this.processLog = processLog;
        this.recoverable = recoverable;
        this.intentional = intentional;
        this.unidentified = unidentified;
    }

    static NativeExitReport inspect(String logcat, String exits, int pid, String process, long launched) {
        return inspect(logcat, exits, "", pid, process, launched);
    }

    static NativeExitReport inspect(String logcat, String exits, String events, int pid, String process, long launched) {
        if (pid <= 1 || process == null || process.isEmpty() || launched <= 0)
            return new NativeExitReport("Game exit not identified", "", "", "", false, false, false);
        String crash = crash(logcat == null ? "" : logcat, pid), history = "";
        int reason = -1;
        Pattern identity = Pattern.compile("timestamp=([^\\r\\n]+?) pid=" + pid + "(?:\\s|$)");
        Pattern name = Pattern.compile("(?:^|\\s)process=" + Pattern.quote(process) + "(?:\\s|$)");
        Pattern value = Pattern.compile("(?:^|\\s)reason=(\\d+)(?:\\s|$)");
        if (exits != null) for (String entry : exits.split("ApplicationExitInfo #")) {
            Matcher match = identity.matcher(entry);
            if (!match.find() || !name.matcher(entry).find()) continue;
            try {
                SimpleDateFormat format = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US);
                format.setLenient(false);
                Date time = format.parse(match.group(1).trim());
                if (time == null || time.getTime() < launched - 5000) continue;
                Matcher number = value.matcher(entry);
                if (!number.find()) continue;
                reason = Integer.parseInt(number.group(1));
                history = entry.length() > 12000 ? entry.substring(0, 12000) : entry;
                break;
            } catch (Exception ignored) {
            }
        }
        String processLog = processEvents(events == null ? "" : events, pid, process, launched);
        boolean requested = false, killed = false, anr = false, managedCrash = false;
        for (String line : processLog.split("\\R")) {
            if (line.contains("am_kill")) {
                killed = true;
                String detail = line.substring(line.indexOf('[')).toLowerCase(Locale.ROOT);
                requested |= detail.contains("user request") || detail.contains("force-stop") || detail.contains("force stop") || detail.contains("remove task") || detail.contains("stop " + process);
            }
            anr |= line.contains("am_anr");
            managedCrash |= line.contains("am_crash");
        }
        if (reason == 1 || reason == 10 || reason == 11 || requested)
            return new NativeExitReport("Game closed by user", crash, history, processLog, false, true, false);
        Matcher signal = Pattern.compile("(?:signal\\s+\\d+\\s*\\(|Fatal signal\\s+\\d+\\s*\\()(SIG[A-Z0-9]+)\\)").matcher(crash);
        if (signal.find()) {
            String detail = signal.group(1);
            Matcher frame = Pattern.compile("#00[^\\r\\n]*?([^/\\s]+\\.so)(?:[\\s(]|$)").matcher(crash);
            if (frame.find()) detail += " · " + frame.group(1);
            return new NativeExitReport("Native crash: " + detail, crash, history, processLog, true, false, false);
        }
        String summary = reason == 3 ? "Game stopped: low memory" : reason == 4 ? "Game crashed" : reason == 5 ? "Native game crash" : reason == 6 ? "Game stopped: ANR" : reason == 9 ? "Game stopped: resource limit" : reason == 12 ? "Game dependency exited" : reason == 14 ? "Game stopped: Android freezer" : "Game process exited; export logs";
        boolean known = reason > 0, recoverable = reason == 3 || reason == 4 || reason == 5 || reason == 6 || reason == 9 || reason == 12 || reason == 14;
        if (!known && (anr || managedCrash || killed)) {
            summary = anr ? "Game stopped: ANR" : managedCrash ? "Game crashed" : "Game stopped by Android; export logs";
            recoverable = true;
            known = true;
        }
        return new NativeExitReport(summary, crash, history, processLog, recoverable, false, !known && crash.isEmpty());
    }

    private static String processEvents(String text, int pid, String process, long launched) {
        Pattern target = Pattern.compile("^\\s*(\\d{10,}(?:\\.\\d{1,9})?)\\s+[^\\r\\n]*?\\b(am_kill|am_proc_died|am_crash|am_anr)\\s*:\\s*\\[\\s*\\d+\\s*,\\s*" + pid + "\\s*,\\s*" + Pattern.quote(process) + "\\s*(?:,|\\])");
        StringBuilder out = new StringBuilder();
        for (String line : text.split("\\R")) {
            Matcher match = target.matcher(line);
            if (!match.find()) continue;
            try {
                if (Double.parseDouble(match.group(1)) * 1000 < launched) continue;
            } catch (NumberFormatException ignored) {
                continue;
            }
            if (out.length() + line.length() + 1 > 12000) break;
            out.append(line).append('\n');
        }
        return out.toString();
    }

    private static String crash(String text, int pid) {
        Pattern target = Pattern.compile("\\bpid:\\s*" + pid + "(?:,|\\s|$)");
        Matcher header = target.matcher(text);
        int selected = -1;
        while (header.find()) selected = header.start();
        if (selected >= 0) {
            int start = text.lastIndexOf("*** ***", selected);
            if (start < 0) start = text.lastIndexOf('\n', selected) + 1;
            else start = text.lastIndexOf('\n', start) + 1;
            int end = text.indexOf("*** ***", selected);
            if (end < 0) end = text.length();
            else end = text.lastIndexOf('\n', end) + 1;
            if (end <= start) end = text.length();
            return text.substring(start, Math.min(end, start + 24000));
        }
        Pattern fatal = Pattern.compile("[^\\r\\n]*Fatal signal[^\\r\\n]*\\bpid\\s+" + pid + "(?:\\s|,|\\().*");
        Matcher match = fatal.matcher(text);
        String line = "";
        while (match.find()) line = match.group();
        return line;
    }
}
