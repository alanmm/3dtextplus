#include "host_win32.h"
#include "gl_window.h"
#include "gl_core.h"
#include "scene.h"
#include "config.h"
#include "i18n.h"
#include "util/log.h"

#include <glad/gl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

static LRESULT CALLBACK cover_wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
        case WM_ERASEBKGND:
            return 1;
        case WM_CLOSE: case WM_DESTROY:
            return DefWindowProcW(h, m, w, l);   /* destruir a capa nao pode encerrar o saver */
    }
    return saver_wndproc(h, m, w, l);
}

/* Capa: janela em camadas (layered), topmost, que ja nasce com a captura da tela
   (UpdateLayeredWindow antes do show => sem quadro preto no primeiro desenho). */
static HWND create_cover(HINSTANCE hInst, RECT r, const unsigned char *bgra)
{
    int w = r.right - r.left, h = r.bottom - r.top;
    HWND hw = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
                              L"M3DTCover", L"", WS_POPUP, r.left, r.top, w, h, NULL, NULL, hInst, NULL);
    if (!hw) return NULL;

    HDC sdc = GetDC(NULL);
    HDC mdc = CreateCompatibleDC(sdc);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize        = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = h;   /* bottom-up, igual ao formato da captura */
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    HBITMAP bmp = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    BOOL ok = FALSE;
    if (bmp && bits) {
        memcpy(bits, bgra, (size_t)w * (size_t)h * 4u);
        HGDIOBJ old = SelectObject(mdc, bmp);
        POINT pd = { r.left, r.top }, ps = { 0, 0 };
        SIZE sz = { w, h };
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, 0 };
        ok = UpdateLayeredWindow(hw, sdc, &pd, &sz, mdc, &ps, 0, &bf, ULW_ALPHA);
        SelectObject(mdc, old);
    }
    if (bmp) DeleteObject(bmp);
    DeleteDC(mdc);
    ReleaseDC(NULL, sdc);
    if (!ok) { DestroyWindow(hw); return NULL; }

    SetWindowPos(hw, HWND_TOPMOST, r.left, r.top, w, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    return hw;
}

/* Capa quase invisivel (1/255 de opacidade) em vez de destruida: uma janela GL
   topmost que cobre o monitor inteiro sozinha e' promovida pelo Windows a "flip
   independente", e essa troca de modo de apresentacao pisca preto. Com a capa por
   cima o DWM segue compondo e a troca nunca acontece. */
static void cover_go_ghost(HWND hw)
{
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 1, 0 };
    UpdateLayeredWindow(hw, NULL, NULL, NULL, NULL, NULL, 0, &bf, ULW_ALPHA);
}

typedef struct { RECT r[16]; int n; } MonitorList;

static BOOL CALLBACK monitor_cb(HMONITOR mon, HDC dc, LPRECT rc, LPARAM lp)
{
    (void)mon; (void)dc;
    MonitorList *ml = (MonitorList *)lp;
    if (ml->n < 16) ml->r[ml->n++] = *rc;
    return TRUE;
}

/* Captura o retangulo `r` da tela (coordenadas virtuais, processo DPI-aware)
   como BGRA bottom-up, pronto pra textura GL. NULL em falha. */
static unsigned char *capture_screen_rect(RECT r)
{
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w < 1 || h < 1) return NULL;

    HDC sdc = GetDC(NULL);
    HDC mdc = sdc ? CreateCompatibleDC(sdc) : NULL;
    unsigned char *out = NULL;
    if (mdc) {
        BITMAPINFO bi; memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize        = sizeof bi.bmiHeader;
        bi.bmiHeader.biWidth       = w;
        bi.bmiHeader.biHeight      = h;   /* positivo = bottom-up, igual a origem da textura GL */
        bi.bmiHeader.biPlanes      = 1;
        bi.bmiHeader.biBitCount    = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void *bits = NULL;
        HBITMAP bmp = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (bmp && bits) {
            HGDIOBJ old = SelectObject(mdc, bmp);
            if (BitBlt(mdc, 0, 0, w, h, sdc, r.left, r.top, SRCCOPY | CAPTUREBLT)) {
                out = (unsigned char *)malloc((size_t)w * (size_t)h * 4u);
                if (out) memcpy(out, bits, (size_t)w * (size_t)h * 4u);
            }
            SelectObject(mdc, old);
        }
        if (bmp) DeleteObject(bmp);
        DeleteDC(mdc);
    }
    if (sdc) ReleaseDC(NULL, sdc);
    if (!out) log_errorf("captura de tela falhou (%d,%d %dx%d) - fade-in parte do preto", (int)r.left, (int)r.top, w, h);
    return out;
}

/* Luminancia media (0..1) por amostragem esparsa de uma captura BGRA - so' pra log */
static double capture_mean_lum(const unsigned char *p, int w, int h)
{
    double sum = 0.0; long n = 0;
    for (long i = 0; i < (long)w * h; i += 97) {
        sum += (p[i * 4] + p[i * 4 + 1] + p[i * 4 + 2]) / 765.0;
        ++n;
    }
    return n ? sum / (double)n : 0.0;
}

int host_run_saver(HINSTANCE hInst)
{
    gl_window_global_init(hInst);

    const bool selftest = env_flag("M3DT_SELFTEST");

    Config cfg;
    config_load(&cfg);
    i18n_init(cfg.ui_language);

    MonitorList ml; ml.n = 0;
    if (!selftest)
        EnumDisplayMonitors(NULL, NULL, monitor_cb, (LPARAM)&ml);
    if (ml.n == 0) {
        ml.r[0].left = 0; ml.r[0].top = 0;
        ml.r[0].right  = selftest ? 640 : GetSystemMetrics(SM_CXSCREEN);
        ml.r[0].bottom = selftest ? 400 : GetSystemMetrics(SM_CYSCREEN);
        ml.n = 1;
    }

    LARGE_INTEGER t_freq, t_begin;
    QueryPerformanceFrequency(&t_freq);
    QueryPerformanceCounter(&t_begin);

    /* Captura cada monitor ANTES de qualquer janela do saver existir; o fade-in
       revela a cena por cima dessa imagem em vez de partir do preto.
       Limite: quando o proprio Windows inicia o saver (inatividade, SC_SCREENSAVE) ele
       roda na area de trabalho "Screen-saver", vazia e escura, e o BitBlt do desktop
       "Default" e' negado dali - o fade parte dessa cor. Rodando "/s" direto (area
       "Default") a captura e' do desktop real. */
    unsigned char *shots[16] = { NULL };
    wchar_t dn[64] = L"?";
    {
        DWORD need = 0;
        HDESK hd = GetThreadDesktop(GetCurrentThreadId());
        if (hd) GetUserObjectInformationW(hd, UOI_NAME, dn, sizeof dn, &need);
        log_infof("saver: desktop='%ls' monitores=%d selftest=%d", dn, ml.n, (int)selftest);
    }
    for (int i = 0; i < ml.n; ++i) {
        shots[i] = capture_screen_rect(ml.r[i]);
        if (shots[i])
            log_infof("saver: captura mon%d %dx%d lum=%.3f", i, (int)(ml.r[i].right - ml.r[i].left),
                      (int)(ml.r[i].bottom - ml.r[i].top),
                      capture_mean_lum(shots[i], ml.r[i].right - ml.r[i].left, ml.r[i].bottom - ml.r[i].top));
    }

    /* "Capas": janelas em camadas que mostram a propria captura, em cima de
       tudo. Uma janela GL recem-mostrada fica preta ate o 1o SwapBuffers
       depois do show; as janelas GL entram escondidas atras das capas e so'
       ficam descobertas, ja com a captura apresentada, quando a capa fica
       quase transparente (ver cover_go_ghost). */
    HWND cover[16] = { NULL };
    {
        WNDCLASSW cc; memset(&cc, 0, sizeof cc);
        cc.lpfnWndProc   = cover_wndproc;
        cc.hInstance     = hInst;
        cc.lpszClassName = L"M3DTCover";
        RegisterClassW(&cc);
        for (int i = 0; i < ml.n; ++i) {
            if (!shots[i]) continue;
            cover[i] = create_cover(hInst, ml.r[i], shots[i]);
        }
    }
    LARGE_INTEGER t_covers; QueryPerformanceCounter(&t_covers);

    const DWORD style   = selftest ? (WS_OVERLAPPEDWINDOW | WS_VISIBLE) : WS_POPUP;
    const DWORD exstyle = selftest ? 0u : WS_EX_TOPMOST;

    GlWindow *win[16]; int nwin = 0;
    for (int i = 0; i < ml.n; ++i) {
        RECT r = ml.r[i];
        wchar_t cls[32]; wsprintfW(cls, L"M3DTSaver%d", i);
        GlWindow *g = gl_window_create(hInst, style, exstyle, NULL,
                                       r.left, r.top, r.right - r.left, r.bottom - r.top,
                                       cls, saver_wndproc, &cfg, 0);
        if (!g) continue;
        gl_window_set_fade_in(g, 1);
        if (shots[i]) gl_window_set_backdrop(g, shots[i], r.right - r.left, r.bottom - r.top);
        win[nwin++] = g;
        gl_window_frame(g, 0.0);   /* frame 0, ainda oculto: absorve o custo da 1a renderizacao */
        if (!selftest)
            SetWindowPos(gl_window_hwnd(g), cover[i] ? cover[i] : HWND_TOPMOST,
                         r.left, r.top, r.right - r.left, r.bottom - r.top,
                         SWP_SHOWWINDOW | SWP_NOACTIVATE);   /* logo abaixo da capa */
        gl_window_frame(g, 0.0);   /* mostrada atras da capa: so' a captura; ancora o fade em t=0 */
        gl_window_frame(g, 0.0);   /* um swap a mais garante a captura ja apresentada pelo DWM */
    }
    for (int i = 0; i < ml.n; ++i) {
        if (cover[i]) cover_go_ghost(cover[i]);
        free(shots[i]);
    }
    if (nwin == 0) {
        log_errorf("nenhuma janela saver criada");
        for (int i = 0; i < ml.n; ++i) if (cover[i]) DestroyWindow(cover[i]);
        return 1;
    }
    {
        LARGE_INTEGER t_ready; QueryPerformanceCounter(&t_ready);
        log_infof("saver: init capas=%.0f ms, janelas GL prontas=%.0f ms",
                  (double)(t_covers.QuadPart - t_begin.QuadPart) * 1000.0 / (double)t_freq.QuadPart,
                  (double)(t_ready.QuadPart - t_begin.QuadPart) * 1000.0 / (double)t_freq.QuadPart);
    }

    {
        /* diagnostico: forca uma visualizacao de debug sem precisar clicar
           no dialogo - so' assim da' pra capturar via M3DT_SHOT headless. */
        char dv[8];
        if (GetEnvironmentVariableA("M3DT_DEBUG_VIEW", dv, sizeof dv) > 0) {
            int mode = atoi(dv);
            for (int i = 0; i < nwin; ++i) gl_window_set_debug_view(win[i], mode);
        }
    }

    if (!selftest) ShowCursor(FALSE);
    {
        /* foco no que esta no topo (a capa), pra ativar nao subir a janela GL
           por cima dela */
        HWND fg = cover[0] ? cover[0] : gl_window_hwnd(win[0]);
        BOOL ok = SetForegroundWindow(fg);
        log_infof("saver: SetForegroundWindow(%s)=%d, em primeiro plano agora=%d", cover[0] ? "capa" : "GL",
                  (int)ok, (int)(GetForegroundWindow() == fg));
    }

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
    for (int i = 0; i < ml.n; ++i) if (cover[i]) DestroyWindow(cover[i]);
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
    i18n_init(cfg.ui_language);

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
