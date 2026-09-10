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

#endif
