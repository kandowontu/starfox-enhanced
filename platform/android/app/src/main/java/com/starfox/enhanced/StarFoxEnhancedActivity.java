package com.starfox.enhanced;

import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import org.libsdl.app.SDLActivity;

public final class StarFoxEnhancedActivity extends SDLActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        hideSystemBars();
    }

    @Override
    protected void onResume() {
        super.onResume();
        hideSystemBars();
        // The controller can be absent until the decor attaches. Retry after
        // attachment, including when returning from Android's document picker.
        getWindow().getDecorView().post(() -> {
            if (!isDestroyed() && !isFinishing()) hideSystemBars();
        });
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemBars();
    }

    @SuppressWarnings("deprecation")
    private void hideSystemBars() {
        final Window window = getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        window.clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
        if (Build.VERSION.SDK_INT >= 28) {
            final WindowManager.LayoutParams attributes = window.getAttributes();
            attributes.layoutInDisplayCutoutMode = Build.VERSION.SDK_INT >= 30
                ? WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
                : WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            window.setAttributes(attributes);
        }
        if (Build.VERSION.SDK_INT >= 30) {
            window.setDecorFitsSystemWindows(false);
            final WindowInsetsController controller = window.getDecorView().getWindowInsetsController();
            if (controller != null) {
                controller.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                controller.hide(WindowInsets.Type.systemBars());
            }
        } else {
            window.getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
    }
}
