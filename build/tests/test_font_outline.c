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

    /* regressao: "Iosevka Term" (instalada nesta maquina como parte de um
       TrueType Collection) fazia GetFontData devolver um buffer cujo
       proprio diretorio de tabelas sfnt aponta pra alem do fim dos bytes
       extraidos - a stb_truetype lia esses ponteiros invalidos e
       derrubava o processo inteiro (SIGSEGV em ttUSHORT, via
       stbtt_FindGlyphIndex). Se essa fonte nao estiver instalada na
       maquina que roda o teste, cai no fallback normal de "fonte
       inexistente" - a asserção continua valendo de qualquer jeito. */
    ContourSet iosevka;
    EXPECT(font_build_contours("A", L"Iosevka Term", 1, 0, 0.01f, &iosevka) == 1);
    EXPECT(iosevka.count >= 1);
    contourset_free(&iosevka);
}
