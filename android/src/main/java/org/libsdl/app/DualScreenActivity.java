package org.libsdl.app;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Color;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Display;
import android.view.View;
import android.view.WindowManager;
import android.widget.RelativeLayout;

public class DualScreenActivity extends Activity {
    private static final String TAG = "SDL/DualScreenActivity";

    private static DualScreenActivity sInstance;

    private SDLSurface mSecondSurface;
    private LoadingCircleView mLoadingView;
    private DisplayManager mDisplayManager;
    private int mDisplayId;
    private final DisplayManager.DisplayListener mDisplayListener =
        new DisplayManager.DisplayListener() {
            @Override
            public void onDisplayAdded(int displayId) {
            }

            @Override
            public void onDisplayRemoved(int displayId) {
            }

            @Override
            public void onDisplayChanged(int displayId) {
                if (displayId == mDisplayId && mSecondSurface != null) {
                    Log.v(TAG, "Display changed, refreshing D2 rotation compensation");
                    mSecondSurface.refreshSecondSurfaceTransform();
                }
            }
        };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        sInstance = this;

        Display display = getWindowManager().getDefaultDisplay();
        mDisplayId = display.getDisplayId();
        Log.v(TAG, "onCreate() display=" + mDisplayId
            + " rotation=" + display.getRotation()
            + " requestedOrientation=" + getRequestedOrientation());

        int flags = View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
        getWindow().getDecorView().setSystemUiVisibility(flags);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);

        RelativeLayout layout = new RelativeLayout(this);
        layout.setBackgroundColor(Color.parseColor("#0A0F28"));

        mSecondSurface = new SDLSurface(this);
        mSecondSurface.setDisplayId(mDisplayId);
        layout.addView(mSecondSurface, new RelativeLayout.LayoutParams(
            RelativeLayout.LayoutParams.MATCH_PARENT,
            RelativeLayout.LayoutParams.MATCH_PARENT));

        mLoadingView = new LoadingCircleView(this);
        mLoadingView.setRotation(180.0f);
        layout.addView(mLoadingView, new RelativeLayout.LayoutParams(
            RelativeLayout.LayoutParams.MATCH_PARENT,
            RelativeLayout.LayoutParams.MATCH_PARENT));
        mLoadingView.bringToFront();

        setContentView(layout);
        mDisplayManager = (DisplayManager)getSystemService(Context.DISPLAY_SERVICE);
        mDisplayManager.registerDisplayListener(mDisplayListener, null);
        startPollingNativeReady();
    }

    private void startPollingNativeReady() {
        Handler handler = new Handler(Looper.getMainLooper());
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (mLoadingView == null || mLoadingView.getVisibility() != View.VISIBLE) {
                    return;
                }
                if (DualScreenPresentation.nativeIsD2Ready()) {
                    Log.v(TAG, "Native D2 ready, hiding loading view");
                    mLoadingView.setVisibility(View.GONE);
                } else {
                    handler.postDelayed(this, 500);
                }
            }
        }, 500);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (mSecondSurface != null) {
            mSecondSurface.handleResume();
            mSecondSurface.refreshSecondSurfaceTransform();
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        Log.v(TAG, "Configuration changed, refreshing D2 rotation compensation");
        if (mSecondSurface != null) {
            mSecondSurface.post(mSecondSurface::refreshSecondSurfaceTransform);
        }
    }

    @Override
    protected void onPause() {
        if (mSecondSurface != null) {
            mSecondSurface.handlePause();
        }
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        Log.v(TAG, "onDestroy()");
        if (mDisplayManager != null) {
            mDisplayManager.unregisterDisplayListener(mDisplayListener);
            mDisplayManager = null;
        }
        if (sInstance == this) {
            sInstance = null;
        }
        super.onDestroy();
    }

    public static void finishInstance() {
        if (sInstance != null) {
            sInstance.finishAndRemoveTask();
        }
    }
}