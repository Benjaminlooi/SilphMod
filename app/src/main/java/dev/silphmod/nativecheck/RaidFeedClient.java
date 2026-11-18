package dev.silphmod.nativecheck;

import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URI;
import java.net.URISyntaxException;
import java.nio.charset.StandardCharsets;
import java.util.function.LongSupplier;
import java.util.HashMap;
import java.util.Map;
import java.util.zip.GZIPInputStream;

import javax.net.ssl.HttpsURLConnection;

public final class RaidFeedClient implements AutoCloseable {
    private static final long MAX_BYTES = 8388608;
    private final LongSupplier clock;
    private final Entry[] entries = new Entry[RaidLocations.COUNT];
    private volatile boolean closed;
    private volatile HttpsURLConnection active;

    private static final class Entry {
        String url = "", etag = "";
        long attempted = -1, nextAttempt, validUntil;
        volatile RaidFeed.Snapshot snapshot;
    }

    public static final class Result {
        public final RaidFeed.Snapshot snapshot;
        public final int status;
        public final boolean fetched, revalidated;
        public final long ageMs, requestMs;

        private Result(RaidFeed.Snapshot snapshot, int status, boolean fetched, boolean revalidated, long ageMs, long requestMs) {
            this.snapshot = snapshot;
            this.status = status;
            this.fetched = fetched;
            this.revalidated = revalidated;
            this.ageMs = ageMs;
            this.requestMs = requestMs;
        }
    }

    public RaidFeedClient(LongSupplier clock) {
        if (clock == null) throw new IllegalArgumentException("Missing raid feed clock");
        this.clock = clock;
        for (int i = 0; i < entries.length; i++) entries[i] = new Entry();
    }

    public static URI validateUrl(String value) throws IOException {
        try {
            URI uri = new URI(value);
            if (!"https".equalsIgnoreCase(uri.getScheme()) || uri.getHost() == null || uri.getRawUserInfo() != null || uri.getFragment() != null || uri.getHost().length() > 253 || value.length() > 2048)
                throw new IOException("Invalid raid feed URL");
            return uri;
        } catch (URISyntaxException | NullPointerException invalid) {
            throw new IOException("Invalid raid feed URL");
        }
    }

    private static final class Limited extends FilterInputStream {
        long remaining = MAX_BYTES;

        Limited(InputStream in) {
            super(in);
        }

        @Override
        public int read() throws IOException {
            int value = super.read();
            if (value >= 0 && --remaining < 0)
                throw new IOException("Raid feed response too large");
            return value;
        }

        @Override
        public int read(byte[] bytes, int offset, int count) throws IOException {
            int length = in.read(bytes, offset, (int) Math.min(count, Math.max(1, remaining + 1)));
            if (length > 0 && (remaining -= length) < 0)
                throw new IOException("Raid feed response too large");
            return length;
        }
    }

    public synchronized Result refresh(int country, String url) throws IOException {
        if (closed) throw new IOException("Raid feed closed");
        if (!RaidLocations.valid(country)) throw new IOException("Invalid raid country");
        URI uri = validateUrl(url);
        Entry entry = entries[country - 1];
        long elapsed = clock.getAsLong();
        if (elapsed < 0) throw new IOException("Invalid raid feed clock");
        if (!url.equals(entry.url)) {
            entry.url = url;
            entry.etag = "";
            entry.snapshot = null;
            entry.attempted = -1;
            entry.nextAttempt = entry.validUntil = 0;
        }
        if (elapsed < entry.nextAttempt || entry.attempted >= 0 && elapsed >= entry.attempted && elapsed - entry.attempted < RaidFeed.REFRESH_MS)
            return new Result(fresh(entry, elapsed), 0, false, false, 0, 0);
        entry.attempted = elapsed;
        entry.nextAttempt = elapsed + RaidFeed.REFRESH_MS;
        HttpsURLConnection connection = null;
        try {
            if (closed) throw new IOException("Raid feed closed");
            Map<String, String> headers = new HashMap<>();
            headers.put("Accept-Encoding", "gzip");
            headers.put("Cache-Control", "no-cache");
            if (!entry.etag.isEmpty()) headers.put("If-None-Match", entry.etag);
            connection = RaidProxy.open(uri.toString(), 8000, headers, () -> closed, c -> active = c);
            int status = connection.getResponseCode();
            long received = clock.getAsLong();
            if (status != HttpsURLConnection.HTTP_OK && status != HttpsURLConnection.HTTP_NOT_MODIFIED)
                throw RaidProxy.failure(connection, status);
            long server = serverTime(connection), age = responseAge(connection, server);
            RaidTime.observe(server, received);
            if (received < elapsed) throw new IOException("Invalid raid feed clock");
            if (status == HttpsURLConnection.HTTP_NOT_MODIFIED && entry.snapshot != null) {
                try {
                    entry.snapshot = entry.snapshot.revalidated(server, received);
                } catch (IllegalArgumentException invalid) {
                    throw new IOException("Invalid raid feed response clock");
                }
                entry.validUntil = received + Math.max(0, RaidFeed.MAX_AGE_MS - age);
                return new Result(entry.snapshot, status, true, true, age, received - elapsed);
            }
            if (status != HttpsURLConnection.HTTP_OK)
                throw new IOException("Raid feed HTTP " + status);
            long length = connection.getContentLengthLong();
            if (length > MAX_BYTES) throw new IOException("Raid feed response too large");
            String encoding = connection.getContentEncoding();
            if (encoding != null && !encoding.equalsIgnoreCase("gzip") && !encoding.equalsIgnoreCase("identity"))
                throw new IOException("Unsupported raid feed encoding");
            try (InputStream raw = new Limited(connection.getInputStream()); InputStream decoded = new Limited("gzip".equalsIgnoreCase(encoding) ? new GZIPInputStream(raw) : raw)) {
                RaidFeed.Snapshot snapshot = RaidFeed.read(new InputStreamReader(decoded, StandardCharsets.UTF_8), country, server, received);
                if (closed) throw new IOException("Raid feed closed");
                entry.snapshot = snapshot;
            }
            entry.etag = header(connection, "ETag");
            entry.validUntil = received + Math.max(0, RaidFeed.MAX_AGE_MS - age);
            return new Result(entry.snapshot, status, true, false, age, clock.getAsLong() - elapsed);
        } catch (RaidProxy.Failure error) {
            entry.nextAttempt = Math.max(entry.nextAttempt, clock.getAsLong() + error.retryMs);
            throw error;
        } finally {
            active = null;
            if (connection != null) connection.disconnect();
        }
    }

    public RaidFeed.Snapshot snapshot(int country) {
        if (closed || !RaidLocations.valid(country)) return null;
        Entry entry = entries[country - 1];
        return entry == null ? null : fresh(entry, clock.getAsLong());
    }

    private static RaidFeed.Snapshot fresh(Entry entry, long elapsed) {
        return entry.snapshot != null && elapsed <= entry.validUntil && entry.snapshot.nowMs(elapsed) >= 0 ? entry.snapshot : null;
    }

    private static long responseAge(HttpsURLConnection connection, long server) throws IOException {
        try {
            String raw = connection.getHeaderField("X-Silph-Feed-Checked-At");
            if (raw == null || !raw.matches("[0-9]{13}")) throw new NumberFormatException();
            long checked = Long.parseLong(raw);
            if (checked < 1577836800000L || checked > server + 5000 || server - checked > RaidFeed.MAX_AGE_MS)
                throw new NumberFormatException();
            return Math.max(0, server - checked);
        } catch (NumberFormatException invalid) {
            throw new IOException("Invalid raid feed response age");
        }
    }

    private static String header(HttpsURLConnection connection, String name) {
        String value = connection.getHeaderField(name);
        return value == null || value.length() > 512 ? "" : value;
    }

    static long serverTime(HttpsURLConnection connection) throws IOException {
        long server = connection.getDate(), age = 0;
        String value = connection.getHeaderField("Age");
        try {
            if (value != null) age = Long.parseLong(value);
        } catch (NumberFormatException invalid) {
            throw new IOException("Invalid raid feed age");
        }
        if (server < 1577836800000L || server > 4102444800000L || age < 0 || age > 86400)
            throw new IOException("Invalid raid feed response clock");
        return server + age * 1000;
    }

    @Override
    public void close() {
        closed = true;
        HttpsURLConnection connection = active;
        if (connection != null) connection.disconnect();
    }
}
