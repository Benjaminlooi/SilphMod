package dev.silphmod.nativecheck;

public final class RocketTypes {
    public enum Type {
        GRUNT("Grunt"), BUG("Bug"), DARK("Dark"), DRAGON("Dragon"), FAIRY("Fairy"),
        FIGHTING("Fighting"), FIRE("Fire"), FLYING("Flying"), GHOST("Ghost"),
        GRASS("Grass"), GROUND("Ground"), ICE("Ice"), NORMAL("Normal"), POISON("Poison"),
        PSYCHIC("Psychic"), ROCK("Rock"), STEEL("Steel"), WATER("Water"), ELECTRIC("Electric");
        public final String label;

        Type(String label) {
            this.label = label;
        }

        public int bit() {
            return 1 << ordinal();
        }
    }

    public static final int ALL = (1 << 19) - 1;

    private RocketTypes() {
    }

    public static boolean valid(int mask) {
        return mask > 0 && (mask & ~ALL) == 0;
    }

    public static int checked(int mask) {
        if (!valid(mask))
            throw new IllegalArgumentException(mask == 0 ? "Select at least one Rocket type" : "Invalid Rocket type selection");
        return mask;
    }

    public static boolean normalGrunt(int character) {
        return character >= 4 && character <= 39 || character >= 47 && character <= 50;
    }

    public static Type type(int character) {
        return switch (character) {
            case 4, 5 -> Type.GRUNT;
            case 6, 7 -> Type.BUG;
            case 10, 11 -> Type.DARK;
            case 12, 13 -> Type.DRAGON;
            case 14, 15 -> Type.FAIRY;
            case 16, 17 -> Type.FIGHTING;
            case 18, 19 -> Type.FIRE;
            case 20, 21 -> Type.FLYING;
            case 22, 23 -> Type.GRASS;
            case 24, 25 -> Type.GROUND;
            case 26, 27 -> Type.ICE;
            case 28, 29 -> Type.STEEL;
            case 30, 31 -> Type.NORMAL;
            case 32, 33 -> Type.POISON;
            case 34, 35 -> Type.PSYCHIC;
            case 36, 37 -> Type.ROCK;
            case 38, 39 -> Type.WATER;
            case 47, 48 -> Type.GHOST;
            case 49, 50 -> Type.ELECTRIC;
            default -> null;
        };
    }

    public static boolean selected(int mask, int character) {
        if (!valid(mask) || !normalGrunt(character)) return false;
        Type type = type(character);
        return mask == ALL || type != null && (mask & type.bit()) != 0;
    }

    public static int confirmedCharacter(String source, String candidate) {
        if (source == null || candidate == null || candidate.isEmpty()) return -1;
        String prefix = candidate + ":npc:";
        if (!source.startsWith(prefix)) return -1;
        String number = source.substring(prefix.length());
        if (!number.matches("[0-9]{1,3}")) return -1;
        int value = Integer.parseInt(number);
        return normalGrunt(value) && Integer.toString(value).equals(number) ? value : -1;
    }
}
