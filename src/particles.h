#ifndef M3DT_PARTICLES_H
#define M3DT_PARTICLES_H
#include "config.h"
#include "util/mathx.h"

typedef struct ParticleSystem ParticleSystem;

/* Requer contexto GL corrente. NULL em falha. */
ParticleSystem *particles_create(void);

/* hx/hy/hz = meia-extensao atual do texto (mesma unidade de scene.c).
   wall_pos/wall_n = vertices com surfaceId==2 (parede) do MeshData atual,
   em espaco local da malha - usados como pontos de emissao de sparks.
   Pode ser chamado com wall_count == 0 (ainda sem malha). */
void particles_set_config(ParticleSystem *p, const Config *cfg,
                          float hx, float hy, float hz,
                          const v3 *wall_pos, const v3 *wall_n, int wall_count);

/* model = matriz atual do texto (usada so' para nascer sparks em espaco de
   mundo). dt em segundos, ja com clamp feito pelo chamador. */
void particles_update(ParticleSystem *p, float dt, m4 model);

/* view/proj identicas as usadas pelo modelo 3D no mesmo frame; fb_h = altura
   do framebuffer em pixels, usada para o tamanho do point sprite ficar
   consistente entre o mini-preview e a tela cheia (ver particles_render). */
void particles_render(ParticleSystem *p, m4 view, m4 proj, int fb_h);

void particles_destroy(ParticleSystem *p);

#endif
