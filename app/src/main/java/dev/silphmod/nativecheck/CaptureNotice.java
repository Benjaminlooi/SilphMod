package dev.silphmod.nativecheck;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import dev.rootcatch.nativeengine.NativeSessionWire;

final class CaptureNotice {
    private final Context context;
    private final Handler main = new Handler(Looper.getMainLooper());
    private int last;
    private long catches, fled;
    private volatile boolean closed;
    private Toast visible;

    CaptureNotice(Context context) {
        this.context = context.getApplicationContext();
    }

    void accept(NativeSessionWire.Snapshot snapshot) {
        if (closed) return;
        int event = snapshot.researchStatus;
        boolean caught = snapshot.caught > catches, escaped = snapshot.fled > fled;
        catches = snapshot.caught;
        fled = snapshot.fled;
        boolean fresh = event != 0 && event != last;
        last = event;
        String text = fresh ? switch (event & 7) {
            case 1 -> "Caught";
            case 2 -> "Escaped";
            case 3 -> "Fled";
            case 4 -> "Missed";
            default -> null;
        } : caught ? "Caught" : escaped ? "Fled" : null;
        if (text != null) main.post(() -> show(text));
    }

    @SuppressWarnings("deprecation")
    private void show(String text) {
        if (closed) return;
        if (visible != null) visible.cancel();
        visible = Toast.makeText(context, text, Toast.LENGTH_SHORT);
        if (Build.VERSION.SDK_INT < 30) {
            float density = context.getResources().getDisplayMetrics().density;
            LinearLayout row = new LinearLayout(context);
            row.setGravity(Gravity.CENTER_VERTICAL);
            int pad = (int) (12 * density);
            row.setPadding(pad, pad, pad, pad);
            GradientDrawable background = new GradientDrawable();
            background.setColor(0xee1b2635);
            background.setCornerRadius(18 * density);
            row.setBackground(background);
            ImageView icon = new ImageView(context);
            icon.setImageResource(R.drawable.ic_launcher);
            row.addView(icon, new LinearLayout.LayoutParams((int) (26 * density), (int) (26 * density)));
            TextView label = new TextView(context);
            label.setText(text);
            label.setTextColor(Color.WHITE);
            label.setTextSize(15);
            label.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
            label.setPadding(pad, 0, 0, 0);
            row.addView(label);
            visible.setView(row);
            visible.setGravity(Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL, 0, (int) (72 * density));
        }
        visible.show();
    }

    void close() {
        closed = true;
        main.removeCallbacksAndMessages(null);
        main.post(() -> {
            if (visible != null) {
                visible.cancel();
                visible = null;
            }
        });
    }
}
