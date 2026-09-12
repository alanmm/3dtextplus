#include "geometry/font_outline.h"
#include "util/log.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "stb_truetype.h"

/* ---- bytes do arquivo da fonte selecionada, via GDI ---- */
static unsigned char *load_face_bytes(const wchar_t *family, int bold, int italic, DWORD *out_size)
{
    HDC dc = CreateCompatibleDC(NULL);
    LOGFONTW lf;
    memset(&lf, 0, sizeof lf);
    lf.lfHeight = -256;
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfItalic = (BYTE)(italic ? 1 : 0);
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    wcsncpy(lf.lfFaceName, (family && family[0]) ? family : L"Segoe UI", LF_FACESIZE - 1);

    HFONT font = CreateFontIndirectW(&lf);
    HGDIOBJ old = SelectObject(dc, font);

    const DWORD TTCF = 0x66637474;   /* 'ttcf' */
    DWORD size = GetFontData(dc, 0, 0, NULL, 0);
    DWORD tag = 0;
    if (size == GDI_ERROR || size == 0) {
        size = GetFontData(dc, TTCF, 0, NULL, 0);
        tag = TTCF;
    }

    unsigned char *buf = NULL;
    if (size != GDI_ERROR && size > 0) {
        buf = (unsigned char *)malloc(size);
        if (buf) {
            DWORD got = GetFontData(dc, tag, 0, buf, size);
            if (got == GDI_ERROR) { free(buf); buf = NULL; }
            else *out_size = size;
        }
    }

    SelectObject(dc, old);
    DeleteObject(font);
    DeleteDC(dc);

    if (!buf) {
        wchar_t path[MAX_PATH];
        UINT n = GetWindowsDirectoryW(path, MAX_PATH);
        if (n && n < MAX_PATH - 20) {
            wcscat(path, L"\\Fonts\\segoeui.ttf");
            HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (h != INVALID_HANDLE_VALUE) {
                DWORD sz = GetFileSize(h, NULL), rd = 0;
                buf = (unsigned char *)malloc(sz);
                if (buf && ReadFile(h, buf, sz, &rd, NULL) && rd == sz) *out_size = sz;
                else { free(buf); buf = NULL; }
                CloseHandle(h);
            }
        }
    }
    return buf;
}

/* ---- buffer de pontos + achatamento de curvas ---- */
typedef struct { v2 *p; int n, cap; } PtBuf;

static void pb_push(PtBuf *b, float x, float y)
{
    if (b->n == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 64;
        b->p = (v2 *)realloc(b->p, (size_t)b->cap * sizeof *b->p);
    }
    b->p[b->n++] = (v2){ x, y };
}

static void flat_quad(PtBuf *b, float x0, float y0, float cx, float cy,
                      float x1, float y1, float tol, int depth)
{
    float mx = (x0 + 2 * cx + x1) * 0.25f, my = (y0 + 2 * cy + y1) * 0.25f;
    float lx = (x0 + x1) * 0.5f, ly = (y0 + y1) * 0.5f;
    if (depth >= 16 || (mx - lx) * (mx - lx) + (my - ly) * (my - ly) <= tol * tol) {
        pb_push(b, x1, y1);
        return;
    }
    float ax = (x0 + cx) * 0.5f, ay = (y0 + cy) * 0.5f;
    float bx = (cx + x1) * 0.5f, by = (cy + y1) * 0.5f;
    float abx = (ax + bx) * 0.5f, aby = (ay + by) * 0.5f;
    flat_quad(b, x0, y0, ax, ay, abx, aby, tol, depth + 1);
    flat_quad(b, abx, aby, bx, by, x1, y1, tol, depth + 1);
}

static void flat_cubic(PtBuf *b, float x0, float y0, float c0x, float c0y,
                       float c1x, float c1y, float x1, float y1, float tol, int depth)
{
    float lx = (x0 + x1) * 0.5f, ly = (y0 + y1) * 0.5f;
    float mx = (x0 + 3 * c0x + 3 * c1x + x1) * 0.125f, my = (y0 + 3 * c0y + 3 * c1y + y1) * 0.125f;
    if (depth >= 16 || (mx - lx) * (mx - lx) + (my - ly) * (my - ly) <= tol * tol) {
        pb_push(b, x1, y1);
        return;
    }
    float ab_x = (x0 + c0x) * .5f, ab_y = (y0 + c0y) * .5f;
    float bc_x = (c0x + c1x) * .5f, bc_y = (c0y + c1y) * .5f;
    float cd_x = (c1x + x1) * .5f, cd_y = (c1y + y1) * .5f;
    float abc_x = (ab_x + bc_x) * .5f, abc_y = (ab_y + bc_y) * .5f;
    float bcd_x = (bc_x + cd_x) * .5f, bcd_y = (bc_y + cd_y) * .5f;
    float m_x = (abc_x + bcd_x) * .5f, m_y = (abc_y + bcd_y) * .5f;
    flat_cubic(b, x0, y0, ab_x, ab_y, abc_x, abc_y, m_x, m_y, tol, depth + 1);
    flat_cubic(b, m_x, m_y, bcd_x, bcd_y, cd_x, cd_y, x1, y1, tol, depth + 1);
}

/* ---- lista de contornos em construcao ---- */
typedef struct { Contour *c; int n, cap; } ConList;

static void finalize_contour(ConList *cl, PtBuf *cur, float sc)
{
    if (cur->n < 3) { cur->n = 0; return; }
    if (cl->n == cl->cap) {
        cl->cap = cl->cap ? cl->cap * 2 : 8;
        cl->c = (Contour *)realloc(cl->c, (size_t)cl->cap * sizeof *cl->c);
    }
    Contour *co = &cl->c[cl->n++];
    co->count = cur->n;
    co->pts = (v2 *)malloc((size_t)cur->n * sizeof(v2));
    for (int k = 0; k < cur->n; ++k) {
        co->pts[k].x = cur->p[k].x * sc;
        co->pts[k].y = cur->p[k].y * sc;
    }
    cur->n = 0;
}

#define KERN_SAMPLES 20

/* Registra x num "perfil de altura" (min/max por faixa vertical), usado
   para kerning otico: a folga visual real entre os contornos ja extraidos
   de um glifo e do proximo, em vez dos metadados de kerning da fonte -
   muitas fontes modernas (ex.: Segoe UI) guardam kerning em GPOS, que esta
   biblioteca (stb_truetype) nao le; so a tabela legada 'kern' e lida, e
   costuma estar vazia ou ausente, entao o kerning por tabela nunca
   ajustava nada na pratica. */
static void profile_update(float *minX, float *maxX, float y, float x, float descent, float span)
{
    if (span < 1e-6f) return;
    int b = (int)((y - descent) / span * (float)(KERN_SAMPLES - 1) + 0.5f);
    if (b < 0) b = 0;
    if (b >= KERN_SAMPLES) b = KERN_SAMPLES - 1;
    if (x < minX[b]) minX[b] = x;
    if (x > maxX[b]) maxX[b] = x;
}

/* Achata os vertices crus de um glifo (stb_truetype) em contornos LOCAIS
   (x cru, sem penx - so peny, para multi-linha), com a tolerancia dada.
   Usado duas vezes por glifo: uma na tolerancia de qualidade do usuario
   (a geometria final) e outra numa tolerancia fixa e fina (so para medir
   o perfil de kerning otico) - ver o comentario acima de profile_update
   sobre por que precisam ser independentes. */
static void flatten_glyph_local(const stbtt_vertex *verts, int nv, float peny,
                                float tol, ConList *outCl)
{
    PtBuf cur = { 0, 0, 0 };
    float px = 0.0f, py = 0.0f;
    for (int i = 0; i < nv; ++i) {
        const stbtt_vertex *v = &verts[i];
        float vx = (float)v->x, vy = peny + (float)v->y;
        switch (v->type) {
            case STBTT_vmove:
                finalize_contour(outCl, &cur, 1.0f);
                pb_push(&cur, vx, vy);
                break;
            case STBTT_vline:
                pb_push(&cur, vx, vy);
                break;
            case STBTT_vcurve:
                flat_quad(&cur, px, py, (float)v->cx, peny + (float)v->cy,
                          vx, vy, tol, 0);
                break;
            case STBTT_vcubic:
                flat_cubic(&cur, px, py, (float)v->cx, peny + (float)v->cy,
                           (float)v->cx1, peny + (float)v->cy1, vx, vy, tol, 0);
                break;
            default:
                break;
        }
        px = vx; py = vy;
    }
    finalize_contour(outCl, &cur, 1.0f);
    free(cur.p);
}

static void conlist_free_pts(ConList *cl)
{
    for (int i = 0; i < cl->n; ++i) free(cl->c[i].pts);
    free(cl->c);
}

/* centraliza horizontalmente os contornos de UMA linha (indices
   [start_ci, cl->n)) em torno do seu proprio centro, em vez de deixar
   cada linha alinhada a esquerda - assim um bloco multilinha com linhas
   de larguras diferentes fica com cada linha centralizada individualmente
   (como um titulo), nao um bloco de texto alinhado a esquerda. */
static void center_line_x(ConList *cl, int start_ci, float minx, float maxx, float sc)
{
    if (maxx <= minx) return;   /* linha sem tinta (linha em branco) */
    float center = (minx + maxx) * 0.5f * sc;
    for (int i = start_ci; i < cl->n; ++i)
        for (int k = 0; k < cl->c[i].count; ++k)
            cl->c[i].pts[k].x -= center;
}

static int utf8_next(const unsigned char **s)
{
    int cp = **s;
    if (cp == 0) return 0;
    if (cp < 0x80) { (*s) += 1; return cp; }
    if (cp < 0xE0) { cp = ((cp & 31) << 6) | ((*s)[1] & 63); (*s) += 2; return cp; }
    if (cp < 0xF0) { cp = ((cp & 15) << 12) | (((*s)[1] & 63) << 6) | ((*s)[2] & 63); (*s) += 3; return cp; }
    cp = ((cp & 7) << 18) | (((*s)[1] & 63) << 12) | (((*s)[2] & 63) << 6) | ((*s)[3] & 63);
    (*s) += 4;
    return cp;
}

int font_build_contours(const char *utf8, const wchar_t *family, int bold, int italic,
                        float flatten_tol, ContourSet *out)
{
    memset(out, 0, sizeof *out);

    DWORD fsize = 0;
    unsigned char *fbytes = load_face_bytes(family, bold, italic, &fsize);
    if (!fbytes) { log_errorf("font: nenhuma fonte carregada"); return 0; }

    stbtt_fontinfo fi;
    int fo = stbtt_GetFontOffsetForIndex(fbytes, 0);
    if (fo < 0 || !stbtt_InitFont(&fi, fbytes, fo)) {
        free(fbytes);
        log_errorf("font: InitFont");
        return 0;
    }

    int ascent, descent, linegap;
    stbtt_GetFontVMetrics(&fi, &ascent, &descent, &linegap);
    float unitsPerEm = (float)(ascent - descent);
    if (unitsPerEm < 1.0f) unitsPerEm = 2048.0f;
    float sc = 1.0f / unitsPerEm;
    /* espacamento entre linhas: 1.0 = metrica padrao da fonte (ascent+
       descent+linegap); >1.0 afasta as linhas verticalmente, ao estilo do
       line-height do CSS. Ajustar aqui se o usuario pedir mais/menos
       espaco entre linhas. */
    const float LINE_HEIGHT_MULT = 1.0f;
    float line_step = (float)(ascent - descent + linegap) * LINE_HEIGHT_MULT;
    float tol_units = (flatten_tol > 1e-6f) ? (flatten_tol * unitsPerEm) : 8.0f;

    ConList cl = { 0, 0, 0 };
    float penx = 0.0f, peny = 0.0f;
    float bminx = 1e30f, bminy = 1e30f, bmaxx = -1e30f, bmaxy = -1e30f;
    int have_bounds = 0;
    int line_start_ci = 0;
    float line_minx = 1e30f, line_maxx = -1e30f;

    const float target_gap = 0.09f * unitsPerEm;   /* folga otica minima entre glifos */
    /* o perfil de kerning precisa de uma tolerancia de achatamento FIXA e
       fina, independente da qualidade de render escolhida pelo usuario:
       com uma tolerancia grosseira (qualidade Baixa/Media), os pontos ja
       achatados passam longe do verdadeiro extremo de uma curva, entao o
       perfil "acha" que ha mais folga do que realmente existe e o texto
       fica apertado demais (ou ate sobreposto) so em qualidades mais
       baixas - a precisao do kerning nao pode depender da qualidade da
       malha final. */
    const float kern_tol = fminf(tol_units, 0.0012f * unitsPerEm);
    float prevMaxX[KERN_SAMPLES];
    float prev_glyph_penx = 0.0f;
    int have_prev_profile = 0;

    const unsigned char *s = (const unsigned char *)utf8;
    for (;;) {
        int cp = utf8_next(&s);
        if (cp == 0) break;

        if (cp == '\r') continue;   /* edits multilinha do Win32 usam \r\n; \r sozinho nao deve virar glifo */

        if (cp == '\n') {
            center_line_x(&cl, line_start_ci, line_minx, line_maxx, sc);
            line_start_ci = cl.n;
            line_minx = 1e30f; line_maxx = -1e30f;
            penx = 0.0f; peny -= line_step; have_prev_profile = 0;
            continue;
        }

        /* extrai o glifo em espaco LOCAL (x cru, sem penx ainda) para poder
           decidir a posicao final antes de fixa-la - uma vez na qualidade
           do usuario (geometria final) e, se houver tinta, outra vez fina
           so para medir o perfil de kerning (ver kern_tol acima) */
        stbtt_vertex *verts = NULL;
        int nv = stbtt_GetCodepointShape(&fi, cp, &verts);
        int glyph_has_ink = (nv > 0);

        ConList localCl = { 0, 0, 0 };
        flatten_glyph_local(verts, nv, peny, tol_units, &localCl);

        float curMinX[KERN_SAMPLES], curMaxX[KERN_SAMPLES];
        for (int i = 0; i < KERN_SAMPLES; ++i) { curMinX[i] = 1e30f; curMaxX[i] = -1e30f; }
        if (glyph_has_ink) {
            ConList profCl = { 0, 0, 0 };
            flatten_glyph_local(verts, nv, peny, kern_tol, &profCl);
            for (int ci = 0; ci < profCl.n; ++ci)
                for (int k = 0; k < profCl.c[ci].count; ++k) {
                    v2 p = profCl.c[ci].pts[k];
                    profile_update(curMinX, curMaxX, p.y, p.x, (float)descent, unitsPerEm);
                }
            conlist_free_pts(&profCl);
        }
        if (verts) stbtt_FreeShape(&fi, verts);

        /* aproxima ate a folga otica minima entre o glifo anterior e este,
           sem nunca deixar o avanco mais largo que o padrao da fonte */
        float final_penx = penx;
        if (have_prev_profile && glyph_has_ink) {
            float need = -1e30f;
            int any = 0;
            for (int b = 0; b < KERN_SAMPLES; ++b) {
                if (prevMaxX[b] <= -1e29f || curMinX[b] >= 1e29f) continue;
                float req = prev_glyph_penx + prevMaxX[b] - curMinX[b] + target_gap;
                if (req > need) need = req;
                any = 1;
            }
            if (any && need < final_penx) final_penx = need;
        }

        /* emite os contornos deste glifo na lista global, deslocados pela
           posicao final e escalados */
        for (int ci = 0; ci < localCl.n; ++ci) {
            Contour *lc = &localCl.c[ci];
            if (cl.n == cl.cap) {
                cl.cap = cl.cap ? cl.cap * 2 : 8;
                cl.c = (Contour *)realloc(cl.c, (size_t)cl.cap * sizeof *cl.c);
            }
            Contour *co = &cl.c[cl.n++];
            co->count = lc->count;
            co->pts = (v2 *)malloc((size_t)lc->count * sizeof(v2));
            for (int k = 0; k < lc->count; ++k) {
                float gx = final_penx + lc->pts[k].x;
                float gy = lc->pts[k].y;
                co->pts[k].x = gx * sc;
                co->pts[k].y = gy * sc;
                if (gx < bminx) bminx = gx;
                if (gx > bmaxx) bmaxx = gx;
                if (gx < line_minx) line_minx = gx;
                if (gx > line_maxx) line_maxx = gx;
                if (gy < bminy) bminy = gy;
                if (gy > bmaxy) bmaxy = gy;
                have_bounds = 1;
            }
            free(lc->pts);
        }
        free(localCl.c);

        int aw = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&fi, cp, &aw, &lsb);
        penx = final_penx + (float)aw;

        if (glyph_has_ink) {
            memcpy(prevMaxX, curMaxX, sizeof curMaxX);
            prev_glyph_penx = final_penx;
            have_prev_profile = 1;
        } else {
            have_prev_profile = 0;   /* espaco: nao ha contra o que otimizar o proximo */
        }
    }
    free(fbytes);
    center_line_x(&cl, line_start_ci, line_minx, line_maxx, sc);   /* ultima linha (sem \n final) */

    /* centraliza na bbox. O eixo X ja foi centralizado POR LINHA acima
       (center_line_x), entao os limites horizontais sao recalculados a
       partir dos pontos finais - bminx/bmaxx acima ainda refletem as
       posicoes brutas alinhadas a esquerda, anteriores a esse ajuste. */
    float ox = 0.0f, oy = 0.0f;
    if (have_bounds) {
        float fminx = 1e30f, fmaxx = -1e30f;
        for (int i = 0; i < cl.n; ++i)
            for (int k = 0; k < cl.c[i].count; ++k) {
                if (cl.c[i].pts[k].x < fminx) fminx = cl.c[i].pts[k].x;
                if (cl.c[i].pts[k].x > fmaxx) fmaxx = cl.c[i].pts[k].x;
            }
        ox = (fminx + fmaxx) * 0.5f;
        oy = (bminy + bmaxy) * 0.5f * sc;
        for (int i = 0; i < cl.n; ++i)
            for (int k = 0; k < cl.c[i].count; ++k) {
                cl.c[i].pts[k].x -= ox;
                cl.c[i].pts[k].y -= oy;
            }
        out->minx = fminx - ox; out->maxx = fmaxx - ox;
        out->miny = bminy * sc - oy; out->maxy = bmaxy * sc - oy;
    }

    out->contours = cl.c;
    out->count = cl.n;
    return 1;
}

void contourset_free(ContourSet *cs)
{
    for (int i = 0; i < cs->count; ++i) free(cs->contours[i].pts);
    free(cs->contours);
    memset(cs, 0, sizeof *cs);
}
