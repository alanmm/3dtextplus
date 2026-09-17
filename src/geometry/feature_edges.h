#ifndef M3DT_FEATURE_EDGES_H
#define M3DT_FEATURE_EDGES_H
#include "geometry/contour_mesh.h"
#include "util/mathx.h"

typedef struct { v3 a, b; } WireEdge;

/* Extrai as arestas "reais" de md: bordas abertas da malha (1
   triangulo vizinho) e vincos (2 triangulos vizinhos cujas normais
   geometricas planas diferem mais que crease_deg). Aloca *out via
   malloc - quem chama libera com free(). Retorna 0 se md nao tiver
   triangulos, ou se nenhuma aresta passar no filtro (nesse caso *out
   e *out_count ficam intocados). */
int feature_edges_build(const MeshData *md, float crease_deg,
                         WireEdge **out, int *out_count);

#endif
