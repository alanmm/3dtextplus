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

/* ---------------- auto-qualidade adaptativa ---------------- */

typedef struct AutoQuality AutoQuality;

/* cfg + tier definem o ladder. Comeca no step 0. NULL se malloc falhar.
   Requer contexto GL corrente (cria os timer queries). */
AutoQuality *aq_create(const Config *cfg, M3dtTier tier);
void         aq_destroy(AutoQuality *aq);            /* com contexto corrente */

/* Em volta do render de 1 frame (scene + post). No-op se aq == NULL ou
   o timer query nao existe. */
void aq_frame_begin(AutoQuality *aq);
void aq_frame_end(AutoQuality *aq);

/* Depois do aq_frame_end. Se mudou de passo, escreve o novo RenderQuality
   em *io e retorna 1; senao retorna 0. */
int  aq_update(AutoQuality *aq, RenderQuality *io);

/* --- decisao pura (testavel) --- */
typedef struct {
    int    step;            /* passo atual */
    int    ladder_len;      /* render_quality_ladder_len(tier) */
    double p90_ms;          /* percentil 90 da janela de tempos de GPU */
    double since_change_s;  /* segundos desde a ultima mudanca de passo */
} AqInput;

/* Sobe de passo (degrada) se p90 > 22 ms e since_change >= 5 s;
   desce de passo se step>0 e p90 < 12 ms e since_change >= 5 s;
   senao mantem. */
int  aq_decide(AqInput in);

#endif
