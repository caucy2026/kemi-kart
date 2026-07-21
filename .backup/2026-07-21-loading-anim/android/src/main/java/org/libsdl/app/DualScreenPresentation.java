package org.libsdl.app;

import android.app.Presentation;
import android.content.Context;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Display;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.RelativeLayout;
import android.widget.TextView;

/**
 * DualScreenPresentation - Shows a second SDLSurface on Display 2
 * for dual-screen split-screen racing mode.
 * 
 * Each display renders one player's camera view:
 *   Display 0 (main) → Player 1 camera
 *   Display 2 (ext)  → Player 2 camera
 */
public class DualScreenPresentation extends Presentation {

    private static final String TAG = "SDL/DualScreen";
    
    protected SDLSurface mSecondSurface;
    protected RelativeLayout mLayout;
    protected LoadingCircleView mLoadingView;
    protected int mDisplayId;
    protected int mWidth;
    protected int mHeight;
    
    // Static reference for JNI to hide loading view
    private static DualScreenPresentation sInstance = null;

    public DualScreenPresentation(Context outerContext, Display display) {
        super(outerContext, display);
        mDisplayId = display.getDisplayId();
    }

    public SDLSurface getSurface() {
        return mSecondSurface;
    }

    public int getDisplayId() {
        return mDisplayId;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        sInstance = this;
        
        Log.v(TAG, "DualScreenPresentation onCreate() on display " + mDisplayId);

        if (android.os.Build.VERSION.SDK_INT >= 19) {
            int flags = View.SYSTEM_UI_FLAG_FULLSCREEN |
                    View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                    View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                    View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.INVISIBLE;
            getWindow().getDecorView().setSystemUiVisibility(flags);
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
        }

        // Get display metrics
        Display display = getDisplay();
        if (display != null) {
            android.util.DisplayMetrics metrics = new android.util.DisplayMetrics();
            display.getRealMetrics(metrics);
            mWidth = metrics.widthPixels;
            mHeight = metrics.heightPixels;
            Log.v(TAG, "Display " + mDisplayId + " size: " + mWidth + "x" + mHeight);
        }

        // Create the second SDL surface for this display
        mSecondSurface = new SDLSurface(getContext());
        mSecondSurface.setDisplayId(mDisplayId);

        // Use full screen layout — dark blue bg so it's never pure black
        mLayout = new RelativeLayout(getContext());
        mLayout.setBackgroundColor(Color.parseColor("#0A0F28"));

        // Add SDLSurface FIRST (bottom layer)
        mSecondSurface = new SDLSurface(getContext());
        mSecondSurface.setDisplayId(mDisplayId);
        mLayout.addView(mSecondSurface,
            new RelativeLayout.LayoutParams(
                RelativeLayout.LayoutParams.MATCH_PARENT,
                RelativeLayout.LayoutParams.MATCH_PARENT));

        // Loading view ON TOP: white circle + blue "KEMI", pulsing animation
        mLoadingView = new LoadingCircleView(getContext());
        mLayout.addView(mLoadingView,
            new RelativeLayout.LayoutParams(
                RelativeLayout.LayoutParams.MATCH_PARENT,
                RelativeLayout.LayoutParams.MATCH_PARENT));
        mLoadingView.bringToFront();

        // Presentation already handles window setup for secondary displays.
        // No need to set window type - Presentation base class handles this correctly.

        setContentView(mLayout);

        Log.v(TAG, "DualScreenPresentation created successfully on display " + mDisplayId);
        
        // Poll native readiness every 500ms; hide loading view when D2 native rendering starts
        startPollingNativeReady();
        // Note: onSecondSurfaceCreated will be called from SDLSurface.surfaceCreated()
    }
    
    private void startPollingNativeReady() {
        Handler handler = new Handler(Looper.getMainLooper());
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (mLoadingView == null || mLoadingView.getVisibility() != View.VISIBLE) {
                    return; // already hidden
                }
                if (nativeIsD2Ready()) {
                    Log.v(TAG, "Native D2 ready, hiding loading view");
                    mLoadingView.setVisibility(View.GONE);
                } else {
                    handler.postDelayed(this, 500); // check again in 500ms
                }
            }
        }, 500);
    }

    @Override
    protected void onStart() {
        super.onStart();
        Log.v(TAG, "DualScreenPresentation onStart() display " + mDisplayId);
        if (mSecondSurface != null) {
            mSecondSurface.handleResume();
        }
    }

    @Override
    protected void onStop() {
        super.onStop();
        Log.v(TAG, "DualScreenPresentation onStop() display " + mDisplayId);
        if (mSecondSurface != null) {
            mSecondSurface.handlePause();
        }
    }

    /**
     * Called from native code (JNI) when D2 native rendering is ready.
     * Hides the loading view, revealing the SDL surface underneath.
     */
    public static void hideLoadingView() {
        new Handler(Looper.getMainLooper()).post(() -> {
            if (sInstance != null && sInstance.mLoadingView != null) {
                Log.v(TAG, "Hiding loading view on display " + sInstance.mDisplayId);
                sInstance.mLoadingView.setVisibility(View.GONE);
            }
        });
    }
    
    /**
     * JNI: Check if native dual-screen rendering is ready.
     */
    public static native boolean nativeIsD2Ready();

    public void destroy() {
        Log.v(TAG, "DualScreenPresentation destroy() display " + mDisplayId);
        if (mSecondSurface != null) {
            SDLActivity.onSecondSurfaceDestroyed();
            mSecondSurface = null;
        }
        dismiss();
    }
}
