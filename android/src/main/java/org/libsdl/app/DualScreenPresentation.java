package org.libsdl.app;

import android.app.Presentation;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.view.WindowManager;
import android.widget.RelativeLayout;

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
    protected int mDisplayId;
    protected int mWidth;
    protected int mHeight;

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
        
        Log.v(TAG, "DualScreenPresentation onCreate() on display " + mDisplayId);

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

        // Use full screen layout
        mLayout = new RelativeLayout(getContext());
        mLayout.addView(mSecondSurface,
            new RelativeLayout.LayoutParams(
                RelativeLayout.LayoutParams.MATCH_PARENT,
                RelativeLayout.LayoutParams.MATCH_PARENT));

        // Presentation already handles window setup for secondary displays.
        // No need to set window type - Presentation base class handles this correctly.

        setContentView(mLayout);

        Log.v(TAG, "DualScreenPresentation created successfully on display " + mDisplayId);
        // Note: onSecondSurfaceCreated will be called from SDLSurface.surfaceCreated()
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

    public void destroy() {
        Log.v(TAG, "DualScreenPresentation destroy() display " + mDisplayId);
        if (mSecondSurface != null) {
            SDLActivity.onSecondSurfaceDestroyed();
            mSecondSurface = null;
        }
        dismiss();
    }
}
