#ifndef M3DT_CONTOUR_MESH_H
#define M3DT_CONTOUR_MESH_H
#include "geometry/geom_types.h"
#include "gl_core.h"

typedef struct { float depth; } MeshParams;

typedef struct {
    MeshVertex *verts; int nverts;
    unsigned   *idx;   int nidx;
    float minx, miny, minz, maxx, maxy, maxz;
} MeshData;

/* Tampa frente (z=+depth/2, +Z, surf 0) + verso (z=-depth/2, -Z, surf 1) + paredes
   (normal da aresta para fora, surf 2). Sem bevel. 0 = vazio/falha. */
int  contour_mesh_build(const ContourSet *cs, MeshParams p, MeshData *out);
void mesh_data_free(MeshData *m);

#endif
