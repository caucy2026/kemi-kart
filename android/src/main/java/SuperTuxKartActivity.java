package org.supertuxkart.stk;

import android.util.Log;
import org.libsdl.app.SDLActivity;
import android.app.ActivityOptions;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Process;
import android.view.Display;

public class SuperTuxKartActivity extends SDLActivity {
    private float m_top_padding = 0.0f;
    private float m_bottom_padding = 0.0f;
    private float m_left_padding = 0.0f;
    private float m_right_padding = 0.0f;
    private int m_initial_orientation = -1;

    public static native void debugMsg(String msg);
    private static native void handlePadding(boolean val);
    private static native void saveKeyboardHeight(int height);
    private static native void saveMovedHeight(int height);
    private static native void addDNSSrvRecords(String name, int weight);
    private static native void pauseRenderingJNI();

    @Override
    protected String getMainSharedObject() {
        return getContext().getApplicationInfo().nativeLibraryDir + "/libmain.so";
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // 防呆：如果被副屏启动器误启动到非主屏，强制迁回主屏
        final int launchedDisplayId = getWindowManager().getDefaultDisplay().getDisplayId();
        if (launchedDisplayId != Display.DEFAULT_DISPLAY) {
            Log.w("STK", "Launched on display " + launchedDisplayId + " — redirecting to D0");
            // 用反射调用隐藏 API setLaunchDisplayId 确保新 Activity 在 D0 启动
            try {
                final android.app.ActivityOptions opts = android.app.ActivityOptions.makeBasic();
                final java.lang.reflect.Method m = opts.getClass()
                    .getMethod("setLaunchDisplayId", int.class);
                m.invoke(opts, Display.DEFAULT_DISPLAY);
                final Intent intent = new Intent(this, SuperTuxKartActivity.class)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
                startActivity(intent, opts.toBundle());
            } catch (Exception e) {
                // 反射失败降级：直接 startActivity（可能仍在 D2，但会再次防呆退出）
                Log.w("STK", "setLaunchDisplayId failed: " + e.getMessage());
                final Intent intent = new Intent(this, SuperTuxKartActivity.class)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
                startActivity(intent);
            }
            finish();
            return;
        }

        m_initial_orientation = getRequestedOrientation();
    }

    /** HOME key → full exit (same as in-game "退出").
     *  Hard-kill the process to guarantee clean cold start on next launch. */
    @Override
    protected void onUserLeaveHint() {
        super.onUserLeaveHint();
        if (mPresentation != null) {
            mPresentation.dismiss();
        }
        finishAffinity();
        // Force-kill ensures no SDL thread residue on next cold start
        android.os.Process.killProcess(android.os.Process.myPid());
    }

    public int getScreenSize() {
        return getResources().getConfiguration().screenLayout &
            Configuration.SCREENLAYOUT_SIZE_MASK;
    }

    public boolean isHardwareKeyboardConnected() {
        return getResources().getConfiguration()
            .keyboard == Configuration.KEYBOARD_QWERTY;
    }

    public float getTopPadding() { return m_top_padding; }
    public float getBottomPadding() { return m_bottom_padding; }
    public float getLeftPadding() { return m_left_padding; }
    public float getRightPadding() { return m_right_padding; }
    public int getInitialOrientation() { return m_initial_orientation; }

    /** Called from native code to update extraction progress bar (0-100, -1=error) */
    public void showExtractProgress(int progress) {
        // Minimal implementation: just log progress
        // The original STK activity shows a progress bar UI
    }

    /** Called from native code to get display DPI metrics */
    public static android.util.DisplayMetrics getDisplayDPI() {
        android.util.DisplayMetrics metrics = new android.util.DisplayMetrics();
        if (SDLActivity.mSingleton != null) {
            ((android.view.WindowManager) SDLActivity.mSingleton
                .getSystemService(android.content.Context.WINDOW_SERVICE))
                .getDefaultDisplay().getMetrics(metrics);
        }
        return metrics;
    }

    /** Called from native code for locale string */
    public String getLocaleString() {
        java.util.Locale locale;
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            locale = getResources().getConfiguration().getLocales().get(0);
        } else {
            locale = getResources().getConfiguration().locale;
        }
        return locale.toString();
    }

    /** Called from native code to hide splash screen when loading completes */
    public void hideSplashScreen() {
        // Stub: splash screen is managed by the theme, no-op for now
    }

    /** Called from native code to show soft keyboard */
    public void showKeyboard(final int type, final int y) {
        // Stub
    }

    /** Called from native code to hide soft keyboard */
    public void hideKeyboard(final boolean clear_text) {
        // Stub
    }

    /**
     * Called from Irrlicht when an edit box receives focus. The current
     * dual-screen activity intentionally has no Android text-input overlay,
     * but native code requires this callback to exist.
     */
    public void fromSTKEditBox(final int widget_id, final String text,
                               final int selection_start,
                               final int selection_end, final int type) {
        // No Android text-input bridge is installed in this activity.
    }

    // Additional STK JNI methods (stubs)

    public int getMovedHeight() { return 0; }
    public int getKeyboardHeight() { return 0; }
    public void getDNSSrvRecords(String name) {}
    public String[] getDNSTxtRecords(String name) { return new String[0]; }
}
