#ifndef M3DT_RENDER_TIERS_H
#define M3DT_RENDER_TIERS_H

#include "config.h"

typedef enum { M3DT_TIER_FULL = 0, M3DT_TIER_REDUCED = 1 } M3dtTier;

/* Pura: classifica pela string de GL_RENDERER (case-insensitive).
   NULL / "" -> FULL. Renderers de software / WARP / GDI -> REDUCED. */
M3dtTier    m3dt_tier_detect(const char *gl_renderer);

/* Le M3DT_FORCE_TIER ("full" | "reduced", case-insensitive).
   1 e escreve *out se setado; 0 caso contrario. */
int         m3dt_tier_forced(M3dtTier *out);

/* GetSystemMetrics(SM_REMOTESESSION) != 0 */
int         m3dt_is_remote_session(void);

const char *m3dt_tier_name(M3dtTier t);   /* "full" | "reduced" */

/* Combina tudo (env > RDP > string) e loga a decisao. */
M3dtTier    m3dt_tier_resolve(const char *gl_renderer);

/* ---------------- qualidade efetiva ---------------- */

typedef struct {
    int   bloom;         /* 0/1: gate de qualidade (o gl_window faz AND com cfg.bloom_on) */
    int   msaa;          /* 0 | 2 | 4 | 8 efetivo */
    float render_scale;  /* 0.5 .. 1.0 efetivo */
    int   step;          /* posicao no ladder; 0 = topo (melhor) */
} RenderQuality;

/* Pura. step 0 = (bloom=1, msaa=cfg.msaa, scale=cfg.render_scale).
   Cada passo piora um item (ver o ladder no plano da Fase 4b).
   Em REDUCED o step 0 ja vem degradado (bloom 0, msaa<=2, scale<=0.75). */
RenderQuality render_quality_for_step(const Config *cfg, M3dtTier tier, int step);

/* Numero de passos validos (>= 1). step e clampado a [0, len-1]. */
int           render_quality_ladder_len(M3dtTier tier);

#endif
