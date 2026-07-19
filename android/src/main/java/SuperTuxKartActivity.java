package org.supertuxkart.stk;

import org.libsdl.app.SDLActivity;
import android.content.res.Configuration;
import android.os.Bundle;

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
        m_initial_orientation = getRequestedOrientation();
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

    // Additional STK JNI methods (stubs)

    public int getMovedHeight() { return 0; }
    public int getKeyboardHeight() { return 0; }
    public void getDNSSrvRecords(String name) {}
    public String[] getDNSTxtRecords(String name) { return new String[0]; }
}
