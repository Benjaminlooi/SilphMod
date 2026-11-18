package dev.silphmod.nativecheck;

import java.text.Normalizer;
import java.time.ZoneId;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

final class RaidLocations {
    static final int COUNT = 28;

    static final class Location {
        final int id;
        final String name, country, providerCity, code, timezone, flag, search;
        final double latitude, longitude;
        final ZoneId zone;

        Location(int id, String name, String country, String providerCity, String code, String timezone, double latitude, double longitude) {
            this.id = id;
            this.name = name;
            this.country = country;
            this.providerCity = providerCity;
            this.code = code;
            this.timezone = timezone;
            this.latitude = latitude;
            this.longitude = longitude;
            zone = ZoneId.of(timezone);
            flag = new String(Character.toChars(0x1f1e6 + code.charAt(0) - 'a')) + new String(Character.toChars(0x1f1e6 + code.charAt(1) - 'a'));
            search = normalize(name + " " + country + " " + providerCity + " " + timezone);
        }

        String reference() {
            return String.format(Locale.ROOT, "%.6f,%.6f", latitude, longitude);
        }
    }

    static final List<Location> ALL = Collections.unmodifiableList(Arrays.asList(
            new Location(3, "Kiritimati", "Kiribati", "Kiritimati", "ki", "Pacific/Kiritimati", 1.987149, -157.477137),
            new Location(4, "Wellington", "New Zealand", "Wellington", "nz", "Pacific/Auckland", -41.282491, 174.769132),
            new Location(5, "Sydney", "Australia", "Sydney", "au", "Australia/Sydney", -33.863021, 151.215353),
            new Location(6, "Tokyo", "Japan", "Tokyo", "jp", "Asia/Tokyo", 35.689201, 139.697804),
            new Location(2, "Sendai", "Japan", "Sendai", "jp", "Asia/Tokyo", 38.262505, 140.878035),
            new Location(7, "Seoul", "Korea", "Seoul", "kr", "Asia/Seoul", 37.512444, 127.101460),
            new Location(8, "Taipei", "Taiwan", "Taipei", "tw", "Asia/Taipei", 25.042289, 121.506751),
            new Location(9, "River Valley", "Singapore", "Singapore", "sg", "Asia/Singapore", 1.292435, 103.847606),
            new Location(10, "Kuala Lumpur", "Malaysia", "Kuala Lumpur", "my", "Asia/Kuala_Lumpur", 3.131001, 101.671742),
            new Location(11, "New Delhi", "India", "New Delhi", "in", "Asia/Kolkata", 28.593711, 77.220227),
            new Location(12, "Mumbai", "India", "Mumbai", "in", "Asia/Kolkata", 18.926681, 72.832133),
            new Location(13, "Dubai", "United Arab Emirates", "Dubai", "ae", "Asia/Dubai", 25.077497, 55.133673),
            new Location(14, "Istanbul", "Turkey", "Istanbul", "tr", "Europe/Istanbul", 41.011949, 28.978977),
            new Location(15, "Zaragoza", "Spain", "Zaragoza", "es", "Europe/Madrid", 41.661545, -0.894701),
            new Location(16, "Paris", "France", "Paris", "fr", "Europe/Paris", 48.863598, 2.327481),
            new Location(17, "Berlin", "Germany", "Berlin", "de", "Europe/Berlin", 52.516103, 13.361087),
            new Location(18, "London", "United Kingdom", "London", "gb", "Europe/London", 51.501222, -0.139702),
            new Location(19, "Lisbon", "Portugal", "Lisbon", "pt", "Europe/Lisbon", 38.716183, -9.148742),
            new Location(20, "São Paulo", "Brazil", "Sao Paulo", "br", "America/Sao_Paulo", -23.584210, -46.659413),
            new Location(21, "Santiago", "Chile", "Santiago", "cl", "America/Santiago", -33.435701, -70.640991),
            new Location(22, "New York", "United States", "New York", "us", "America/New_York", 40.765723, -73.973038),
            new Location(23, "Lima", "Peru", "Lima", "pe", "America/Lima", -12.062746, -77.036275),
            new Location(24, "Chicago", "United States", "Chicago", "us", "America/Chicago", 41.891882, -87.598955),
            new Location(1, "Mexico City", "Mexico", "Mexico City", "mx", "America/Mexico_City", 19.435731, -99.143961),
            new Location(25, "Pier 39 San Francisco", "United States", "San Francisco", "us", "America/Los_Angeles", 37.808971, -122.410242),
            new Location(26, "Vancouver", "Canada", "Vancouver", "ca", "America/Vancouver", 49.240966, -123.112591),
            new Location(27, "Honolulu", "United States", "Honolulu", "us", "Pacific/Honolulu", 21.291101, -157.843110),
            new Location(28, "Fagatogo", "American Samoa", "Pago Pago", "as", "Pacific/Pago_Pago", -14.277941, -170.688202)
    ));

    static boolean valid(int id) {
        return id >= 1 && id <= COUNT;
    }

    static Location get(int id) {
        for (Location location : ALL) if (location.id == id) return location;
        throw new IllegalArgumentException("Unknown raid location");
    }

    static String normalize(String text) {
        return Normalizer.normalize(text, Normalizer.Form.NFD).replaceAll("\\p{M}+", "").toLowerCase(Locale.ROOT).trim();
    }

    private RaidLocations() {
    }
}
