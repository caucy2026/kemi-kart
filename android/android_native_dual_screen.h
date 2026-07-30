// android_native_dual_screen.h
// Public API for dual-screen EGL management
// Used by the STK render loop to render Player 2's view to Display 2.

#ifndef ANDROID_NATIVE_DUAL_SCREEN_H
#define ANDROID_NATIVE_DUAL_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

/** Check if the second display surface is ready for rendering. */
bool dualScreenIsReady();

/** Get second display dimensions. Pass NULL for either param if not needed. */
void dualScreenGetSize(int* width, int* height);

/** 
 * Activate the second EGL surface for rendering Player 2's view.
 * Must be called before rendering Camera 1.
 * Returns true on success.
 */
bool dualScreenMakeCurrent();

/**
 * Swap buffers on the second display.
 * Must be called after rendering Camera 1's viewport.
 * Returns true on success.
 */
bool dualScreenSwapBuffers();

/**
 * Restore the primary EGL surface for subsequent rendering.
 */
bool dualScreenRestorePrimary();

/**
 * Call BEFORE endScene on Display 0.
 * Captures Display 0's backbuffer to a mirror FBO for later presentation.
 */
void dualScreenMirrorCapture(int w, int h);

/**
 * Call AFTER endScene on Display 0.
 * Presents the previously captured frame to Display 2.
 */
void dualScreenMirrorPresent();

/**
 * Draw an arbitrary GL texture as a fullscreen quad on Display 2.
 * Must be called with Display 2's EGL surface active.
 * Used for independent camera view (Camera 1 FBO → Display 2).
 */
void dualScreenDrawFBO(unsigned int texId, int w, int h);

#ifdef __cplusplus
}
#endif

#endif // ANDROID_NATIVE_DUAL_SCREEN_H
