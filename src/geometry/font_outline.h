#ifndef M3DT_FONT_OUTLINE_H
#define M3DT_FONT_OUTLINE_H
#include "geometry/geom_types.h"
#include <wchar.h>

/* Constroi os contornos de `utf8` na fonte dada. Contornos em unidades de em,
   centrados na bbox, Y para cima. Retorna 1 em sucesso (mesmo com string vazia). */
int  font_build_contours(const char *utf8, const wchar_t *family, int bold, int italic,
                         float flatten_tol, ContourSet *out);
void contourset_free(ContourSet *cs);

/* 0 se a familia nao deve aparecer na lista selecionavel da aba
   Conteudo: e' uma fonte variavel OpenType (tabela 'fvar' - stb_truetype
   so' le' a instancia estatica default do 'glyf', entao negrito/italico
   nunca tem efeito visual nela), ou os dados que o GDI devolve pra ela
   sao inconsistentes o bastante (fonte reconstruida de um .ttc grande,
   por exemplo) pra estourar os limites da stb_truetype ao tentar
   renderizar - nesse caso hoje cairia num fallback silencioso (ou, sem
   a validacao em load_face_bytes, derrubaria o processo). 1 caso
   contrario. */
int font_is_usable(const wchar_t *family);

#endif
