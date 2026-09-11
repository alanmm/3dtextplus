#include "host_win32.h"
#include "gl_window.h"
#include "gl_core.h"
#include "scene.h"
#include "config.h"
#include "util/log.h"

#include <glad/gl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "stb_image_write.h"

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
    gl_window_global_init(hInst);

    const bool selftest = env_flag("M3DT_SELFTEST");

    Config cfg;
    config_load(&cfg);

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

    GlWindow *win[16]; int nwin = 0;
    for (int i = 0; i < ml.n; ++i) {
        RECT r = ml.r[i];
        wchar_t cls[32]; wsprintfW(cls, L"M3DTSaver%d", i);
        GlWindow *g = gl_window_create(hInst, style, exstyle, NULL,
                                       r.left, r.top, r.right - r.left, r.bottom - r.top,
                                       cls, saver_wndproc, &cfg, 0);
        if (g) {
            if (!selftest)
                SetWindowPos(gl_window_hwnd(g), HWND_TOPMOST, r.left, r.top,
                             r.right - r.left, r.bottom - r.top, SWP_SHOWWINDOW);
            win[nwin++] = g;
        }
    }
    if (nwin == 0) { log_errorf("nenhuma janela saver criada"); return 1; }

    if (!selftest) ShowCursor(FALSE);
    SetForegroundWindow(gl_window_hwnd(win[0]));

    log_infof("saver: fps_cap=%d vsync=%d msaa=%d render_scale=%.2f auto_quality=%d",
              cfg.fps_cap, cfg.vsync, cfg.msaa, (double)cfg.render_scale, cfg.auto_quality);

    char shot[MAX_PATH]; shot[0] = 0;
    GetEnvironmentVariableA("M3DT_SHOT", shot, sizeof shot);

    /* teto de frame: fps_cap 0 => sem alvo (deixa o vsync/loop mandar).
       nao busca precisao de vsync; so evita fritar a GPU quando o vsync esta off. */
    const double frame_ms = cfg.fps_cap > 0 ? 1000.0 / (double)cfg.fps_cap : 0.0;

    timeBeginPeriod(1);
    LARGE_INTEGER freq, start;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    long frame = 0;
    while (!g_quit) {
        LARGE_INTEGER iter_start; QueryPerformanceCounter(&iter_start);
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) request_quit();
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        double t = (double)(now.QuadPart - start.QuadPart) / (double)freq.QuadPart;
        for (int i = 0; i < nwin; ++i) gl_window_frame(win[i], t);
        ++frame;

        if (frame_ms > 0.0) {
            LARGE_INTEGER e; QueryPerformanceCounter(&e);
            double used = (double)(e.QuadPart - iter_start.QuadPart) * 1000.0 / (double)freq.QuadPart;
            double rest = frame_ms - used;
            if (rest > 1.5) Sleep((DWORD)(rest - 0.5));
            else Sleep(0);
        } else {
            Sleep(1);
        }

        if (shot[0] && frame == 60) {
            double shot_t = 2.25;
            char stbuf[16];
            if (GetEnvironmentVariableA("M3DT_SHOT_T", stbuf, sizeof stbuf) > 0)
                shot_t = atof(stbuf);

            /* M3DT_SHOT so captura a janela do monitor 0; M3DT_SHOT2..M3DT_SHOT16
               capturam as demais (diagnostico multi-monitor - nao usado no dia a dia). */
            for (int wi = 0; wi < nwin; ++wi) {
                char path[MAX_PATH];
                if (wi == 0) {
                    snprintf(path, sizeof path, "%s", shot);
                } else {
                    char envname[24];
                    snprintf(envname, sizeof envname, "M3DT_SHOT%d", wi + 1);
                    if (GetEnvironmentVariableA(envname, path, sizeof path) == 0) continue;
                }
                gl_window_render_scene_at(win[wi], shot_t);
                int W = 0, H = 0;
                gl_window_size(win[wi], &W, &H);
                unsigned char *px = (unsigned char *)malloc((size_t)W * H * 3);
                if (px) {
                    glPixelStorei(GL_PACK_ALIGNMENT, 1);
                    glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, px);
                    stbi_flip_vertically_on_write(1);
                    stbi_write_png(path, W, H, 3, px, W * 3);
                    free(px);
                    log_infof("shot salvo em %s (%dx%d, t=%.2f, monitor %d)", path, W, H, shot_t, wi);
                }
            }
            request_quit();
        }
        if (selftest && frame >= 90) request_quit();
    }

    timeEndPeriod(1);
    if (!selftest) ShowCursor(TRUE);
    for (int i = 0; i < nwin; ++i) gl_window_destroy(win[i]);
    log_infof("saver encerrou (frames=%ld)", frame);
    return 0;
}

/* ---------------- modo preview ---------------- */

int host_run_preview(HINSTANCE hInst, HWND parent)
{
    if (!IsWindow(parent)) { log_errorf("preview: parent invalido"); return 1; }

    gl_window_global_init(hInst);

    Config cfg;
    config_load(&cfg);

    RECT pr; GetClientRect(parent, &pr);
    GlWindow *g = gl_window_create(hInst, WS_CHILD | WS_VISIBLE, 0, parent,
                                   0, 0, pr.right, pr.bottom, L"M3DTPreview", DefWindowProcW, &cfg, 1);
    if (!g) return 1;

    LARGE_INTEGER freq, start;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    log_infof("preview: parent=%p client=%ldx%ld", (void *)parent, pr.right, pr.bottom);

    int last_w = pr.right, last_h = pr.bottom;
    for (;;) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!IsWindow(parent)) break;

        RECT c; GetClientRect(parent, &c);
        if (c.right != last_w || c.bottom != last_h) {
            SetWindowPos(gl_window_hwnd(g), NULL, 0, 0, c.right, c.bottom,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            last_w = c.right; last_h = c.bottom;
        }

        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        double t = (double)(now.QuadPart - start.QuadPart) / (double)freq.QuadPart;
        gl_window_frame(g, t);
        Sleep(16);
    }

    gl_window_destroy(g);
    log_infof("preview encerrou");
    return 0;
}
