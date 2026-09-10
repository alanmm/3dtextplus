#include "host_win32.h"
#include "gl_core.h"
#include "util/log.h"

#include <windowsx.h>
#include <glad/gl.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

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

    if (!gl_load()) log_errorf("gl_load falhou");

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

/* ---------------- modo saver ---------------- */

static volatile LONG g_quit;
static POINT g_mouse_anchor;
static bool  g_mouse_anchored;

static void request_quit(void) { InterlockedExchange(&g_quit, 1); }

static bool env_flag(const char *name)
{
    char buf[8];
    DWORD k = GetEnvironmentVariableA(name, buf, sizeof buf);
    return k > 0 && k < sizeof buf && buf[0] != '0';
}

static LRESULT CALLBACK saver_wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
        case WM_MOUSEMOVE: {
            POINT p; GetCursorPos(&p);
            if (!g_mouse_anchored) { g_mouse_anchor = p; g_mouse_anchored = true; break; }
            long dx = p.x - g_mouse_anchor.x, dy = p.y - g_mouse_anchor.y;
            if (dx * dx + dy * dy > 16) request_quit();   /* > 4 px */
            break;
        }
        case WM_KEYDOWN: case WM_SYSKEYDOWN:
        case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
        case WM_MOUSEWHEEL:
            request_quit();
            break;
        case WM_CLOSE: case WM_DESTROY:
            request_quit();
            return 0;
        case WM_SETCURSOR:
            SetCursor(NULL);
            return TRUE;
    }
    return DefWindowProcW(h, m, w, l);
}

typedef struct { RECT r[16]; int n; } MonitorList;

static BOOL CALLBACK monitor_cb(HMONITOR mon, HDC dc, LPRECT rc, LPARAM lp)
{
    (void)mon; (void)dc;
    MonitorList *ml = (MonitorList *)lp;
    if (ml->n < 16) ml->r[ml->n++] = *rc;
    return TRUE;
}

int host_run_saver(HINSTANCE hInst)
{
    m3dt_set_dpi_aware();
    m3dt_wgl_bootstrap(hInst);

    const bool selftest = env_flag("M3DT_SELFTEST");   /* dev/CI: janela + auto-saida */

    MonitorList ml; ml.n = 0;
    if (!selftest)
        EnumDisplayMonitors(NULL, NULL, monitor_cb, (LPARAM)&ml);
    if (ml.n == 0) {
        ml.r[0].left = 0; ml.r[0].top = 0;
        ml.r[0].right  = selftest ? 640 : GetSystemMetrics(SM_CXSCREEN);
        ml.r[0].bottom = selftest ? 400 : GetSystemMetrics(SM_CYSCREEN);
        ml.n = 1;
    }

    const DWORD style   = selftest ? (WS_OVERLAPPEDWINDOW | WS_VISIBLE) : (WS_POPUP | WS_VISIBLE);
    const DWORD exstyle = selftest ? 0u : WS_EX_TOPMOST;

    GlWindow win[16]; int nwin = 0;
    for (int i = 0; i < ml.n; ++i) {
        RECT r = ml.r[i];
        wchar_t cls[32]; wsprintfW(cls, L"M3DTSaver%d", i);
        if (gl_window_create(hInst, &win[nwin], style, exstyle, NULL,
                             r.left, r.top, r.right - r.left, r.bottom - r.top,
                             cls, saver_wndproc)) {
            if (!selftest)
                SetWindowPos(win[nwin].hwnd, HWND_TOPMOST, r.left, r.top,
                             r.right - r.left, r.bottom - r.top, SWP_SHOWWINDOW);
            nwin++;
        }
    }
    if (nwin == 0) { log_errorf("nenhuma janela saver criada"); return 1; }

    if (!selftest) ShowCursor(FALSE);
    SetForegroundWindow(win[0].hwnd);

    timeBeginPeriod(1);
    LARGE_INTEGER freq, start;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    long frame = 0;
    while (!g_quit) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) request_quit();
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        double t = (double)(now.QuadPart - start.QuadPart) / (double)freq.QuadPart;
        for (int i = 0; i < nwin; ++i) gl_render_clear(&win[i], t);
        Sleep(1);
        if (selftest && ++frame >= 90) request_quit();
    }

    timeEndPeriod(1);
    if (!selftest) ShowCursor(TRUE);
    for (int i = 0; i < nwin; ++i) gl_window_destroy(&win[i]);
    log_infof("saver encerrou (frames=%ld)", frame);
    return 0;
}

/* ---------------- modo preview ---------------- */

int host_run_preview(HINSTANCE hInst, HWND parent)
{
    if (!IsWindow(parent)) { log_errorf("preview: parent invalido"); return 1; }

    m3dt_set_dpi_aware();
    m3dt_wgl_bootstrap(hInst);

    RECT pr; GetClientRect(parent, &pr);
    GlWindow g;
    if (!gl_window_create(hInst, &g, WS_CHILD | WS_VISIBLE, 0, parent,
                          0, 0, pr.right, pr.bottom, L"M3DTPreview", DefWindowProcW))
        return 1;

    LARGE_INTEGER freq, start;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    log_infof("preview: parent=%p client=%ldx%ld", (void *)parent, pr.right, pr.bottom);

    for (;;) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!IsWindow(parent)) break;

        RECT c; GetClientRect(parent, &c);
        if (c.right != g.w || c.bottom != g.h) {
            SetWindowPos(g.hwnd, NULL, 0, 0, c.right, c.bottom, SWP_NOZORDER | SWP_NOACTIVATE);
            g.w = c.right; g.h = c.bottom;
        }

        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        double t = (double)(now.QuadPart - start.QuadPart) / (double)freq.QuadPart;
        gl_render_clear(&g, t);
        Sleep(16);
    }

    gl_window_destroy(&g);
    log_infof("preview encerrou");
    return 0;
}
