#include "host_win32.h"
#include "util/log.h"

#include <windowsx.h>
#include <GL/gl.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

/* ---------- constantes WGL ARB (nao estao no <GL/gl.h> do MinGW) ---------- */
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

typedef HGLRC(WINAPI *PFN_wglCreateContextAttribsARB)(HDC, HGLRC, const int *);
typedef BOOL (WINAPI *PFN_wglChoosePixelFormatARB)(HDC, const int *, const FLOAT *, UINT, int *, UINT *);
typedef BOOL (WINAPI *PFN_wglSwapIntervalEXT)(int);

static PFN_wglCreateContextAttribsARB p_wglCreateContextAttribsARB;
static PFN_wglChoosePixelFormatARB    p_wglChoosePixelFormatARB;
static PFN_wglSwapIntervalEXT         p_wglSwapIntervalEXT;

typedef struct { HWND hwnd; HDC dc; HGLRC rc; int w, h; } GlWindow;

/* ------------------------------------------------------------------ */

static void m3dt_set_dpi_aware(void)
{
    HMODULE u = GetModuleHandleW(L"user32");
    typedef BOOL(WINAPI * PFN_setctx)(HANDLE);
    PFN_setctx f = u ? (PFN_setctx)(void *)GetProcAddress(u, "SetProcessDpiAwarenessContext") : NULL;
    if (f) {
        /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4 */
        if (f((HANDLE)(INT_PTR)-4)) return;
    }
    SetProcessDPIAware();
}

static void m3dt_wgl_bootstrap(HINSTANCE hInst)
{
    static bool done = false;
    if (done) return;
    done = true;

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

    log_infof("GL bootstrap: vendor=%s renderer=%s",
              (const char *)glGetString(GL_VENDOR),
              (const char *)glGetString(GL_RENDERER));

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(rc);
    ReleaseDC(w, dc);
    DestroyWindow(w);
    UnregisterClassW(cls, hInst);
}

static int gl_window_create(HINSTANCE hInst, GlWindow *g, DWORD style, DWORD exstyle,
                            HWND parent, int x, int y, int w, int h,
                            const wchar_t *cls, WNDPROC proc)
{
    memset(g, 0, sizeof *g);

    WNDCLASSW wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc   = proc ? proc : DefWindowProcW;
    wc.hInstance     = hInst;
    wc.hCursor       = NULL;
    wc.lpszClassName = cls;
    wc.style         = CS_OWNDC;
    RegisterClassW(&wc);   /* re-registro na 2a chamada retorna 0; ok */

    g->hwnd = CreateWindowExW(exstyle, cls, L"Modern 3D Text", style,
                              x, y, w, h, parent, NULL, hInst, NULL);
    if (!g->hwnd) { log_errorf("CreateWindowExW falhou (%lu)", GetLastError()); return 0; }

    g->dc = GetDC(g->hwnd);

    int pf = 0;
    if (p_wglChoosePixelFormatARB) {
        const int attribs[] = {
            WGL_DRAW_TO_WINDOW_ARB, 1,
            WGL_SUPPORT_OPENGL_ARB, 1,
            WGL_DOUBLE_BUFFER_ARB,  1,
            WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB,     32,
            WGL_DEPTH_BITS_ARB,     24,
            WGL_STENCIL_BITS_ARB,   8,
            0
        };
        UINT n = 0;
        p_wglChoosePixelFormatARB(g->dc, attribs, NULL, 1, &pf, &n);
        if (n == 0) pf = 0;
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
        memset(g, 0, sizeof *g);
        return 0;
    }

    wglMakeCurrent(g->dc, g->rc);
    if (p_wglSwapIntervalEXT) p_wglSwapIntervalEXT(1);

    RECT cr; GetClientRect(g->hwnd, &cr);
    g->w = cr.right; g->h = cr.bottom;
    return 1;
}

static void gl_window_destroy(GlWindow *g)
{
    if (g->rc) { wglMakeCurrent(NULL, NULL); wglDeleteContext(g->rc); }
    if (g->dc && g->hwnd) ReleaseDC(g->hwnd, g->dc);
    if (g->hwnd) DestroyWindow(g->hwnd);
    memset(g, 0, sizeof *g);
}

static void gl_render_clear(GlWindow *g, double t)
{
    wglMakeCurrent(g->dc, g->rc);
    glViewport(0, 0, g->w, g->h);

    double hue = fmod(t * 0.05, 1.0);
    double r  = fabs(hue * 6.0 - 3.0) - 1.0;
    double gg = 2.0 - fabs(hue * 6.0 - 2.0);
    double b  = 2.0 - fabs(hue * 6.0 - 4.0);
    #define M3DT_SAT(x) ((x) < 0.0 ? 0.0 : ((x) > 1.0 ? 1.0 : (x)))
    glClearColor((float)(M3DT_SAT(r) * 0.15), (float)(M3DT_SAT(gg) * 0.15),
                 (float)(M3DT_SAT(b) * 0.15), 1.0f);
    #undef M3DT_SAT

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    SwapBuffers(g->dc);
}

/* host_run_saver / host_run_preview: Tasks 5 e 6. Stubs por ora. */
int host_run_saver(HINSTANCE hInst) { (void)hInst; return 0; }
int host_run_preview(HINSTANCE hInst, HWND parent) { (void)hInst; (void)parent; return 0; }

/* -------- verificacao do Task 4 (headless): removida no Task 5 -------- */
int m3dt_task4_smoke(HINSTANCE hInst);
int m3dt_task4_smoke(HINSTANCE hInst)
{
    log_init();
    m3dt_set_dpi_aware();
    m3dt_wgl_bootstrap(hInst);

    GlWindow g;
    if (!gl_window_create(hInst, &g, WS_OVERLAPPEDWINDOW, 0, NULL,
                          0, 0, 320, 200, L"M3DTSmoke", NULL)) {
        log_errorf("smoke: gl_window_create falhou");
        return 1;
    }

    const char *ver = (const char *)glGetString(GL_VERSION);
    log_infof("smoke: GL_VERSION=%s", ver ? ver : "(null)");

    int rc = 0;
    for (int i = 0; i < 5; ++i)
        gl_render_clear(&g, (double)i * 0.5);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) { log_errorf("smoke: glGetError=0x%04x", (unsigned)err); rc = 1; }
    if (!ver) rc = 1;

    gl_window_destroy(&g);
    log_infof("smoke: fim rc=%d", rc);
    log_shutdown();
    return rc;
}
