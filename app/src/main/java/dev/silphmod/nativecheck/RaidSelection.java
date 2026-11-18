package dev.silphmod.nativecheck;

import java.util.Map;
import java.util.Set;

final class RaidSelection {
    static final long MIN_REMAINING_MS = 120000;

    static final class Result {
        RaidFeed.Candidate candidate;
        int total, matching, completed, retrying, ending, outside, cooling;
        long waitMs = Long.MAX_VALUE, feedAgeMs, captureWaitMs;
        double referenceMeters = -1, actionMeters = -1;
        String reason = "Waiting for raid list", target = "";

        String signature() {
            return reason + ":" + total + ":" + matching + ":" + completed + ":" + retrying + ":" + ending + ":" + outside + ":" + cooling;
        }

        String summary() {
            return "available=" + total + " matching=" + matching + " completed=" + completed + " retrying=" + retrying + " ending=" + ending + " outside=" + outside + " cooling=" + cooling + " nextMs=" + (waitMs == Long.MAX_VALUE ? -1 : waitMs) + " feedAgeMs=" + feedAgeMs + " referenceM=" + Math.round(referenceMeters) + " actionM=" + Math.round(actionMeters);
        }
    }

    static Result choose(RaidFeed.Snapshot snapshot, RaidTargetPlan plan, Map<String, Integer> wins, Set<String> completed, Map<String, Long> retry, RaidSearchArea area, long elapsed, long utc, long lastAction, double lastLatitude, double lastLongitude) {
        Result first = null;
        for (Map.Entry<String, Integer> goal : plan.limits.entrySet())
            if (goal.getValue() > 0) {
                Result result = chooseTarget(snapshot, goal.getKey(), completed, retry, area, elapsed, utc, lastAction, lastLatitude, lastLongitude);
                if (result.candidate != null) return result;
                if (first == null || first.matching == 0 && result.matching > 0) first = result;
            }
        if (first != null) return first;
        Result out = new Result();
        out.reason = "Select raid targets to continue";
        return out;
    }

    private static Result chooseTarget(RaidFeed.Snapshot snapshot, String target, Set<String> completed, Map<String, Long> retry, RaidSearchArea area, long elapsed, long utc, long lastAction, double lastLatitude, double lastLongitude) {
        Result out = new Result();
        out.target = target;
        if (snapshot == null) return out;
        long now = snapshot.nowMs(elapsed);
        if (now < 0) {
            out.reason = "Waiting for fresh raid list";
            return out;
        }
        out.total = snapshot.candidates.size();
        out.feedAgeMs = elapsed - snapshot.receivedElapsedMs;
        double score = Double.POSITIVE_INFINITY;
        int[] requested = RaidTargetPlan.parts(target);
        for (RaidFeed.Candidate item : snapshot.candidates) {
            if (!RaidEventPolicy.matches(item, requested)) continue;
            out.matching++;
            if (completed.contains(item.key) || completed.contains(RaidLocalSearch.identityKey(item))) {
                out.completed++;
                continue;
            }
            long after = Math.max(retry.getOrDefault(item.key, 0L), retry.getOrDefault(RaidLocalSearch.identityKey(item), 0L));
            if (after > elapsed) {
                out.retrying++;
                out.waitMs = Math.min(out.waitMs, after - elapsed);
                continue;
            }
            if (item.expiresMs - now < MIN_REMAINING_MS) {
                out.ending++;
                continue;
            }
            if (!area.includes(item)) {
                out.outside++;
                continue;
            }
            double fromAction = RaidFeed.distanceMeters(lastLatitude, lastLongitude, item.latitude, item.longitude);
            long wait = lastAction == 0 ? 0 : RaidCooldown.remainingMillis(fromAction, utc - lastAction);
            if (wait > 0) out.cooling++;
            double distance = area.distance(item);
            if (distance < score || distance == score && (out.candidate == null || item.expiresMs < out.candidate.expiresMs)) {
                out.candidate = item;
                score = distance;
                out.captureWaitMs = wait;
                out.referenceMeters = distance;
                out.actionMeters = lastAction == 0 ? -1 : fromAction;
            }
        }
        if (out.candidate != null && out.captureWaitMs > Math.min(180000, out.candidate.expiresMs - now - 45000)) {
            out.candidate = null;
            out.waitMs = out.captureWaitMs;
            out.reason = "Waiting for cooldown near the reference coordinates";
            return out;
        }
        if (out.candidate != null) out.reason = "";
        else if (out.cooling > 0) out.reason = "Waiting for cooldown before the next raid";
        else if (out.retrying > 0) out.reason = "Waiting to retry selected raids";
        else if (out.outside > 0)
            out.reason = "Waiting for selected raids within the reference radius";
        else if (out.ending > 0) out.reason = "Selected raids are ending too soon";
        else if (out.matching > 0 && out.completed == out.matching)
            out.reason = "Waiting for new raids of the selected target";
        else out.reason = "Waiting for selected raid targets";
        return out;
    }

    static String variants(RaidFeed.Snapshot snapshot) {
        if (snapshot == null) return "[]";
        Map<String, Integer> values = new java.util.LinkedHashMap<>();
        for (RaidFeed.Candidate item : snapshot.candidates) {
            String key = RaidEventPolicy.key(RaidTargetPlan.key(item));
            if (values.containsKey(key) || values.size() < 32) values.merge(key, 1, Integer::sum);
        }
        return values.toString();
    }

    private RaidSelection() {
    }
}
