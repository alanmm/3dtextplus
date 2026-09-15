#ifndef M3DT_CONTOUR_MESH_H
#define M3DT_CONTOUR_MESH_H
#include "geometry/geom_types.h"
#include "gl_core.h"

typedef struct {
    float depth;
    int   bevel_mode;      /* 0 arredondado, 1 geometrico, 2 desligado */
    float bevel_size;      /* em */
    float bevel_depth;     /* em (so' nos modos com chanfro, 0/1) */
    int   bevel_segments;  /* 2..8 (so' nos modos com chanfro, 0/1) */
    int   shell;           /* 0/1 */
    float wall_thickness;  /* em (so com shell) */
    int   quality;         /* 0 baixa, 1 media, 2 alta -> tolerancia de achatamento do contorno */
} MeshParams;

typedef struct {
    MeshVertex *verts; int nverts;
    unsigned   *idx;   int nidx;
    float minx, miny, minz, maxx, maxy, maxz;
} MeshData;

/* surfaceId: 0 tampa frente, 1 tampa verso, 2 parede, 3 chanfro/micro-bevel.
   Retorno 0 = malha vazia / falha. */
int  contour_mesh_build(const ContourSet *cs, MeshParams p, MeshData *out);
void mesh_data_free(MeshData *m);

#endif
