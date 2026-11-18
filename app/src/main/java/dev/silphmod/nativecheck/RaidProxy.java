package dev.silphmod.nativecheck;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.SystemClock;
import android.provider.Settings;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.math.BigInteger;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.security.Signature;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECGenParameterSpec;
import java.util.Map;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

import javax.net.ssl.HttpsURLConnection;

import org.json.JSONObject;

final class RaidProxy {
    private static final Object LOCK = new Object();
    private static final SecureRandom RANDOM = new SecureRandom();
    private static final int BASE64 = Base64.URL_SAFE | Base64.NO_WRAP | Base64.NO_PADDING;
    private static SharedPreferences store;
    private static KeyStore.PrivateKeyEntry signing;
    private static JSONObject publicKey;
    private static Session session;
    private static int boot = -1;
    private static String scope = "", installation = "";
    private static long blockedUntil;
    private static volatile long generation;

    private static final class Session {
        final String value, id;
        final long until;

        Session(String value, String id, long until) {
            this.value = value;
            this.id = id;
            this.until = until;
        }
    }

    static final class Failure extends IOException {
        private static final long serialVersionUID = 1L;
        final int status;
        final long retryMs;

        Failure(int status, String reason, long retryMs) {
            super("Raid service HTTP " + status + " · " + reason);
            this.status = status;
            this.retryMs = retryMs;
        }
    }

    static void init(Context context) {
        synchronized (LOCK) {
            if (store != null) return;
            Context app = context.getApplicationContext();
            store = app.getSharedPreferences("raid-proxy", 0);
            boot = Settings.Global.getInt(app.getContentResolver(), Settings.Global.BOOT_COUNT, -1);
        }
    }

    static long generation() {
        return generation;
    }

    static String installation() {
        synchronized (LOCK) {
            return installation;
        }
    }

    private static String encode(byte[] value) {
        return Base64.encodeToString(value, BASE64);
    }

    private static String hash(String value) throws Exception {
        return encode(MessageDigest.getInstance("SHA-256").digest(value.getBytes(StandardCharsets.UTF_8)));
    }

    private static byte[] coordinate(BigInteger value) throws IOException {
        byte[] bytes = value.toByteArray();
        int start = bytes.length == 33 && bytes[0] == 0 ? 1 : 0, length = bytes.length - start;
        if (length > 32 || value.signum() < 0) throw new IOException("Invalid raid signing key");
        byte[] out = new byte[32];
        System.arraycopy(bytes, start, out, 32 - length, length);
        return out;
    }

    private static void key() throws IOException {
        if (signing != null) return;
        if (store == null) throw new IOException("Raid connection is not initialized");
        if (BuildConfig.RAID_API_BASE.isBlank())
            throw new IOException("Raid service is not configured");
        try {
            scope = hash(BuildConfig.RAID_API_BASE);
            String alias = "silph.raid." + scope.substring(0, 16);
            KeyStore keys = KeyStore.getInstance("AndroidKeyStore");
            keys.load(null);
            if (!keys.containsAlias(alias)) {
                KeyPairGenerator generator = KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, "AndroidKeyStore");
                generator.initialize(new KeyGenParameterSpec.Builder(alias, KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY).setAlgorithmParameterSpec(new ECGenParameterSpec("secp256r1")).setDigests(KeyProperties.DIGEST_SHA256).setUserAuthenticationRequired(false).build());
                generator.generateKeyPair();
            }
            signing = (KeyStore.PrivateKeyEntry) keys.getEntry(alias, null);
            ECPublicKey point = (ECPublicKey) signing.getCertificate().getPublicKey();
            String x = encode(coordinate(point.getW().getAffineX())), y = encode(coordinate(point.getW().getAffineY()));
            publicKey = new JSONObject().put("kty", "EC").put("crv", "P-256").put("x", x).put("y", y);
            installation = hash("{\"crv\":\"P-256\",\"kty\":\"EC\",\"x\":\"" + x + "\",\"y\":\"" + y + "\"}");
            if (boot >= 0 && store.getInt("boot", -2) == boot && scope.equals(store.getString("scope", ""))) {
                String value = store.getString("token", ""), id = store.getString("session", "");
                long until = store.getLong("until", -1), now = SystemClock.elapsedRealtime();
                if (value.matches("[A-Za-z0-9_-]{1,900}\\.[A-Za-z0-9_-]{43}") && id.matches("[A-Za-z0-9_-]{32}") && until > now + 30000 && until - now <= 900000)
                    session = new Session(value, id, until);
            }
        } catch (Exception error) {
            signing = null;
            throw new IOException("Raid signing key unavailable (" + error.getClass().getSimpleName() + ")");
        }
    }

    private static String sign(String message) throws IOException {
        try {
            Signature signature = Signature.getInstance("SHA256withECDSA");
            signature.initSign(signing.getPrivateKey());
            signature.update(message.getBytes(StandardCharsets.UTF_8));
            return encode(signature.sign());
        } catch (Exception error) {
            throw new IOException("Raid request signing unavailable (" + error.getClass().getSimpleName() + ")");
        }
    }

    private static HttpsURLConnection connection(String url, int timeout) throws IOException {
        try {
            URI base = URI.create(BuildConfig.RAID_API_BASE), target = URI.create(url);
            if (!"https".equals(base.getScheme()) || base.getHost() == null || !base.getHost().equalsIgnoreCase(target.getHost()) || !base.getScheme().equals(target.getScheme()) || target.getPort() != base.getPort() || target.getRawUserInfo() != null || target.getRawQuery() != null || target.getRawFragment() != null || !target.getRawPath().matches("/v1/(time|raids/[0-9]{1,2}|installations/(challenge|complete))"))
                throw new IOException("Invalid raid service destination");
            HttpsURLConnection c = (HttpsURLConnection) target.toURL().openConnection();
            c.setInstanceFollowRedirects(false);
            c.setConnectTimeout(timeout);
            c.setReadTimeout(timeout);
            c.setRequestProperty("Accept", "application/json");
            c.setRequestProperty("User-Agent", "SilphMod/AutoRaid");
            return c;
        } catch (IllegalArgumentException error) {
            throw new IOException("Invalid raid service configuration");
        }
    }

    private static String read(InputStream input, int limit) throws IOException {
        if (input == null) return "";
        try (InputStream in = input; ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[1024];
            for (int count; (count = in.read(buffer)) != -1; ) {
                if (out.size() + count > limit)
                    throw new IOException("Raid service response too large");
                out.write(buffer, 0, count);
            }
            return out.toString(StandardCharsets.UTF_8.name());
        }
    }

    static Failure failure(HttpsURLConnection c, int status) {
        long delay = 60000;
        String raw = c.getHeaderField("Retry-After");
        if (raw != null) try {
            long seconds = Long.parseLong(raw);
            if (seconds >= 0 && seconds <= 604800) delay = Math.max(1000, seconds * 1000);
        } catch (NumberFormatException ignored) {
            long at = c.getHeaderFieldDate("Retry-After", -1), server = c.getDate();
            if (at > 0 && server > 0 && at >= server)
                delay = Math.min(604800000, Math.max(1000, at - server));
        }
        String reason = "request_unavailable";
        try {
            String code = new JSONObject(read(c.getErrorStream(), 2048)).optString("error", "");
            if (code.matches("[a-z0-9_]{1,64}")) reason = code;
        } catch (Exception ignored) {
        }
        return new Failure(status, reason, delay);
    }

    private static JSONObject post(String path, JSONObject value, BooleanSupplier cancelled) throws IOException {
        if (cancelled.getAsBoolean() || Thread.currentThread().isInterrupted())
            throw new IOException("Raid connection cancelled");
        HttpsURLConnection c = connection(BuildConfig.RAID_API_BASE + path, 6000);
        try {
            c.setRequestMethod("POST");
            c.setDoOutput(true);
            c.setRequestProperty("Content-Type", "application/json");
            byte[] bytes = value.toString().getBytes(StandardCharsets.UTF_8);
            c.setFixedLengthStreamingMode(bytes.length);
            try (java.io.OutputStream out = c.getOutputStream()) {
                out.write(bytes);
            }
            int status = c.getResponseCode();
            if (status != 200) throw failure(c, status);
            return new JSONObject(read(c.getInputStream(), 8192));
        } catch (Failure error) {
            throw error;
        } catch (Exception error) {
            throw new IOException("Raid registration response unavailable (" + error.getClass().getSimpleName() + ")");
        } finally {
            c.disconnect();
        }
    }

    private static Session session(BooleanSupplier cancelled) throws IOException {
        synchronized (LOCK) {
            key();
            long now = SystemClock.elapsedRealtime();
            if (session != null && session.until > now + 30000) return session;
            if (now < blockedUntil) throw new Failure(429, "registration_wait", blockedUntil - now);
            try {
                JSONObject challenge = post("/v1/installations/challenge", new JSONObject().put("publicKey", publicKey), cancelled);
                String id = challenge.getString("installationId"), nonce = challenge.getString("challenge");
                if (!installation.equals(id) || !nonce.matches("[A-Za-z0-9_-]{32}"))
                    throw new IOException("Invalid raid registration challenge");
                JSONObject reply = post("/v1/installations/complete", new JSONObject().put("publicKey", publicKey).put("challenge", nonce).put("proof", sign("silph-register-v1\n" + id + "\n" + nonce)), cancelled);
                String value = reply.getString("accessToken");
                long ttl = reply.getLong("expiresAt") - reply.getLong("serverTime");
                String[] parts = value.split("\\.", -1);
                if (!id.equals(reply.getString("installationId")) || value.length() > 1024 || parts.length != 2 || ttl <= 30000 || ttl > 900000)
                    throw new IOException("Invalid raid connection session");
                JSONObject claims = new JSONObject(new String(Base64.decode(parts[0], BASE64), StandardCharsets.UTF_8));
                String sid = claims.getString("sid");
                if (!sid.matches("[A-Za-z0-9_-]{32}") || !id.equals(claims.getString("sub")))
                    throw new IOException("Invalid raid connection identity");
                session = new Session(value, sid, SystemClock.elapsedRealtime() + ttl);
                generation++;
                blockedUntil = 0;
                if (boot >= 0)
                    store.edit().putString("scope", scope).putString("token", value).putString("session", sid).putLong("until", session.until).putInt("boot", boot).apply();
                return session;
            } catch (Failure error) {
                blockedUntil = SystemClock.elapsedRealtime() + error.retryMs;
                throw error;
            } catch (IOException error) {
                blockedUntil = SystemClock.elapsedRealtime() + 30000;
                throw error;
            } catch (Exception error) {
                blockedUntil = SystemClock.elapsedRealtime() + 30000;
                throw new IOException("Raid connection session unavailable (" + error.getClass().getSimpleName() + ")");
            }
        }
    }

    private static void invalidate(Session old) {
        synchronized (LOCK) {
            if (session != old) return;
            session = null;
            if (store != null)
                store.edit().remove("token").remove("session").remove("until").apply();
        }
    }

    static HttpsURLConnection open(String url, int timeout, Map<String, String> headers, BooleanSupplier cancelled, Consumer<HttpsURLConnection> watch) throws IOException {
        for (int attempt = 0; attempt < 2; attempt++) {
            Session current = session(cancelled);
            if (cancelled.getAsBoolean() || Thread.currentThread().isInterrupted())
                throw new IOException("Raid connection cancelled");
            HttpsURLConnection c = connection(url, timeout);
            watch.accept(c);
            boolean returned = false;
            try {
                String path = URI.create(url).getRawPath();
                byte[] bytes = new byte[24];
                RANDOM.nextBytes(bytes);
                String nonce = encode(bytes);
                c.setRequestProperty("Authorization", "Bearer " + current.value);
                c.setRequestProperty("X-Silph-Nonce", nonce);
                c.setRequestProperty("X-Silph-Proof", sign("silph-request-v1\nGET\n" + path + "\n" + nonce + "\n" + current.id));
                for (Map.Entry<String, String> row : headers.entrySet())
                    c.setRequestProperty(row.getKey(), row.getValue());
                int status = c.getResponseCode();
                if ((status == 401 || status == 409) && attempt == 0) {
                    if (status == 401) invalidate(current);
                    continue;
                }
                returned = true;
                return c;
            } catch (IOException error) {
                throw error;
            } catch (Exception error) {
                throw new IOException("Raid service connection unavailable (" + error.getClass().getSimpleName() + ")");
            } finally {
                if (!returned) {
                    c.disconnect();
                    watch.accept(null);
                }
            }
        }
        throw new IOException("Raid service connection unavailable");
    }

    private RaidProxy() {
    }
}
