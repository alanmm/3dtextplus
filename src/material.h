#ifndef M3DT_MATERIAL_H
#define M3DT_MATERIAL_H
#include "util/mathx.h"

typedef struct {
    unsigned prog;
    int uModel, uView, uProj, uCamPos, uBaseColor;
} Material;

int  material_init(Material *m);                                   /* 0 = falha */
void material_begin(const Material *m, m4 view, m4 proj, v3 campos, v3 base);
void material_set_model(const Material *m, m4 model);
void material_destroy(Material *m);

#endif
