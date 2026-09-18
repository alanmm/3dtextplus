#ifndef M3DT_ROBUST_OFFSET_H
#define M3DT_ROBUST_OFFSET_H
#include "geometry/geom_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Offset robusto de `c` por `d` (mesma convencao/assinatura da antiga
   inset_contour() em contour_mesh.c): d>0 encolhe rumo ao interior, d<0
   cresce pra fora; inward_left = 1 se as normais internas ficam a
   esquerda das arestas (contorno CCW). Escreve c->count pontos em `out`,
   um por vertice original (mesma ordem/indice - o resto da malha depende
   dessa correspondencia 1:1).

   Por vertice, usa a mesma bissetriz/miter da versao antiga, mas limita
   o passo pelo poligono de offset robusto do Clipper2 (offset com
   limpeza de auto-intersecao) em vez de um piso fixo de cosseno - isso
   evita tanto o "estouro" de miter em cantos agudos/junções estreitas
   quanto a convergencia de reentrâncias côncavas no mesmo ponto. */
void robust_offset_contour(const Contour *c, float d, int inward_left, v2 *out);

#ifdef __cplusplus
}
#endif
#endif
