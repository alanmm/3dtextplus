#include "gl_window.h"
#include "gl_core.h"
#include "scene.h"
#include "post.h"
#include "render_tiers.h"
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
    Post *post;
    PostParams post_params;
    int   preview;
    M3dtTier      tier;
    RenderQuality quality;
    AutoQuality  *aq;
    int   vsync;
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
                           const Config *cfg, int preview)
{
    GlWindow *g = (GlWindow *)calloc(1, sizeof *g);
    if (!g) return NULL;
    g->preview = preview;

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
        free(g);
        return NULL;
    }

    wglMakeCurrent(g->dc, g->rc);
    g->vsync = cfg ? (cfg->vsync ? 1 : 0) : 1;
    if (p_wglSwapIntervalEXT) p_wglSwapIntervalEXT(g->vsync);

    if (!gl_load()) log_errorf("gl_load falhou");
    glEnable(GL_MULTISAMPLE);   /* efetivo nos FBOs MSAA do pos-processamento */

    static bool logged_gl = false;
    if (!logged_gl) {
        logged_gl = true;
        log_infof("GL: vendor=%s renderer=%s version=%s",
                  (const char *)glGetString(GL_VENDOR),
                  (const char *)glGetString(GL_RENDERER),
                  (const char *)glGetString(GL_VERSION));
    }

    /* nivel de qualidade: o mini-preview / o /p nao adaptam (tier FULL fixo) */
    g->tier = g->preview ? M3DT_TIER_FULL
                         : m3dt_tier_resolve((const char *)glGetString(GL_RENDERER));
    {
        Config tmp;
        const Config *qc = cfg;
        if (!qc) { config_defaults(&tmp); qc = &tmp; }
        g->quality = render_quality_for_step(qc, g->tier, 0);
    }

    RECT cr; GetClientRect(g->hwnd, &cr);
    g->w = cr.right; g->h = cr.bottom;

    g->post = post_create();
    if (!g->post) log_errorf("post_create falhou");
    if (cfg) gl_window_set_config(g, cfg);
    else { g->post_params.bloom = 1; g->post_params.threshold = 1.05f;
           g->post_params.intensity = 0.6f; g->post_params.radius = 0.55f;
           g->post_params.streaks_mode = 0; g->post_params.fxaa_on = 1; }

    g->scene = scene_create(cfg);
    if (!g->scene) log_errorf("scene_create falhou (%ls)", cls);
    return g;
}

/* Bloom efetivo: gate de qualidade (tier/ladder) AND toggle do usuario,
   e nunca no modo preview. */
static int frame_bloom(const GlWindow *g)
{
    if (g->preview) return 0;
    return (g->quality.bloom && g->post_params.bloom) ? 1 : 0;
}

static void render_into_post(GlWindow *g, double t, int measure)
{
    RECT cr; GetClientRect(g->hwnd, &cr);
    g->w = cr.right; g->h = cr.bottom;

    RenderQuality q = g->quality;
    int sw = (int)(g->w * q.render_scale + 0.5f); if (sw < 16) sw = 16;
    int sh = (int)(g->h * q.render_scale + 0.5f); if (sh < 16) sh = 16;

    if (measure) aq_frame_begin(g->aq);

    post_begin(g->post, sw, sh, q.msaa);
    if (g->scene) scene_render(g->scene, t, sw, sh);
    else { glClearColor(0.10f, 0.0f, 0.0f, 1.0f); glClear(GL_COLOR_BUFFER_BIT); }

    PostParams pr = g->post_params;
    pr.bloom = frame_bloom(g);
    pr.streaks_mode = (!g->preview && g->quality.streaks && g->post_params.streaks_mode)
                      ? g->post_params.streaks_mode : 0;
    int post_fx_ok = !g->preview && g->tier != M3DT_TIER_REDUCED;
    pr.chroma_on   = (post_fx_ok && g->post_params.chroma_on)   ? 1 : 0;
    pr.vignette_on = (post_fx_ok && g->post_params.vignette_on) ? 1 : 0;
    pr.fxaa_on     = (post_fx_ok && g->post_params.fxaa_on)     ? 1 : 0;
    post_present(g->post, g->w, g->h, pr);

    if (measure) {
        aq_frame_end(g->aq);
        RenderQuality nq = g->quality;
        if (aq_update(g->aq, &nq)) {
            g->quality = nq;
            log_infof("autoQuality: step %d (streaks=%d bloom=%d msaa=%d scale=%.2f)",
                      nq.step, nq.streaks, nq.bloom, nq.msaa, (double)nq.render_scale);
        }
    }
}

void gl_window_frame(GlWindow *g, double t)
{
    wglMakeCurrent(g->dc, g->rc);
    render_into_post(g, t, 1);
    SwapBuffers(g->dc);
}

void gl_window_render_scene_at(GlWindow *g, double t)
{
    wglMakeCurrent(g->dc, g->rc);
    render_into_post(g, t, 0);   /* frame de captura: fora do orcamento do aq */
}

void gl_window_set_config(GlWindow *g, const Config *cfg)
{
    g->post_params.bloom     = cfg->bloom_on;
    g->post_params.threshold = cfg->bloom_threshold;
    g->post_params.intensity = cfg->bloom_intensity;
    g->post_params.radius    = cfg->bloom_radius;

    g->post_params.streaks_mode      = cfg->streaks_mode;
    g->post_params.streaks_intensity = cfg->streaks_intensity;
    g->post_params.streaks_length    = cfg->streaks_length;

    g->post_params.chroma_on        = cfg->chroma_on;
    g->post_params.chroma_strength  = cfg->chroma_strength;
    g->post_params.vignette_on      = cfg->vignette_on;
    g->post_params.vignette_amount  = cfg->vignette_amount;
    g->post_params.fxaa_on          = cfg->fxaa_on;

    if (g->rc) wglMakeCurrent(g->dc, g->rc);

    g->vsync = cfg->vsync ? 1 : 0;
    if (g->rc && p_wglSwapIntervalEXT) p_wglSwapIntervalEXT(g->vsync);

    /* auto-qualidade: recria com o cfg novo (comeca no topo); sem, so o topo */
    if (!g->preview) {
        if (g->aq) { aq_destroy(g->aq); g->aq = NULL; }
        if (cfg->auto_quality && g->rc) g->aq = aq_create(cfg, g->tier);
    }
    g->quality = render_quality_for_step(cfg, g->tier, 0);

    if (!g->scene) return;
    scene_set_config(g->scene, cfg);
}

void gl_window_size(const GlWindow *g, int *w, int *h) { if (w) *w = g->w; if (h) *h = g->h; }
HWND  gl_window_hwnd(const GlWindow *g) { return g->hwnd; }
HDC   gl_window_dc(const GlWindow *g)   { return g->dc; }
HGLRC gl_window_rc(const GlWindow *g)   { return g->rc; }
void  gl_window_make_current(const GlWindow *g) { wglMakeCurrent(g->dc, g->rc); }
int   gl_window_vsync(const GlWindow *g) { return g->vsync; }

void gl_window_destroy(GlWindow *g)
{
    if (!g) return;
    if (g->rc) {
        wglMakeCurrent(g->dc, g->rc);
        if (g->scene) { scene_destroy(g->scene); g->scene = NULL; }
        if (g->post)  { post_destroy(g->post);  g->post  = NULL; }
        if (g->aq)    { aq_destroy(g->aq);      g->aq    = NULL; }
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(g->rc);
    }
    if (g->dc && g->hwnd) ReleaseDC(g->hwnd, g->dc);
    if (g->hwnd) DestroyWindow(g->hwnd);
    free(g);
}
