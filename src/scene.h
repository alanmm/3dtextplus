#ifndef M3DT_SCENE_H
#define M3DT_SCENE_H
#include "config.h"

typedef struct SceneRenderer SceneRenderer;

/* Cria o renderizador a partir de `cfg`. NULL em falha. Requer contexto GL corrente. */
SceneRenderer *scene_create(const Config *cfg);
void scene_set_config(SceneRenderer *s, const Config *cfg);   /* reconstroi a malha so se preciso */
void scene_set_zoom(SceneRenderer *s, float zoom);   /* 1.0 = enquadramento padrao; >1 aproxima a camera */
void scene_set_auto_spin(SceneRenderer *s, int enabled);   /* giro continuo, ignora o pendulo configurado */
/* Controle manual de camera (arrastar/pan do mouse): a primeira chamada a
   qualquer uma das duas assume o controle (desliga auto_spin e o pendulo
   configurado) ate a SceneRenderer ser recriada. */
void scene_orbit(SceneRenderer *s, float dyaw_deg, float dpitch_deg);
void scene_pan(SceneRenderer *s, float dx, float dy);
void scene_render(SceneRenderer *s, double t, int fb_w, int fb_h, int particles_active);
void scene_destroy(SceneRenderer *s);

#endif
