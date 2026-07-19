// android_native_dual_screen.cpp
// Dual-screen EGL management for STK Android
// Manages a second EGL surface on Display 2, sharing the EGL context with Display 0.
//
// Architecture:
//   Display 0 (main)  → SDL's existing EGL surface (unchanged)
//   Display 2 (ext)   → Our second EGL surface (shared context)
//
// Render loop integration:
//   For each frame:
//     1. Render Camera 0 → SDL swaps Display 0 (normal flow)
//     2. eglMakeCurrent(display, secondSurface, ...) 
//     3. Render Camera 1 → eglSwapBuffers(display, secondSurface)

#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/log.h>
#include <cstdlib>

#define LOG_TAG "STK_DualScreen"
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Second display state
static ANativeWindow* g_secondWindow = nullptr;
static EGLSurface g_secondEGLSurface = EGL_NO_SURFACE;
static int g_secondWidth = 0;
static int g_secondHeight = 0;
static bool g_secondReady = false;

extern "C" {

JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_onNativeSecondSurfaceCreated(
    JNIEnv* env, jclass clazz, jobject surface)
{
    LOGI("onNativeSecondSurfaceCreated");
    
    if (surface == nullptr) {
        LOGE("Second surface is null!");
        return;
    }
    
    // Release any previous window
    if (g_secondWindow != nullptr) {
        LOGI("Releasing previous second window");
        ANativeWindow_release(g_secondWindow);
        g_secondWindow = nullptr;
    }
    
    g_secondWindow = ANativeWindow_fromSurface(env, surface);
    if (g_secondWindow == nullptr) {
        LOGE("Failed to get ANativeWindow from second surface");
        return;
    }
    
    // Try to get native window size
    int32_t width = ANativeWindow_getWidth(g_secondWindow);
    int32_t height = ANativeWindow_getHeight(g_secondWindow);
    LOGI("Second window native size: %dx%d", width, height);
    
    if (width > 0 && height > 0) {
        g_secondWidth = width;
        g_secondHeight = height;
    }
    
    // EGL surface creation is deferred until SDL's EGL context is ready.
    // The render loop will retry via dualScreenIsReady().
    LOGI("Second window stored. EGL surface will be created when SDL context is ready.");
}

JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_onNativeSecondSurfaceChanged(
    JNIEnv* env, jclass clazz, jobject surface, jint width, jint height)
{
    LOGI("onNativeSecondSurfaceChanged: %dx%d", width, height);
    g_secondWidth = width;
    g_secondHeight = height;
    
    // If window exists but EGL surface hasn't been created, the render loop
    // will retry via dualScreenIsReady(). Don't try to create here to avoid
    // race conditions with SDL initialization.
    if (g_secondWindow == nullptr && surface != nullptr)
    {
        // First time we get the surface - store the ANativeWindow
        g_secondWindow = ANativeWindow_fromSurface(env, surface);
        if (g_secondWindow != nullptr)
        {
            LOGI("onNativeSecondSurfaceChanged: window stored, %dx%d", width, height);
        }
        else
        {
            LOGE("onNativeSecondSurfaceChanged: Failed to get ANativeWindow");
        }
    }
    
    if (g_secondWindow != nullptr)
    {
        ANativeWindow_setBuffersGeometry(g_secondWindow, width, height,
                                          WINDOW_FORMAT_RGBA_8888);
    }
}

JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_onNativeSecondSurfaceDestroyed(
    JNIEnv* env, jclass clazz)
{
    LOGI("onNativeSecondSurfaceDestroyed");
    
    EGLDisplay display = eglGetCurrentDisplay();
    
    if (g_secondEGLSurface != EGL_NO_SURFACE && display != EGL_NO_DISPLAY) {
        eglDestroySurface(display, g_secondEGLSurface);
        LOGI("Second EGL surface destroyed");
    }
    g_secondEGLSurface = EGL_NO_SURFACE;
    
    if (g_secondWindow != nullptr) {
        ANativeWindow_release(g_secondWindow);
        g_secondWindow = nullptr;
    }
    
    g_secondReady = false;
    g_secondWidth = 0;
    g_secondHeight = 0;
}

JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_nativeSetSecondScreenResolution(
    JNIEnv* env, jclass clazz, jint width, jint height)
{
    LOGI("nativeSetSecondScreenResolution: %dx%d", width, height);
    g_secondWidth = width;
    g_secondHeight = height;
    
    if (g_secondWindow != nullptr) {
        ANativeWindow_setBuffersGeometry(g_secondWindow, width, height,
                                          WINDOW_FORMAT_RGBA_8888);
    }
}

} // extern "C"

// ============================================================
// Display 2 touch input is now handled by SDL standard path.
// SDL_androidtouch.c receives touch events with distinct
// touchDeviceId per display. STK InputManager routes by
// DeviceID to separate MultitouchDevice instances (P0 / P1).
// ============================================================

// ============================================================
// Public API for the STK render loop
// ============================================================

/**
 * Check if the second display surface is ready for rendering.
 * If the ANativeWindow exists but EGL surface hasn't been created yet,
 * try to create it now (SDL's EGL context should be ready by the time
 * the render loop calls this).
 */
bool dualScreenIsReady()
{
    // Retry EGL surface creation if window exists but surface doesn't
    if (g_secondWindow != nullptr && g_secondEGLSurface == EGL_NO_SURFACE)
    {
        EGLDisplay display = eglGetCurrentDisplay();
        EGLContext context = eglGetCurrentContext();
        
        if (display != EGL_NO_DISPLAY && context != EGL_NO_CONTEXT)
        {
            LOGI("dualScreenIsReady: retrying EGL surface creation (ctx ready)");
            
            // Get EGL config
            EGLint numConfigs = 0;
            EGLConfig configs[16];
            EGLint configAttribs[] = {
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_RED_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_BLUE_SIZE, 8,
                EGL_ALPHA_SIZE, 8,
                EGL_DEPTH_SIZE, 24,
                EGL_STENCIL_SIZE, 8,
                EGL_NONE
            };
            
            eglChooseConfig(display, configAttribs, configs, 16, &numConfigs);
            if (numConfigs > 0)
            {
                // Set buffer geometry
                ANativeWindow_setBuffersGeometry(g_secondWindow, 
                    g_secondWidth > 0 ? g_secondWidth : 1920,
                    g_secondHeight > 0 ? g_secondHeight : 1280,
                    WINDOW_FORMAT_RGBA_8888);
                
                EGLint surfaceAttribs[] = { EGL_NONE };
                g_secondEGLSurface = eglCreateWindowSurface(display, configs[0],
                                                              g_secondWindow, surfaceAttribs);
                
                if (g_secondEGLSurface != EGL_NO_SURFACE)
                {
                    LOGI("dualScreenIsReady: EGL surface created successfully (retry)");
                    g_secondReady = true;
                }
                else
                {
                    EGLint error = eglGetError();
                    LOGW("dualScreenIsReady: EGL surface creation failed: 0x%x", error);
                }
            }
        }
    }
    
    return g_secondReady && g_secondEGLSurface != EGL_NO_SURFACE;
}

/**
 * Get second display dimensions.
 */
void dualScreenGetSize(int* width, int* height)
{
    if (width) *width = g_secondWidth;
    if (height) *height = g_secondHeight;
}

// Saved Display 0 EGL surface for restore after Display 2 rendering
static EGLSurface g_primaryDrawSurface = EGL_NO_SURFACE;
static EGLSurface g_primaryReadSurface = EGL_NO_SURFACE;
static EGLDisplay g_primaryDisplay = EGL_NO_DISPLAY;
static EGLContext g_primaryContext = EGL_NO_CONTEXT;

/**
 * Activate the second EGL surface for rendering Camera 1.
 * Saves Display 0's EGL state so it can be restored later.
 * Returns true on success.
 */
bool dualScreenMakeCurrent()
{
    if (!g_secondReady || g_secondEGLSurface == EGL_NO_SURFACE) {
        return false;
    }
    
    // Save Display 0's EGL state before switching
    g_primaryDisplay = eglGetCurrentDisplay();
    g_primaryDrawSurface = eglGetCurrentSurface(EGL_DRAW);
    g_primaryReadSurface = eglGetCurrentSurface(EGL_READ);
    g_primaryContext = eglGetCurrentContext();
    
    if (g_primaryDisplay == EGL_NO_DISPLAY || g_primaryContext == EGL_NO_CONTEXT) {
        LOGE("dualScreenMakeCurrent: no EGL context on Display 0");
        return false;
    }
    
    // Switch to Display 2
    EGLBoolean result = eglMakeCurrent(g_primaryDisplay, g_secondEGLSurface, 
                                        g_secondEGLSurface, g_primaryContext);
    if (result == EGL_FALSE) {
        EGLint error = eglGetError();
        LOGE("dualScreenMakeCurrent failed: error 0x%x", error);
        return false;
    }
    
    return true;
}

/**
 * Swap buffers on the second display.
 * Call after rendering Camera 1's viewport.
 */
bool dualScreenSwapBuffers()
{
    if (!g_secondReady || g_secondEGLSurface == EGL_NO_SURFACE) {
        return false;
    }
    
    EGLDisplay display = eglGetCurrentDisplay();
    if (display == EGL_NO_DISPLAY) {
        return false;
    }
    
    EGLBoolean result = eglSwapBuffers(display, g_secondEGLSurface);
    if (result == EGL_FALSE) {
        EGLint error = eglGetError();
        LOGE("dualScreenSwapBuffers failed: error 0x%x", error);
        return false;
    }
    
    return true;
}

/**
 * Restore the primary EGL surface (SDL's surface on Display 0).
 * Must be called after dualScreenSwapBuffers() to return to Display 0.
 */
bool dualScreenRestorePrimary()
{
    if (g_primaryDrawSurface == EGL_NO_SURFACE || 
        g_primaryDisplay == EGL_NO_DISPLAY) {
        LOGE("dualScreenRestorePrimary: no saved Display 0 surface");
        return false;
    }
    
    EGLBoolean result = eglMakeCurrent(g_primaryDisplay, g_primaryDrawSurface,
                                        g_primaryReadSurface, g_primaryContext);
    if (result == EGL_FALSE) {
        EGLint error = eglGetError();
        LOGE("dualScreenRestorePrimary failed: error 0x%x", error);
        return false;
    }
    
    return true;
}

// ============================================================
// Frame mirror: copy Display 0 content to Display 2 (GLES2-compatible)
// ============================================================

static GLuint s_mirrorTex = 0;
static int s_mirrorW = 0, s_mirrorH = 0;

// Simple fullscreen quad shaders for GLES2
static const char* s_quadVertSrc =
    "attribute vec2 aPos;"
    "varying vec2 vTexCoord;"
    "void main() {"
    "  vTexCoord = aPos * 0.5 + 0.5;"
    "  gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);"
    "}";

static const char* s_quadFragSrc =
    "precision mediump float;"
    "varying vec2 vTexCoord;"
    "uniform sampler2D uTex;"
    "void main() {"
    "  gl_FragColor = texture2D(uTex, vTexCoord);"
    "}";

static GLuint s_quadProgram = 0;
static GLuint s_quadVBuf = 0;

static void ensureQuadProgram()
{
    if (s_quadProgram != 0) return;
    
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &s_quadVertSrc, NULL);
    glCompileShader(vs);
    
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &s_quadFragSrc, NULL);
    glCompileShader(fs);
    
    s_quadProgram = glCreateProgram();
    glAttachShader(s_quadProgram, vs);
    glAttachShader(s_quadProgram, fs);
    glLinkProgram(s_quadProgram);
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    // Fullscreen quad vertices: two triangles covering [-1,1]
    float quadVerts[] = {
        -1.0f, -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f, -1.0f,   1.0f,  1.0f
    };
    glGenBuffers(1, &s_quadVBuf);
    glBindBuffer(GL_ARRAY_BUFFER, s_quadVBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    LOGI("Mirror quad shader compiled");
}

/**
 * Call BEFORE endScene on Display 0.
 * Captures Display 0's current backbuffer to a texture.
 */
void dualScreenMirrorCapture(int w, int h)
{
    // Ensure EGL surface is created
    if (!g_secondReady) {
        dualScreenIsReady();
    }
    
    if (!g_secondReady || g_secondEGLSurface == EGL_NO_SURFACE) {
        static int s_mirror_fail_count = 0;
        if (++s_mirror_fail_count <= 3)
            LOGW("MirrorCapture: not ready (ready=%d surface=%p)", g_secondReady, (void*)g_secondEGLSurface);
        return;
    }
    
    // Detect framebuffer alpha channel — used for format selection below
    static GLint s_alphaBits = -1;
    if (s_alphaBits < 0) {
        glGetIntegerv(GL_ALPHA_BITS, &s_alphaBits);
        LOGI("MirrorCapture: framebuffer alpha_bits=%d", s_alphaBits);
    }
    
    // Lazy-create mirror texture with correct format
    if (s_mirrorTex == 0 || s_mirrorW != w || s_mirrorH != h)
    {
        if (s_mirrorTex != 0)
            glDeleteTextures(1, &s_mirrorTex);
        
        // Query the framebuffer's alpha channel depth to select matching format.
        // glCopyTexSubImage2D requires source and destination formats to be
        // "compatible" (same base internal format).  If the EGL surface was
        // created without alpha (common on embedded devices), we must use GL_RGB.
        GLenum texFormat = (s_alphaBits > 0) ? GL_RGBA : GL_RGB;
        LOGI("MirrorCapture: using format=%s", (texFormat == GL_RGBA) ? "GL_RGBA" : "GL_RGB");
        
        glGenTextures(1, &s_mirrorTex);
        glBindTexture(GL_TEXTURE_2D, s_mirrorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, texFormat, w, h, 0, texFormat, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        s_mirrorW = w;
        s_mirrorH = h;
        LOGI("MirrorCapture: texture created %dx%d format=%s", w, h,
             (texFormat == GL_RGBA) ? "GL_RGBA" : "GL_RGB");
    }
    
    // Capture Display 0's backbuffer → mirror texture
    glBindTexture(GL_TEXTURE_2D, s_mirrorTex);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
    GLenum captureErr = glGetError();
    if (captureErr != GL_NO_ERROR) {
        LOGW("MirrorCapture: glCopyTexSubImage2D failed 0x%x — falling back to glReadPixels", captureErr);
        // Fallback: use glReadPixels to read framebuffer, then upload via glTexSubImage2D.
        // glReadPixels supports GL_RGBA/GL_RGB independently of framebuffer format.
        GLint readFormat = (s_alphaBits > 0) ? GL_RGBA : GL_RGB;
        // Allocate a small row buffer (read one row at a time to keep memory low)
        int rowSize = w * ((readFormat == GL_RGBA) ? 4 : 3);
        unsigned char* rowBuf = (unsigned char*)malloc(rowSize);
        if (rowBuf) {
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            for (int y = 0; y < h; y++) {
                glReadPixels(0, y, w, 1, readFormat, GL_UNSIGNED_BYTE, rowBuf);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, w, 1, readFormat, GL_UNSIGNED_BYTE, rowBuf);
            }
            free(rowBuf);
            LOGI("MirrorCapture: glReadPixels fallback complete %dx%d", w, h);
        } else {
            LOGE("MirrorCapture: malloc failed for fallback row buffer (%d bytes)", rowSize);
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

/**
 * Call AFTER endScene on Display 0.
 * Draws the captured mirror texture as a fullscreen quad on Display 2.
 */
void dualScreenMirrorPresent()
{
    if (!g_secondReady) {
        dualScreenIsReady();
    }
    
    if (!g_secondReady || s_mirrorTex == 0) {
        static int s_present_fail = 0;
        if (++s_present_fail <= 3)
            LOGW("MirrorPresent: fail (ready=%d tex=%u)", g_secondReady, s_mirrorTex);
        return;
    }
    
    // Save Display 0's EGL state before switching to Display 2
    EGLDisplay eglDisp = eglGetCurrentDisplay();
    EGLSurface primaryDraw = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface primaryRead = eglGetCurrentSurface(EGL_READ);
    EGLContext eglCtx = eglGetCurrentContext();
    
    static int s_present_count = 0;
    s_present_count++;
    if (s_present_count <= 3)
        LOGI("MirrorPresent #%d: disp=%p draw=%p ctx=%p", s_present_count, (void*)eglDisp, (void*)primaryDraw, (void*)eglCtx);
    
    if (!dualScreenMakeCurrent()) {
        if (s_present_count <= 3)
            LOGW("MirrorPresent: dualScreenMakeCurrent failed");
        return;
    }
    
    ensureQuadProgram();
    
    // Clear and draw — DEBUG: alternate red/green to verify composition
    GLfloat r = (s_present_count % 2 == 0) ? 1.0f : 0.0f;
    GLfloat g = (s_present_count % 2 == 0) ? 0.0f : 1.0f;
    glClearColor(r, g, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    GLenum clearErr = glGetError();
    
    GLint oldProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glUseProgram(s_quadProgram);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_mirrorTex);
    glUniform1i(glGetUniformLocation(s_quadProgram, "uTex"), 0);
    
    glBindBuffer(GL_ARRAY_BUFFER, s_quadVBuf);
    GLint aPos = glGetAttribLocation(s_quadProgram, "aPos");
    glVertexAttribPointer(aPos, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(aPos);
    
    glViewport(0, 0, s_mirrorW, s_mirrorH);
    glScissor(0, 0, s_mirrorW, s_mirrorH);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    GLenum drawErr = glGetError();
    
    glDisableVertexAttribArray(aPos);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(oldProgram);
    
    dualScreenSwapBuffers();
    
    // Diagnostic: log GL errors and check if D2 surface is healthy
    if (s_present_count <= 5 && (clearErr != GL_NO_ERROR || drawErr != GL_NO_ERROR))
        LOGW("MirrorPresent #%d: GL errors — clear=0x%x draw=0x%x",
             s_present_count, clearErr, drawErr);
    
    // CRITICAL: Restore Display 0's EGL surface
    if (eglDisp != EGL_NO_DISPLAY && primaryDraw != EGL_NO_SURFACE)
        eglMakeCurrent(eglDisp, primaryDraw, primaryRead, eglCtx);
}

/**
 * Draw an arbitrary GL texture as a fullscreen quad on Display 2.
 * Must be called with Display 2's EGL surface active.
 * Used to present Camera 1's FBO on Display 2 (independent view).
 */
void dualScreenDrawFBO(GLuint texId, int w, int h)
{
    if (!g_secondReady || texId == 0)
    {
        LOGW("dualScreenDrawFBO: not ready (ready=%d tex=%d)", g_secondReady, texId);
        return;
    }
    
    ensureQuadProgram();
    
    // Save state
    GLint oldProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    
    glUseProgram(s_quadProgram);
    
    // Bind the FBO texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texId);
    glUniform1i(glGetUniformLocation(s_quadProgram, "uTex"), 0);
    
    // Draw fullscreen quad
    glBindBuffer(GL_ARRAY_BUFFER, s_quadVBuf);
    GLint aPos = glGetAttribLocation(s_quadProgram, "aPos");
    glVertexAttribPointer(aPos, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(aPos);
    
    glViewport(0, 0, w, h);
    glScissor(0, 0, w, h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    glDisableVertexAttribArray(aPos);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    // Restore state
    glUseProgram(oldProgram);
}
