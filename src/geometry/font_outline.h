#ifndef M3DT_FONT_OUTLINE_H
#define M3DT_FONT_OUTLINE_H
#include "geometry/geom_types.h"
#include <wchar.h>

/* Constroi os contornos de `utf8` na fonte dada. Contornos em unidades de em,
   centrados na bbox, Y para cima. Retorna 1 em sucesso (mesmo com string vazia). */
int  font_build_contours(const char *utf8, const wchar_t *family, int bold, int italic,
                         float flatten_tol, ContourSet *out);
void contourset_free(ContourSet *cs);

/* 1 se a familia responde de verdade a negrito/italico neste pipeline de
   extracao de contorno, 0 se e' uma fonte variavel OpenType (tabela
   'fvar') - stb_truetype so' le' a instancia estatica default do 'glyf',
   entao negrito/italico nunca tem efeito visual nela. Usado pra tirar
   essas fontes da lista selecionavel na aba Conteudo. */
int font_supports_bold_italic(const wchar_t *family);

#endif
