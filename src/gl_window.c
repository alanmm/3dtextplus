#include "gl_window.h"
#include "gl_core.h"
#include "scene.h"
#include "util/log.h"

#include <glad/gl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* ---------- constantes WGL ARB (nao estao no <glad/gl.h>) ---------- */
#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB      0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001

#define WGL_DRAW_TO_WINDOW_ARB  0x2001
#define WGL_SUPPORT_OPENGL_ARB  0x2010
#define WGL_DOUBLE_BUFFER_ARB   0x2011
#define WGL_PIXEL_TYPE_ARB      0x2013
#define WGL_TYPE_RGBA_ARB       0x202B
#define WGL_COLOR_BITS_ARB      0x2014
#define WGL_DEPTH_BITS_ARB      0x2022
#define WGL_STENCIL_BITS_ARB    0x2023
#define WGL_SAMPLE_BUFFERS_ARB  0x2041
#define WGL_SAMPLES_ARB         0x2042

#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif

typedef HGLRC(WINAPI *PFN_wglCreateContextAttribsARB)(HDC, HGLRC, const int *);
typedef BOOL (WINAPI *PFN_wglChoosePixelFormatARB)(HDC, const int *, const FLOAT *, UINT, int *, UINT *);
typedef BOOL (WINAPI *PFN_wglSwapIntervalEXT)(int);

static PFN_wglCreateContextAttribsARB p_wglCreateContextAttribsARB;
static PFN_wglChoosePixelFormatARB    p_wglChoosePixelFormatARB;
static PFN_wglSwapIntervalEXT         p_wglSwapIntervalEXT;

struct GlWindow {
    HWND  hwnd;
    HDC   dc;
    HGLRC rc;
    int   w, h;
    SceneRenderer *scene;
};

/* ------------------------------------------------------------------ */

static void set_dpi_aware(void)
{
    HMODULE u = GetModuleHandleW(L"user32");
    typedef BOOL(WINAPI * PFN_setctx)(HANDLE);
    PFN_setctx f = u ? (PFN_setctx)(void *)GetProcAddress(u, "SetProcessDpiAwarenessContext") : NULL;
    if (f) {
        if (f((HANDLE)(INT_PTR)-4)) return;   /* PER_MONITOR_AWARE_V2 */
    }
    SetProcessDPIAware();
}

static void wgl_bootstrap(HINSTANCE hInst)
{
    const wchar_t *cls = L"M3DTGLBootstrap";
    WNDCLASSW wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = hInst;
    wc.lpszClassName = cls;
    RegisterClassW(&wc);

    HWND w = CreateWindowW(cls, L"", WS_OVERLAPPED, 0, 0, 1, 1, NULL, NULL, hInst, NULL);
    HDC dc = GetDC(w);

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof pfd);
    pfd.nSize      = sizeof pfd;
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;

    int pf = ChoosePixelFormat(dc, &pfd);
    SetPixelFormat(dc, pf, &pfd);

    HGLRC rc = wglCreateContext(dc);
    wglMakeCurrent(dc, rc);

    p_wglCreateContextAttribsARB =
        (PFN_wglCreateContextAttribsARB)(void *)wglGetProcAddress("wglCreateContextAttribsARB");
    p_wglChoosePixelFormatARB =
        (PFN_wglChoosePixelFormatARB)(void *)wglGetProcAddress("wglChoosePixelFormatARB");
    p_wglSwapIntervalEXT =
        (PFN_wglSwapIntervalEXT)(void *)wglGetProcAddress("wglSwapIntervalEXT");

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(rc);
    ReleaseDC(w, dc);
    DestroyWindow(w);
    UnregisterClassW(cls, hInst);
}

void gl_window_global_init(HINSTANCE hInst)
{
    static bool done = false;
    if (done) return;
    done = true;
    set_dpi_aware();
    wgl_bootstrap(hInst);
}

GlWindow *gl_window_create(HINSTANCE hInst, DWORD style, DWORD exstyle, HWND parent,
                           int x, int y, int w, int h, const wchar_t *cls, WNDPROC proc,
                           const Config *cfg)
{
    GlWindow *g = (GlWindow *)calloc(1, sizeof *g);
    if (!g) return NULL;

    WNDCLASSW wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc   = proc ? proc : DefWindowProcW;
    wc.hInstance     = hInst;
    wc.hCursor       = NULL;
    wc.lpszClassName = cls;
    wc.style         = CS_OWNDC;
    RegisterClassW(&wc);

    g->hwnd = CreateWindowExW(exstyle, cls, L"Modern 3D Text", style,
                              x, y, w, h, parent, NULL, hInst, NULL);
    if (!g->hwnd) { log_errorf("CreateWindowExW falhou (%lu)", GetLastError()); free(g); return NULL; }

    g->dc = GetDC(g->hwnd);

    int pf = 0;
    if (p_wglChoosePixelFormatARB) {
        int msaa[] = { 8, 4, 2, 0 };
        for (int mi = 0; mi < 4 && pf == 0; ++mi) {
            const int attribs[] = {
                WGL_DRAW_TO_WINDOW_ARB, 1,
                WGL_SUPPORT_OPENGL_ARB, 1,
                WGL_DOUBLE_BUFFER_ARB,  1,
                WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
                WGL_COLOR_BITS_ARB,     32,
                WGL_DEPTH_BITS_ARB,     24,
                WGL_STENCIL_BITS_ARB,   8,
                WGL_SAMPLE_BUFFERS_ARB, msaa[mi] ? 1 : 0,
                WGL_SAMPLES_ARB,        msaa[mi],
                0
            };
            UINT n = 0;
            p_wglChoosePixelFormatARB(g->dc, attribs, NULL, 1, &pf, &n);
            if (n == 0) pf = 0;
            else if (msaa[mi]) log_infof("MSAA %dx", msaa[mi]);
        }
    }
    if (!pf) {
        PIXELFORMATDESCRIPTOR pfd;
        memset(&pfd, 0, sizeof pfd);
        pfd.nSize = sizeof pfd; pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32; pfd.cDepthBits = 24; pfd.cStencilBits = 8;
        pf = ChoosePixelFormat(g->dc, &pfd);
    }

    PIXELFORMATDESCRIPTOR chosen;
    DescribePixelFormat(g->dc, pf, sizeof chosen, &chosen);
    SetPixelFormat(g->dc, pf, &chosen);

    const int versions[][2] = { { 3, 3 }, { 3, 1 }, { 2, 1 } };
    for (int i = 0; i < 3 && !g->rc && p_wglCreateContextAttribsARB; ++i) {
        const int cattr[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, versions[i][0],
            WGL_CONTEXT_MINOR_VERSION_ARB, versions[i][1],
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        g->rc = p_wglCreateContextAttribsARB(g->dc, NULL, cattr);
        if (g->rc) log_infof("GL context %d.%d core", versions[i][0], versions[i][1]);
    }
    if (!g->rc) {
        g->rc = wglCreateContext(g->dc);
        if (g->rc) log_infof("GL context legado");
    }
    if (!g->rc) {
        log_errorf("sem contexto GL");
        ReleaseDC(g->hwnd, g->dc);
        DestroyWindow(g->hwnd);
        free(g);
        return NULL;
    }

    wglMakeCurrent(g->dc, g->rc);
    if (p_wglSwapIntervalEXT) p_wglSwapIntervalEXT(1);

    if (!gl_load()) log_errorf("gl_load falhou");
    glEnable(GL_MULTISAMPLE);

    static bool logged_gl = false;
    if (!logged_gl) {
        logged_gl = true;
        log_infof("GL: vendor=%s renderer=%s version=%s",
                  (const char *)glGetString(GL_VENDOR),
                  (const char *)glGetString(GL_RENDERER),
                  (const char *)glGetString(GL_VERSION));
    }

    RECT cr; GetClientRect(g->hwnd, &cr);
    g->w = cr.right; g->h = cr.bottom;

    g->scene = scene_create(cfg);
    if (!g->scene) log_errorf("scene_create falhou (%ls)", cls);
    return g;
}

void gl_window_frame(GlWindow *g, double t)
{
    wglMakeCurrent(g->dc, g->rc);
    RECT cr; GetClientRect(g->hwnd, &cr);
    g->w = cr.right; g->h = cr.bottom;
    if (g->scene) {
        scene_render(g->scene, t, g->w, g->h);
    } else {
        glViewport(0, 0, g->w, g->h);
        glClearColor(0.10f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    SwapBuffers(g->dc);
}

void gl_window_render_scene_at(GlWindow *g, double t)
{
    wglMakeCurrent(g->dc, g->rc);
    RECT cr; GetClientRect(g->hwnd, &cr);
    g->w = cr.right; g->h = cr.bottom;
    if (g->scene) scene_render(g->scene, t, g->w, g->h);
}

void gl_window_set_config(GlWindow *g, const Config *cfg)
{
    if (!g->scene) return;
    wglMakeCurrent(g->dc, g->rc);
    scene_set_config(g->scene, cfg);
}

void gl_window_size(const GlWindow *g, int *w, int *h) { if (w) *w = g->w; if (h) *h = g->h; }
HWND  gl_window_hwnd(const GlWindow *g) { return g->hwnd; }
HDC   gl_window_dc(const GlWindow *g)   { return g->dc; }
HGLRC gl_window_rc(const GlWindow *g)   { return g->rc; }
void  gl_window_make_current(const GlWindow *g) { wglMakeCurrent(g->dc, g->rc); }

void gl_window_destroy(GlWindow *g)
{
    if (!g) return;
    if (g->rc) {
        wglMakeCurrent(g->dc, g->rc);
        if (g->scene) { scene_destroy(g->scene); g->scene = NULL; }
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(g->rc);
    }
    if (g->dc && g->hwnd) ReleaseDC(g->hwnd, g->dc);
    if (g->hwnd) DestroyWindow(g->hwnd);
    free(g);
}
