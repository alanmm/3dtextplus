#ifndef M3DT_SVG_SHAPES_H
#define M3DT_SVG_SHAPES_H
#include "geometry/geom_types.h"
#include <wchar.h>

typedef struct {
    ContourSet cs;   /* contornos desta peca, normalizados e centralizados */
    float r, g, b;   /* cor de preenchimento desta peca (0..1) */
} SvgPiece;

typedef struct {
    SvgPiece *pieces;
    int       count;
} SvgShapeSet;

/* Carrega um arquivo .svg e monta uma peca por shape visivel com
   preenchimento (shapes so-com-stroke ou totalmente transparentes sao
   ignorados nesta fase). Cada peca fica em coordenadas normalizadas: a
   uniao de todas as pecas cabe no intervalo [-0.5,0.5] no maior eixo,
   centralizada, com Y invertido (SVG e y-para-baixo). flatten_tol e'
   uma fracao do tamanho de cada shape (mesma convencao do "tol" usado
   em font_build_contours - ex.: 0.010/0.004/0.0018 por qualidade).
   Retorna 1 em sucesso (mesmo com 0 pecas resultantes, ex.: SVG so com
   stroke); 0 se o arquivo nao existe ou nao pode ser lido/parseado. */
int  svg_shapes_load(const wchar_t *path, float flatten_tol, SvgShapeSet *out);
void svg_shapes_free(SvgShapeSet *s);

#endif
