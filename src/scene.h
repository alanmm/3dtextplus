#ifndef M3DT_SCENE_H
#define M3DT_SCENE_H
#include "config.h"

typedef struct SceneRenderer SceneRenderer;

/* Cria o renderizador a partir de `cfg`. NULL em falha. Requer contexto GL corrente. */
SceneRenderer *scene_create(const Config *cfg);
void scene_set_config(SceneRenderer *s, const Config *cfg);   /* reconstroi a malha so se preciso */
void scene_set_zoom(SceneRenderer *s, float zoom);   /* 1.0 = enquadramento padrao; >1 aproxima a camera */
void scene_render(SceneRenderer *s, double t, int fb_w, int fb_h);
void scene_destroy(SceneRenderer *s);

#endif
