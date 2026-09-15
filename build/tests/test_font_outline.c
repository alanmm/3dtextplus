#include "test.h"
#include "geometry/font_outline.h"

void run_font_outline_tests(void)
{
    ContourSet cs;

    /* 'A' em Arial: contorno externo + 1 furo => >= 2 contornos */
    EXPECT(font_build_contours("A", L"Arial", 0, 0, 0.01f, &cs) == 1);
    EXPECT(cs.count >= 2);
    EXPECT(cs.maxx > cs.minx && cs.maxy > cs.miny);
    contourset_free(&cs);

    /* string vazia => 0 contornos, sem crash */
    EXPECT(font_build_contours("", L"Arial", 0, 0, 0.01f, &cs) == 1);
    EXPECT(cs.count == 0);
    contourset_free(&cs);

    /* multi-linha: "A\nA" bem mais alto que "A" */
    ContourSet one, two;
    font_build_contours("A", L"Arial", 0, 0, 0.01f, &one);
    font_build_contours("A\nA", L"Arial", 0, 0, 0.01f, &two);
    EXPECT((two.maxy - two.miny) > (one.maxy - one.miny) * 1.5f);
    contourset_free(&one);
    contourset_free(&two);

    /* fonte inexistente => cai para uma fonte do sistema, ainda retorna geometria */
    EXPECT(font_build_contours("A", L"NaoExisteEssaFonte123", 0, 0, 0.01f, &cs) == 1);
    EXPECT(cs.count >= 1);
    contourset_free(&cs);

    /* Arial e' uma fonte estatica normal - negrito/italico funcionam de verdade */
    EXPECT(font_supports_bold_italic(L"Arial") == 1);
    /* fonte inexistente cai num fallback estatico do GDI - tambem nao e' excluida */
    EXPECT(font_supports_bold_italic(L"NaoExisteEssaFonte123") == 1);
}
