#ifndef M3DT_GL_WINDOW_H
#define M3DT_GL_WINDOW_H

#include <windows.h>
#include "config.h"

typedef struct GlWindow GlWindow;

/* DPI-aware + bootstrap das funcoes WGL ARB. Idempotente. */
void gl_window_global_init(HINSTANCE hInst);

/* Cria uma janela + contexto GL 3.3 (fallback 3.1/2.1/legado) + uma SceneRenderer
   a partir de `cfg`. NULL em falha. */
GlWindow *gl_window_create(HINSTANCE hInst, DWORD style, DWORD exstyle, HWND parent,
                           int x, int y, int w, int h, const wchar_t *cls, WNDPROC proc,
                           const Config *cfg, int preview);

void  gl_window_frame(GlWindow *g, double t);          /* render 1 frame + SwapBuffers */
/* Liga o fade-in de abertura (so' o saver real; desligado por padrao). */
void  gl_window_set_fade_in(GlWindow *g, int on);
/* Captura da tela (BGRA, linhas de baixo pra cima, w*h*4 bytes) que o fade-in do
   inicio revela por crossfade. Sem isso, o fade parte do preto. */
void  gl_window_set_backdrop(GlWindow *g, const unsigned char *bgra, int w, int h);
void  gl_window_set_config(GlWindow *g, const Config *cfg);
void  gl_window_set_zoom(GlWindow *g, float zoom);     /* 1.0 = enquadramento padrao; >1 aproxima a camera */
void  gl_window_set_debug_view(GlWindow *g, int mode); /* ferramenta de debug visual - ver scene_set_debug_view */
void  gl_window_set_auto_spin(GlWindow *g, int enabled);  /* giro continuo, ignora o pendulo configurado */
void  gl_window_orbit(GlWindow *g, float dyaw_deg, float dpitch_deg);  /* controle manual - assume a camera */
void  gl_window_pan(GlWindow *g, float dx, float dy);                 /* idem */
int   gl_window_vsync(const GlWindow *g);              /* 0/1 efetivo */
void  gl_window_size(const GlWindow *g, int *w, int *h);
HWND  gl_window_hwnd(const GlWindow *g);
HDC   gl_window_dc(const GlWindow *g);
HGLRC gl_window_rc(const GlWindow *g);
void  gl_window_make_current(const GlWindow *g);
void  gl_window_render_scene_at(GlWindow *g, double t);  /* render sem SwapBuffers (p/ captura) */
void  gl_window_destroy(GlWindow *g);

#endif
