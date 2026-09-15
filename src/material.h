#ifndef M3DT_MATERIAL_H
#define M3DT_MATERIAL_H
#include "util/mathx.h"

typedef struct {
    unsigned prog;
    int uModel, uView, uProj, uCamPos, uBaseColor;
    int uMode, uMetalness, uRoughness, uEnvTex, uHasEnv;
} Material;

int  material_init(Material *m);                                   /* 0 = falha */
void material_begin(const Material *m, m4 view, m4 proj, v3 campos, v3 base);
void material_set_style(const Material *m, int mode, float metalness, float roughness,
                        unsigned env_tex /* 0 = nenhuma */);
void material_set_piece_color(const Material *m, v3 color);
void material_set_model(const Material *m, m4 model);
void material_destroy(Material *m);

#endif
