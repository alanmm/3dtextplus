#ifndef M3DT_SDF_H
#define M3DT_SDF_H
#include "geometry/geom_types.h"

typedef struct {
    float *dist;   /* res*res, distancia assinada em unidades de em (< 0 dentro) */
    float *gx;     /* res*res, gradiente x normalizado (aponta para fora) */
    float *gy;     /* res*res */
    int    res;
    float  min_x, min_y;    /* canto local (em) da grade */
    float  size_x, size_y;  /* extensao local (em) coberta pela grade */
} Sdf;

int   sdf_build(const ContourSet *cs, int res, Sdf *out);   /* 0 = falha */
void  sdf_free(Sdf *out);
float sdf_sample(const Sdf *s, float lx, float ly);          /* bilinear, coord local (em) */

#endif
