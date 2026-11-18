package dev.silphmod.nativecheck;

import android.content.Context;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.provider.Settings;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;
import android.view.Gravity;
import android.view.WindowManager;
import android.widget.TextView;

import java.io.IOException;
import java.util.List;
import java.util.StringJoiner;

import dev.rootcatch.automation.PokemonCatalog;
import dev.rootcatch.nativeengine.NativeSessionWire.PokemonDetails;
import dev.rootcatch.nativeengine.NativeSessionWire.Snapshot;

final class FarmerActionOverlay {
    static final String ENABLED = "farmer.actionOverlay";
    private static final int MUTED = 0xffc3d0df, CAUGHT = 0xffa2efba, TRANSFERRED = 0xffadceff;
    private final Context context;
    private final WindowManager windows;
    private final String[] activityLabels;
    private TextView badge;
    private boolean unavailable;
    private Snapshot shown;
    private List<PokemonCatalog.Entry> catalog;

    FarmerActionOverlay(Context context) {
        this.context = context;
        windows = context.getSystemService(WindowManager.class);
        activityLabels = context.getResources().getStringArray(R.array.farmer_overlay_activities);
    }

    void show(Snapshot snapshot) {
        int activity = snapshot == null ? 0 : snapshot.farmerActivity;
        if (activity <= 0 || activity >= activityLabels.length || !Settings.canDrawOverlays(context)) {
            hide();
            return;
        }
        if (unavailable || windows == null || sameContent(snapshot)) return;
        CharSequence content = content(snapshot, activityLabels[activity]);
        if (badge == null) {
            TextView view = new TextView(context);
            view.setText(content);
            view.setTextSize(13);
            view.setTextColor(Color.WHITE);
            view.setMaxWidth(dp(316));
            view.setPadding(dp(12), dp(8), dp(12), dp(8));
            GradientDrawable background = new GradientDrawable();
            background.setColor(0xff18222c);
            background.setCornerRadius(dp(14));
            view.setBackground(background);
            WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                    WindowManager.LayoutParams.WRAP_CONTENT, WindowManager.LayoutParams.WRAP_CONTENT,
                    WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                    WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE,
                    PixelFormat.TRANSLUCENT);
            params.gravity = Gravity.TOP | Gravity.CENTER_HORIZONTAL;
            params.y = dp(12);
            // Keep the window below Android's obscuring-opacity limit for touch passthrough.
            params.alpha = 0.8f;
            try {
                windows.addView(view, params);
            } catch (WindowManager.BadTokenException | WindowManager.InvalidDisplayException |
                     SecurityException error) {
                unavailable = true;
                return;
            }
            badge = view;
        } else {
            badge.setText(content);
        }
        shown = snapshot;
    }

    void hide() {
        if (badge == null) return;
        windows.removeViewImmediate(badge);
        badge = null;
        shown = null;
    }

    private boolean sameContent(Snapshot snapshot) {
        return shown != null && shown.farmerActivity == snapshot.farmerActivity
                && shown.caught == snapshot.caught && shown.transferred == snapshot.transferred
                && shown.catchOutcome == snapshot.catchOutcome && shown.targetPokemon.equals(snapshot.targetPokemon)
                && shown.lastCatch.equals(snapshot.lastCatch);
    }

    private CharSequence content(Snapshot snapshot, String action) {
        SpannableStringBuilder text = new SpannableStringBuilder();
        PokemonDetails target = snapshot.targetPokemon;
        boolean encountering = snapshot.farmerActivity >= 3 && snapshot.farmerActivity <= 5 && target.species() != 0;
        line(text, context.getString(R.string.farmer_overlay_action, action), Color.WHITE, true);
        if (encountering) {
            line(text, name(target), Color.WHITE, true);
            line(text, details(target), MUTED, false);
        }
        if (snapshot.lastCatch.species() != 0) {
            int outcome = switch (snapshot.catchOutcome) {
                case 2 -> R.string.farmer_overlay_kept;
                case 3 -> R.string.farmer_overlay_transferred;
                default -> R.string.farmer_overlay_caught;
            };
            line(text, context.getString(outcome, name(snapshot.lastCatch)),
                    snapshot.catchOutcome == 3 ? TRANSFERRED : CAUGHT, true);
            line(text, details(snapshot.lastCatch), MUTED, false);
        }
        if (snapshot.caught != 0 || snapshot.transferred != 0)
            line(text, context.getString(R.string.farmer_overlay_totals, snapshot.caught, snapshot.transferred), MUTED, false);
        return text;
    }

    private String name(PokemonDetails pokemon) {
        if (catalog == null) {
            try {
                catalog = PokemonCatalog.load(context);
            } catch (IOException ignored) {
                catalog = List.of();
            }
        }
        String name = context.getString(R.string.farmer_overlay_unknown_pokemon, pokemon.species());
        for (PokemonCatalog.Entry entry : catalog) {
            if (entry.dex() > pokemon.species()) break;
            if (entry.dex() != pokemon.species()) continue;
            if (entry.form() == pokemon.form()) return entry.name();
            if (entry.form() == 0) name = entry.name();
        }
        return name;
    }

    private String details(PokemonDetails pokemon) {
        StringJoiner details = new StringJoiner(" · ");
        if (pokemon.cp() > 0) details.add(context.getString(R.string.farmer_overlay_cp, pokemon.cp()));
        if (pokemon.ivs() != 0) {
            int ivs = pokemon.ivs() - 1;
            int attack = ivs >>> 8, defense = (ivs >>> 4) & 15, stamina = ivs & 15;
            int percent = Math.round((attack + defense + stamina) * 100f / 45);
            details.add(context.getString(R.string.farmer_overlay_iv, percent, attack, defense, stamina));
        }
        if (pokemon.shiny()) details.add(context.getString(R.string.farmer_overlay_shiny));
        return details.toString();
    }

    private static void line(SpannableStringBuilder text, String value, int color, boolean bold) {
        if (value.isEmpty()) return;
        if (text.length() != 0) text.append('\n');
        int start = text.length();
        text.append(value);
        text.setSpan(new ForegroundColorSpan(color), start, text.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        if (bold) text.setSpan(new StyleSpan(Typeface.BOLD), start, text.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
    }

    private int dp(int value) {
        return Math.round(value * context.getResources().getDisplayMetrics().density);
    }
}
