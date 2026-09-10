#ifndef M3DT_FONT_OUTLINE_H
#define M3DT_FONT_OUTLINE_H
#include "geometry/geom_types.h"
#include <wchar.h>

/* Constroi os contornos de `utf8` na fonte dada. Contornos em unidades de em,
   centrados na bbox, Y para cima. Retorna 1 em sucesso (mesmo com string vazia). */
int  font_build_contours(const char *utf8, const wchar_t *family, int bold, int italic,
                         float flatten_tol, ContourSet *out);
void contourset_free(ContourSet *cs);

#endif
