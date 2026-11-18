package dev.silphmod.nativecheck;

import android.content.Context;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.drawable.GradientDrawable;
import android.provider.Settings;
import android.view.Gravity;
import android.view.WindowManager;
import android.widget.TextView;

final class FarmerActionOverlay {
    static final String ENABLED="farmer.actionOverlay";
    private final Context context;
    private final WindowManager windows;
    private TextView badge;
    private boolean unavailable;
    private int shown;

    FarmerActionOverlay(Context context){
        this.context=context;
        windows=context.getSystemService(WindowManager.class);
    }

    void show(int activity){
        String label=label(activity);
        if(label==null||!Settings.canDrawOverlays(context)){
            hide();
            return;
        }
        if(unavailable||windows==null||shown==activity)return;
        if(badge==null){
            TextView view=new TextView(context);
            view.setText("Farmer · "+label);
            view.setTextSize(13);
            view.setTextColor(Color.WHITE);
            view.setSingleLine(true);
            view.setPadding(dp(12),dp(6),dp(12),dp(6));
            GradientDrawable background=new GradientDrawable();
            background.setColor(0xff18222c);
            background.setCornerRadius(dp(14));
            view.setBackground(background);
            WindowManager.LayoutParams params=new WindowManager.LayoutParams(
                    WindowManager.LayoutParams.WRAP_CONTENT,WindowManager.LayoutParams.WRAP_CONTENT,
                    WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                    WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE|WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE,
                    PixelFormat.TRANSLUCENT);
            params.gravity=Gravity.TOP|Gravity.CENTER_HORIZONTAL;
            params.y=dp(12);
            // Keep the window below Android's obscuring-opacity limit for touch passthrough.
            params.alpha=0.8f;
            try{windows.addView(view,params);}
            catch(WindowManager.BadTokenException|WindowManager.InvalidDisplayException|SecurityException error){
                unavailable=true;
                return;
            }
            badge=view;
        }else{
            badge.setText("Farmer · "+label);
        }
        shown=activity;
    }

    void hide(){
        if(badge==null)return;
        windows.removeViewImmediate(badge);
        badge=null;
        shown=0;
    }

    private int dp(int value){
        return Math.round(value*context.getResources().getDisplayMetrics().density);
    }

    // Values match the Farmer activity codes in session-wire.h.
    static String label(int activity){
        return switch(activity){
            case 1 -> "Waiting";
            case 2 -> "Looking for Pokémon";
            case 3 -> "Starting encounter";
            case 4 -> "Feeding a berry";
            case 5 -> "Catching Pokémon";
            case 6 -> "Checking new catch";
            case 7 -> "Transferring Pokémon";
            case 8 -> "Spinning PokéStop";
            case 9 -> "Discarding items";
            case 10 -> "Claiming research";
            case 11 -> "Claiming GO Pass rewards";
            case 12 -> "Caring for buddy";
            case 13 -> "Handling gifts";
            case 14 -> "Removing buddy";
            default -> null;
        };
    }
}
