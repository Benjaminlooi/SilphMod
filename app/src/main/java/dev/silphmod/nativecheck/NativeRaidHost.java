package dev.silphmod.nativecheck;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.SystemClock;

import dev.rootcatch.nativeengine.*;
import dev.rootcatch.root.RootShell;
import dev.rootcatch.ugphone.KecleonMovement;

import java.util.*;
import java.util.concurrent.*;
import java.util.function.BooleanSupplier;

final class NativeRaidHost implements AutoCloseable {
    private final NativeSessionController controller;
    private final CheckLog log;
    private final BooleanSupplier active;
    private final SharedPreferences settings, history;
    private final RootShell joystick = new RootShell();
    private final RaidFeedClient feeds = new RaidFeedClient(SystemClock::elapsedRealtime);
    private final ScheduledExecutorService network = Executors.newSingleThreadScheduledExecutor(r -> new Thread(r, "silph-raid-feed"));
    private final ExecutorService worker = Executors.newSingleThreadExecutor(r -> new Thread(r, "silph-raid-route"));
    private final Set<String> completed = new LinkedHashSet<>();
    private final Map<String, Long> retry = new HashMap<>();
    private final Map<String, Integer> wins = new HashMap<>();
    private RaidTargetPlan plan;
    private RaidPending pending;
    private String pendingText = "";
    private volatile NativeKecleonWire.Event event;
    private volatile RaidFeed.Candidate candidate;
    private volatile String account = "";
    private volatile boolean closed;
    private volatile long candidateRequest;
    private boolean choosing;
    private int country;
    private long commandSequence, retryAt, throwsSeen, berriesSeen;
    private volatile String waiting = "";
    private String waitSignature = "";
    private long waitLoggedAt = -30000;
    private volatile NativeSessionWire.Snapshot latest;
    private volatile CaptureTrace capture;
    private volatile double captureLatitude, captureLongitude;
    private volatile long captureAuthorized, captureAuthorizationSequence;
    private final RaidLocalSearch local;
    private boolean surveyPending;
    private long surveySequence, surveyUntil;
    private long proxyGeneration = -1, recoverySaved;

    private static final class CaptureTrace {
        final long request, caught, fled, throwsCount, berries, raids, encounters;
        boolean logged, uncertain;

        CaptureTrace(long request, NativeSessionWire.Snapshot value) {
            this.request = request;
            caught = value == null ? 0 : value.caught;
            fled = value == null ? 0 : value.fled;
            throwsCount = value == null ? 0 : value.throwsCount;
            berries = value == null ? 0 : value.berries;
            raids = value == null ? 0 : value.spun;
            encounters = value == null ? 0 : value.encounters;
        }
    }

    private final RaidTargetLabels labels;
    private final int location;
    private final RaidSearchArea area;
    private double latitude, longitude;
    private volatile double lastLatitude, lastLongitude;
    private volatile long lastAction;

    NativeRaidHost(Context context, NativeSessionController controller, CheckLog log, BooleanSupplier active) {
        this.controller = controller;
        this.log = log;
        this.active = active;
        labels = new RaidTargetLabels(context);
        settings = new NativeSettings(context).prefs;
        history = context.getSharedPreferences("raid-history", 0);
        plan = RaidTargetPlan.parse(settings.getString("raid.targets", ""));
        RaidTime.init(context);
        RaidTime.refresh();
        location = RaidLocationPrefs.selected(settings);
        area = new RaidSearchArea(RaidLocationPrefs.reference(settings, location), RaidLocationPrefs.radius(settings, location));
        local = new RaidLocalSearch(location, area);
        RaidLocations.Location city = RaidLocations.get(location);
        log.write("raid_location", "id=" + location + " city=" + city.name + " zone=" + city.timezone + " reference=" + area.latitude + "," + area.longitude + " radius=" + area.radius);
        log.write("raid_plan", "targets=" + settings.getString("raid.targets", "") + " totalRemaining=" + RaidProgress.remaining(settings) + " next=" + plan.current(wins));
        network.scheduleWithFixedDelay(this::refresh, 0, 60, TimeUnit.SECONDS);
    }

    static void clear(Context context) {
        RaidActionHistory.clear(context.getSharedPreferences("raid-history", 0), new NativeSettings(context).prefs);
    }

    private void refresh() {
        int c = location;
        if (!closed) try {
            RaidFeedClient.Result result = feeds.refresh(c, RaidSources.url(c));
            if (result.fetched) {
                long generation = RaidProxy.generation();
                if (proxyGeneration != generation) {
                    proxyGeneration = generation;
                    log.write("raid_proxy", "installation=" + RaidProxy.installation() + " sessionGeneration=" + generation);
                }
                log.write("raid_feed", "country=" + c + " source=proxy HTTP=" + result.status + " rows=" + result.snapshot.rows + " candidates=" + result.snapshot.candidates.size() + " expired=" + result.snapshot.expired + " invalid=" + result.snapshot.invalid + " unsupported=" + result.snapshot.unsupported + " duplicates=" + result.snapshot.duplicates + " serverMs=" + result.snapshot.serverTimeMs + " receivedElapsed=" + result.snapshot.receivedElapsedMs + " feedAgeMs=" + result.ageMs + " requestMs=" + result.requestMs);
                synchronized (this) {
                    if (candidate == null) retryAt = 0;
                }
                tick();
            }
        } catch (Exception error) {
            if (!closed) log.write("raid_feed_error", "country=" + c + " " + error.getMessage());
        }
    }

    private synchronized void rememberRecovery(long request) {
        if (recoverySaved == request || candidate == null || candidateRequest != request) return;
        recoverySaved = request;
        long utc = RaidTime.now();
        if (utc > 0)
            history.edit().putString("retry." + account, candidate.key).putLong("retryAt." + account, utc + 60000).apply();
        log.write("raid_map_recovery", "request=" + request + " pendingEncounterPreserved=" + (pending != null));
    }

    void snapshot(NativeSessionWire.Snapshot value) {
        latest = value;
        if (closed || account.isEmpty()) return;
        if (value.candidateState == 8 && value.reason == 21)
            rememberRecovery(value.candidateRequest);
        CaptureTrace trace = capture;
        if (trace != null && !trace.logged && value.candidateRequest == trace.request && (value.candidateState == 705 || value.candidateState == 7) && (value.spun > trace.raids || value.encounters > trace.encounters)) {
            trace.logged = true;
            String outcome = value.caught > trace.caught ? "caught" : value.reason == 2 ? "balls_exhausted" : value.fled > trace.fled ? "server_fled" : value.reason == 4 ? "filtered" : "unconfirmed";
            log.write("raid_capture_result", "request=" + trace.request + " encounter=" + Long.toUnsignedString(value.encounterId) + " outcome=" + outcome + " throws=" + (value.throwsCount - trace.throwsCount) + " berries=" + (value.berries - trace.berries) + " captured=" + Long.toUnsignedString(value.capturedId));
        }
        boolean uncertain = trace != null && !trace.uncertain && value.candidateRequest == trace.request && captureAuthorized == trace.request && value.candidateState == 8 && value.reason == 11;
        if (value.throwsCount > throwsSeen || value.berries > berriesSeen || uncertain) {
            if (uncertain) trace.uncertain = true;
            throwsSeen = value.throwsCount;
            berriesSeen = value.berries;
            RaidFeed.Candidate current = candidate;
            if (current == null || candidateRequest != value.candidateRequest) return;
            long at = RaidTime.now();
            if (at < 0) {
                log.write("raid_clock_wait", "Capture action awaits trusted time");
                return;
            }
            lastLatitude = captureAuthorized == candidateRequest ? captureLatitude : current.latitude;
            lastLongitude = captureAuthorized == candidateRequest ? captureLongitude : current.longitude;
            lastAction = at;
            RaidActionHistory.record(history, account, lastAction, lastLatitude, lastLongitude);
            log.write("raid_cooldown", "capture action recorded request=" + value.candidateRequest + " at=" + lastAction + " latitude=" + lastLatitude + " longitude=" + lastLongitude + " throws=" + throwsSeen + " berries=" + berriesSeen + " uncertain=" + uncertain);
        }
    }

    String waiting() {
        return waiting;
    }

    synchronized void accept(NativeKecleonWire.Event value) {
        if (closed) return;
        String key = Long.toUnsignedString(value.accountHi, 16) + ":" + Long.toUnsignedString(value.accountLo, 16);
        if (!account.isEmpty() && !account.equals(key)) {
            controller.requestStop("Raid account changed");
            return;
        }
        if (account.isEmpty()) {
            account = key;
            history.edit().putString("activeAccount", account).apply();
            completed.addAll(history.getStringSet("done." + account, Collections.emptySet()));
            lastAction = history.getLong("time." + account, 0);
            local.load(history.getString("visits." + account + "." + location, ""));
            local.seed(completed);
            String prefix = "wins." + account + ".";
            for (Map.Entry<String, ?> row : history.getAll().entrySet())
                if (row.getKey().startsWith(prefix) && row.getValue() instanceof Integer)
                    wins.put(row.getKey().substring(prefix.length()), (Integer) row.getValue());
            String retryKey = history.getString("retry." + account, "");
            long retryUtc = history.getLong("retryAt." + account, 0), utcNow = RaidTime.now();
            if (!retryKey.isEmpty() && utcNow > 0 && retryUtc > utcNow && retryUtc - utcNow <= 180000)
                retry.put(retryKey, SystemClock.elapsedRealtime() + retryUtc - utcNow);
            pendingText = history.getString("pending." + account, "");
            String point = history.getString("location." + account, "");
            long supplied = RaidTime.manualAt(settings);
            if (supplied > lastAction) {
                lastAction = supplied;
                point = settings.getString("raid.lastActionCoordinates", "");
            }
            if (!point.isBlank()) {
                double[] p = RaidActionHistory.coordinates(point);
                lastLatitude = p[0];
                lastLongitude = p[1];
                if (supplied == lastAction && supplied > 0)
                    RaidActionHistory.record(history, account, lastAction, lastLatitude, lastLongitude);
            }
        }
        event = value;
        if (value.phase == 1 && value.move != 2) {
            latitude = value.latitude;
            longitude = value.longitude;
        }
        submit(() -> process(value));
    }

    private interface Job {
        void run() throws Exception;
    }

    private void submit(Job job) {
        if (closed) return;
        try {
            worker.execute(() -> {
                try {
                    if (!closed) job.run();
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                } catch (Exception e) {
                    if (!closed) {
                        log.write("raid_host_error", e.toString());
                        controller.requestStop("Raid controller: " + e.getMessage());
                    }
                }
            });
        } catch (RejectedExecutionException ignored) {
        }
    }

    synchronized void tick() {
        if (!closed && event != null && (event.phase == 1 || event.phase == 4 && captureAuthorizationSequence != event.sequence) && !choosing && (retryAt == 0 || SystemClock.elapsedRealtime() >= retryAt)) {
            choosing = true;
            NativeKecleonWire.Event e = event;
            submit(() -> {
                try {
                    process(e);
                } finally {
                    synchronized (this) {
                        choosing = false;
                    }
                }
            });
        }
    }

    private synchronized void send(NativeKecleonWire.Event e, int kind, boolean accepted, double lat, double lon, String source) {
        if (closed || event == null || event.request != e.request) return;
        controller.enqueue(NativeKecleonWire.command(e, ++commandSequence, kind, 0, accepted, lat, lon, source));
        log.write("raid_command", "request=" + e.request + " kind=" + kind + " sequence=" + commandSequence);
    }

    private void process(NativeKecleonWire.Event e) throws Exception {
        if (closed || event == null || event.request != e.request) return;
        boolean surveyRow = false;
        if (e.phase == 1) {
            if (e.sequence != event.sequence) return;
            if (e.move == 2) {
                if (!surveyPending || e.sequence == surveySequence) return;
                surveyRow = true;
                surveySequence = e.sequence;
                long utc = RaidTime.now();
                try {
                    String[] p = e.source.split(":", -1);
                    if (p.length != 6 || e.verified != 1 || Long.parseUnsignedLong(e.incident) == 0)
                        throw new IllegalArgumentException("Invalid local raid identity");
                    RaidFeed.Candidate observed = RaidFeed.local(location, e.fort, e.latitude, e.longitude, Long.parseLong(p[4]), Integer.parseInt(p[0]), Integer.parseInt(p[1]), Integer.parseInt(p[2]), Integer.parseInt(p[3]), Integer.parseInt(p[5]), utc);
                    local.observe(observed, SystemClock.elapsedRealtime());
                    log.write("raid_local_observed", "gym=" + e.fort + " target=" + RaidTargetPlan.key(observed) + " latitude=" + e.latitude + " longitude=" + e.longitude + " remainingMs=" + (observed.expiresMs - utc));
                } catch (IllegalArgumentException error) {
                    log.write("raid_local_ignored", error.getMessage());
                }
            }
            if (e.move == 1 && e.sequence != surveySequence) {
                surveySequence = e.sequence;
                surveyPending = false;
                retryAt = 0;
                log.write("raid_local_scan", "completed reason=" + e.reason);
            }
            if (surveyPending && !surveyRow) {
                if (SystemClock.elapsedRealtime() < surveyUntil) return;
                surveyPending = false;
                log.write("raid_local_scan", "host wait expired; deferring scan");
            }
        }
        RaidFeed.Candidate current = candidate;
        boolean victory = e.verified == 1 && (e.phase == 4 || e.phase == 5 || e.phase == 7) && current != null && candidateRequest == e.request && !e.fort.isEmpty();
        if (victory && (pending == null || !pending.candidate.key.equals(current.key))) {
            pending = new RaidPending(current, e.fort);
            history.edit().putString("pending." + account, pending.encode()).apply();
        }
        if (victory && !completed.contains(current.key)) {
            String target = plan.target(current);
            if (target.isEmpty()) target = RaidTargetPlan.key(current);
            RaidProgress.credit(settings, account, current.key, target);
            plan = RaidTargetPlan.parse(settings.getString("raid.targets", ""));
            completed.add(current.key);
            completed.add(RaidLocalSearch.identityKey(current));
            completed.add(RaidLocalSearch.gymKey(location, e.fort, current.expiresMs));
            local.visit(current.latitude, current.longitude);
            history.edit().putString("visits." + account + "." + location, local.encode()).apply();
            while (completed.size() > 5000) {
                Iterator<String> old = completed.iterator();
                old.next();
                old.remove();
            }
            int count = wins.getOrDefault(target, 0) + 1;
            wins.put(target, count);
            history.edit().putStringSet("done." + account, new HashSet<>(completed)).putInt("wins." + account + "." + target, count).apply();
            log.write("raid_target_progress", "target=" + target + " victories=" + count + " remaining=" + plan.limits.getOrDefault(target, 0) + " totalRemaining=" + RaidProgress.remaining(settings));
        }
        if (e.phase == 1) {
            synchronized (this) {
                if (candidate != null) return;
            }
            long utc = RaidTime.now();
            if (utc < 0) {
                RaidTime.refresh();
                if (retryAt == 0 || SystemClock.elapsedRealtime() >= retryAt + 25000)
                    log.write("raid_clock_wait", "Waiting for trusted raid time");
                retryAt = SystemClock.elapsedRealtime() + 5000;
                if (surveyRow) send(e, 6, false, 0, 0, "");
                return;
            }
            if (!pendingText.isEmpty()) {
                try {
                    pending = RaidPending.decode(pendingText, utc);
                } catch (Exception error) {
                    log.write("raid_pending_error", error.getMessage());
                }
                pendingText = "";
                if (pending == null) history.edit().remove("pending." + account).apply();
            }
            RaidFeed.Candidate best = null;
            long elapsed = SystemClock.elapsedRealtime();
            RaidPending resume = pending;
            if (resume != null && resume.candidate.country == location && resume.candidate.expiresMs > utc)
                best = resume.candidate;
            boolean recovering = best != null;
            if (best == null && plan.reached(wins)) {
                controller.requestStop("Selected raid targets completed");
                return;
            }
            RaidSelection.Result selection = null;
            boolean observed = false;
            if (!recovering) {
                selection = RaidSelection.choose(feeds.snapshot(location), plan, wins, completed, retry, area, elapsed, utc, lastAction, lastLatitude, lastLongitude);
                best = selection.candidate;
                if (best == null) {
                    RaidSelection.Result found = RaidSelection.choose(local.snapshot(utc, elapsed, completed), plan, wins, completed, retry, area, elapsed, utc, lastAction, lastLatitude, lastLongitude);
                    best = found.candidate;
                    observed = best != null;
                }
            }
            if (best == null) {
                waiting = selection.reason + " · " + labels.label(selection.target);
                String signature = selection.signature();
                if (!signature.equals(waitSignature) || elapsed - waitLoggedAt >= 30000) {
                    waitSignature = signature;
                    waitLoggedAt = elapsed;
                    log.write("raid_wait", "target=" + plan.current(wins) + " reason=" + selection.reason + " " + selection.summary() + " variants=" + RaidSelection.variants(feeds.snapshot(location)));
                }
                retryAt = elapsed + 2500;
                if (surveyRow) send(e, 6, false, 0, 0, "");
                else if (active.getAsBoolean() && !plan.current(wins).isEmpty())
                    searchLocal(e, elapsed);
                return;
            }
            surveyPending = false;
            waiting = "";
            waitSignature = "";
            synchronized (this) {
                if (candidate != null) return;
                candidateRequest = e.request;
                candidate = best;
                country = best.country;
                captureAuthorized = 0;
                capture = new CaptureTrace(e.request, latest);
            }
            int filters = settings.getBoolean("raid.catchAll", true) ? 0 : settings.getInt("raid.filters", 0), players = settings.getInt("raid.minPlayers", 2);
            if (RaidEventPolicy.group(best.species)) filters |= 32;
            String descriptor = best.species + ":" + best.level + ":" + best.form + ":" + best.costume + ":" + best.expiresMs + ":" + filters + ":" + players + ":" + settings.getInt("raid.exitRemainingSeconds", 20) + ":" + best.temporaryEvolution;
            if (resume != null && best.key.equals(resume.candidate.key)) {
                descriptor += ":" + resume.gym;
                if (descriptor.length() > 96) {
                    controller.requestStop("Pending raid encounter needs manual recovery");
                    return;
                }
                log.write("raid_resume", "Recovering won encounter without another pass");
            }
            send(e, 1, false, best.latitude, best.longitude, descriptor);
            log.write("raid_candidate", "source=" + (observed ? "game_map" : "feed") + " target=" + plan.target(best) + " pending=" + plan.limits.getOrDefault(plan.target(best), 0) + " country=" + country + " species=" + best.species + " level=" + best.level + " latitude=" + best.latitude + " longitude=" + best.longitude + " remainingMs=" + (best.expiresMs - utc) + " distanceFromReferenceM=" + Math.round(area.distance(best)) + " distanceFromActionM=" + (lastAction == 0 ? -1 : Math.round(RaidFeed.distanceMeters(lastLatitude, lastLongitude, best.latitude, best.longitude))) + " sinceActionMs=" + (lastAction == 0 ? -1 : utc - lastAction));
        } else if (e.phase == 4) {
            if (current == null || candidateRequest != e.request || captureAuthorizationSequence == e.sequence)
                return;
            long utc = RaidTime.now(), elapsed = SystemClock.elapsedRealtime();
            double gymDistance = RaidFeed.distanceMeters(e.latitude, e.longitude, current.latitude, current.longitude);
            double distance = lastAction == 0 ? 0 : RaidFeed.distanceMeters(lastLatitude, lastLongitude, e.latitude, e.longitude);
            long wait = utc < 0 ? -1 : lastAction == 0 ? 0 : RaidCooldown.remainingMillis(distance, utc - lastAction);
            if (!active.getAsBoolean() || gymDistance > 50 || wait != 0) {
                waiting = !active.getAsBoolean() ? "Paused" : utc < 0 ? "Waiting for trusted capture time" : gymDistance > 50 ? "Waiting for raid capture location" : "Waiting for capture cooldown";
                if (utc < 0) RaidTime.refresh();
                if (elapsed - waitLoggedAt >= 5000) {
                    waitLoggedAt = elapsed;
                    log.write("raid_capture_wait", "request=" + e.request + " remainingMs=" + wait + " actionM=" + Math.round(distance) + " gymM=" + Math.round(gymDistance));
                }
                retryAt = elapsed + 500;
                return;
            }
            captureLatitude = e.latitude;
            captureLongitude = e.longitude;
            captureAuthorized = e.request;
            captureAuthorizationSequence = e.sequence;
            waiting = "";
            retryAt = 0;
            log.write("raid_capture_ready", "request=" + e.request + " latitude=" + e.latitude + " longitude=" + e.longitude + " actionM=" + Math.round(distance) + " sinceActionMs=" + (lastAction == 0 ? -1 : utc - lastAction) + " referenceM=" + Math.round(area.distance(current)));
            send(e, 3, true, 0, 0, "");
        } else if (e.phase == 2 || e.phase == 6) {
            RaidFeed.Candidate target = candidate;
            if (target == null) return;
            while (!closed && !active.getAsBoolean()) Thread.sleep(100);
            if (closed) return;
            boolean accepted = true;
            double[][] points = e.phase == 6 ? KecleonMovement.points(target.latitude, target.longitude) : new double[][]{{target.latitude, target.longitude}};
            for (int i = 0; i < points.length && !closed; i++) {
                while (!closed && !active.getAsBoolean()) Thread.sleep(100);
                if (closed) return;
                RootShell.Result result = joystick.execute(KecleonMovement.command(points[i][0], points[i][1]), 3000);
                String lower = result.output.toLowerCase(Locale.ROOT);
                accepted = result.code == 0 && !lower.contains("error") && !lower.contains("exception") && !lower.contains("not found");
                if (!accepted) break;
                if (i + 1 < points.length) Thread.sleep(KecleonMovement.STEP_MS);
            }
            synchronized (this) {
                if (!closed && event != null && event.request == e.request)
                    controller.enqueue(NativeKecleonWire.command(e, ++commandSequence, 2, e.move, accepted, 0, 0, ""));
            }
            log.write("raid_move", "accepted=" + accepted + " request=" + e.request + " radius=" + (e.phase == 6 ? 8 : 0));
        } else if (e.phase == 8 && e.reason == 21 && current != null && candidateRequest == e.request) {
            rememberRecovery(e.request);
        } else if (e.phase == 7) {
            if (victory && pending != null && pending.candidate.key.equals(candidate.key)) {
                pending = null;
                history.edit().remove("pending." + account).apply();
            }
            synchronized (this) {
                if (candidate == null) return;
                if (e.reason == 5) {
                    completed.add(candidate.key);
                    completed.add(RaidLocalSearch.identityKey(candidate));
                    if (!e.fort.isEmpty())
                        completed.add(RaidLocalSearch.gymKey(location, e.fort, candidate.expiresMs));
                    while (completed.size() > 5000) {
                        Iterator<String> old = completed.iterator();
                        old.next();
                        old.remove();
                    }
                    history.edit().putStringSet("done." + account, new HashSet<>(completed)).apply();
                    log.write("raid_skipped", "already completed; request=" + e.request);
                } else if (e.verified != 1) {
                    long after = SystemClock.elapsedRealtime() + (e.reason == 7 ? 180000 : 60000);
                    retry.put(candidate.key, after);
                    retry.put(RaidLocalSearch.identityKey(candidate), after);
                }
                retry.entrySet().removeIf(row -> row.getValue() <= SystemClock.elapsedRealtime());
                candidate = null;
                candidateRequest = 0;
                retryAt = 0;
            }
            if (plan.reached(wins)) {
                controller.requestStop(pending == null ? "Selected raid targets completed" : "Pending raid encounter could not be reopened");
                return;
            }
            send(e, 4, false, 0, 0, "");
        }
    }

    private void searchLocal(NativeKecleonWire.Event e, long elapsed) throws InterruptedException {
        if (local.scanDue(elapsed)) {
            local.scanned(elapsed);
            surveyPending = true;
            surveyUntil = elapsed + 18000;
            send(e, 6, false, 0, 0, "");
            log.write("raid_local_scan", "requested latitude=" + latitude + " longitude=" + longitude);
            return;
        }
        RaidLocalSearch.Point point = local.next(latitude, longitude, elapsed);
        if (point == null) return;
        try {
            RootShell.Result result = joystick.execute(KecleonMovement.command(point.latitude, point.longitude), 3000);
            String output = result.output.toLowerCase(Locale.ROOT);
            boolean accepted = result.code == 0 && !output.contains("error") && !output.contains("exception") && !output.contains("not found");
            if (accepted) {
                latitude = point.latitude;
                longitude = point.longitude;
                local.arrived(SystemClock.elapsedRealtime());
                waiting = "Checking nearby raids during feed update";
            }
            log.write("raid_local_move", "accepted=" + accepted + " latitude=" + point.latitude + " longitude=" + point.longitude);
        } catch (Exception error) {
            if (error instanceof InterruptedException) throw (InterruptedException) error;
            log.write("raid_local_move_error", error.toString());
        }
    }

    @Override
    public void close() {
        closed = true;
        feeds.close();
        network.shutdownNow();
        worker.shutdownNow();
        joystick.close();
    }
}
