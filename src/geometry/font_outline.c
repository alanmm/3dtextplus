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
    float line_step = (float)(ascent - descent + linegap);   /* unidades de fonte */
    float tol_units = (flatten_tol > 1e-6f) ? (flatten_tol * unitsPerEm) : 8.0f;

    ConList cl = { 0, 0, 0 };
    float penx = 0.0f, peny = 0.0f;
    float bminx = 1e30f, bminy = 1e30f, bmaxx = -1e30f, bmaxy = -1e30f;
    int have_bounds = 0;

    const unsigned char *s = (const unsigned char *)utf8;
    int prev_cp = 0;
    for (;;) {
        int cp = utf8_next(&s);
        if (cp == 0) break;

        if (cp == '\n') { penx = 0.0f; peny -= line_step; prev_cp = 0; continue; }

        if (prev_cp) penx += (float)stbtt_GetCodepointKernAdvance(&fi, prev_cp, cp);

        stbtt_vertex *verts = NULL;
        int nv = stbtt_GetCodepointShape(&fi, cp, &verts);

        PtBuf cur = { 0, 0, 0 };
        float px = 0.0f, py = 0.0f;
        for (int i = 0; i < nv; ++i) {
            stbtt_vertex *v = &verts[i];
            float vx = penx + (float)v->x, vy = peny + (float)v->y;
            switch (v->type) {
                case STBTT_vmove:
                    finalize_contour(&cl, &cur, sc);
                    pb_push(&cur, vx, vy);
                    break;
                case STBTT_vline:
                    pb_push(&cur, vx, vy);
                    break;
                case STBTT_vcurve:
                    flat_quad(&cur, px, py, penx + (float)v->cx, peny + (float)v->cy,
                              vx, vy, tol_units, 0);
                    break;
                case STBTT_vcubic:
                    flat_cubic(&cur, px, py, penx + (float)v->cx, peny + (float)v->cy,
                               penx + (float)v->cx1, peny + (float)v->cy1, vx, vy, tol_units, 0);
                    break;
                default:
                    break;
            }
            px = vx; py = vy;
            if (vx < bminx) bminx = vx;
            if (vx > bmaxx) bmaxx = vx;
            if (vy < bminy) bminy = vy;
            if (vy > bmaxy) bmaxy = vy;
            have_bounds = 1;
        }
        finalize_contour(&cl, &cur, sc);
        free(cur.p);
        if (verts) stbtt_FreeShape(&fi, verts);

        int aw = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&fi, cp, &aw, &lsb);
        penx += (float)aw;
        prev_cp = cp;
    }
    free(fbytes);

    /* centraliza na bbox */
    float ox = 0.0f, oy = 0.0f;
    if (have_bounds) {
        ox = (bminx + bmaxx) * 0.5f * sc;
        oy = (bminy + bmaxy) * 0.5f * sc;
        for (int i = 0; i < cl.n; ++i)
            for (int k = 0; k < cl.c[i].count; ++k) {
                cl.c[i].pts[k].x -= ox;
                cl.c[i].pts[k].y -= oy;
            }
        out->minx = bminx * sc - ox; out->maxx = bmaxx * sc - ox;
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
