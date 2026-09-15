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
    EXPECT(font_is_usable(L"Arial") == 1);
    /* fonte inexistente cai num fallback estatico do GDI - tambem nao e' excluida */
    EXPECT(font_is_usable(L"NaoExisteEssaFonte123") == 1);

    /* regressao: "Iosevka Term" (instalada nesta maquina como parte de um
       TrueType Collection) fazia GetFontData devolver um buffer cujo
       proprio diretorio de tabelas sfnt aponta pra alem do fim dos bytes
       extraidos - a stb_truetype lia esses ponteiros invalidos e
       derrubava o processo inteiro (SIGSEGV em ttUSHORT, via
       stbtt_FindGlyphIndex). Se essa fonte nao estiver instalada na
       maquina que roda o teste, cai no fallback normal de "fonte
       inexistente" - a asserção continua valendo de qualquer jeito. Alem
       de nao crashar mais, o usuario notou que a fonte so' renderiza via
       fallback (Segoe UI) mesmo depois do fix - font_is_usable agora
       exclui ela da lista, em vez de deixar selecionavel mas enganosa. */
    ContourSet iosevka;
    EXPECT(font_build_contours("A", L"Iosevka Term", 1, 0, 0.01f, &iosevka) == 1);
    EXPECT(iosevka.count >= 1);
    contourset_free(&iosevka);
    EXPECT(font_is_usable(L"Iosevka Term") == 0);
    EXPECT(font_is_usable(L"Iosevka") == 0);

    /* regressao: "MS UI Gothic" (fonte CJK grande, ~9MB) reportada pelo
       usuario derrubando o processo - padrao de corrupcao DIFERENTE do
       da Iosevka: o diretorio de tabelas sfnt bate certinho (todas as
       tabelas cabem no buffer), mas o CONTEUDO da propria tabela 'cmap'
       vem desalinhado - versao=130 (so' 0 e' valido) e numTables=42582
       (medido nesta maquina; a faixa real e' pequena, poucas dezenas).
       stbtt_InitFont le' esse numTables sem checar limite nenhum,
       percorrendo memoria bem alem do buffer (SIGSEGV em ttUSHORT, via
       stbtt_InitFont_internal). sfnt_tables_in_bounds agora tambem
       valida o cabecalho interno da 'cmap'. Reproduzido e confirmado
       nesta maquina antes do fix (via gdb) e depois (sem crash). O
       usuario tambem reportou "MingLiU"/"PMingLiU" derrubando o
       processo, mas essas duas NAO reproduziram aqui nem antes nem
       depois do fix - a copia delas nesta maquina aparentemente nao
       tem a mesma corrupcao (varia por maquina/versao do Windows).
       Como font_is_usable roda a validacao ao vivo em cima do que o
       GDI realmente devolve em CADA maquina, se a copia do usuario
       estiver corrompida do mesmo jeito o mesmo check deve pegar la',
       mesmo sem eu conseguir confirmar isso localmente - por isso so'
       "MS UI Gothic" (que reproduziu aqui) vira asserção de
       font_is_usable==0; as outras duas so' teriam o teste de
       "nao crasha", que ja' e' garantido por construção quando nao ha'
       corrupcao real pra detectar. */
    ContourSet mingliu;
    EXPECT(font_build_contours("A", L"MS UI Gothic", 1, 0, 0.01f, &mingliu) == 1);
    EXPECT(mingliu.count >= 1);
    contourset_free(&mingliu);
    EXPECT(font_is_usable(L"MS UI Gothic") == 0);

    /* regressao: o perfil de kerning otico so' registrava pontos nos
       vertices do contorno achatado - uma aresta reta e alta (o caule
       de "t", a barra vertical de "+") so' tem 2 vertices (topo/base),
       entao varias faixas de altura no meio nunca recebiam nenhum
       registro de tinta, como se o glifo vizinho tivesse folga ali
       quando na verdade tem tinta solida. Em "Circular Std Bold" isso
       fazia "t+" ficar com sobreposicao geometrica de verdade (medido
       antes do fix: ~0.15em de overlap). Se a fonte nao estiver
       instalada na maquina do teste, cai no fallback estatico - os 2
       contornos ainda saem sem overlap (mesmo glifo repetido nao se
       sobrepoe a si mesmo). */
    ContourSet tp;
    EXPECT(font_build_contours("t+", L"Circular Std Bold", 0, 0, 0.01f, &tp) == 1);
    EXPECT(tp.count == 2);
    float t_maxx = -1e30f, plus_minx = 1e30f;
    for (int k = 0; k < tp.contours[0].count; ++k)
        if (tp.contours[0].pts[k].x > t_maxx) t_maxx = tp.contours[0].pts[k].x;
    for (int k = 0; k < tp.contours[1].count; ++k)
        if (tp.contours[1].pts[k].x < plus_minx) plus_minx = tp.contours[1].pts[k].x;
    EXPECT(plus_minx > t_maxx);
    contourset_free(&tp);
}
