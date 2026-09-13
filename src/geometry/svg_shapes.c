#include "geometry/svg_shapes.h"
#include "util/log.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "nanosvg.h"

static char *read_file_as_buffer(const wchar_t *path, DWORD *out_size)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(h, NULL), rd = 0;
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { CloseHandle(h); return NULL; }
    if (!ReadFile(h, buf, sz, &rd, NULL) || rd != sz) {
        free(buf);
        CloseHandle(h);
        return NULL;
    }
    CloseHandle(h);
    buf[sz] = 0;
    *out_size = sz;
    return buf;
}

/* ---- buffer de pontos + achatamento de cubica (mesma logica de font_outline.c) ---- */
typedef struct { v2 *p; int n, cap; } PtBuf;

static void pb_push(PtBuf *b, float x, float y)
{
    if (b->n == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 64;
        b->p = (v2 *)realloc(b->p, (size_t)b->cap * sizeof *b->p);
    }
    b->p[b->n++] = (v2){ x, y };
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

typedef struct { Contour *c; int n, cap; } ConList;

typedef struct { SvgPiece *p; int n, cap; } PieceBuf;

static SvgPiece *piece_push(PieceBuf *b)
{
    if (b->n == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 8;
        b->p = (SvgPiece *)realloc(b->p, (size_t)b->cap * sizeof *b->p);
    }
    memset(&b->p[b->n], 0, sizeof(SvgPiece));
    return &b->p[b->n++];
}

static void unpack_color(unsigned int c, float *r, float *g, float *b)
{
    /* NSVG_RGB empacota como 0x00BBGGRR (r no byte baixo) */
    *r = (float)(c & 0xFF) / 255.0f;
    *g = (float)((c >> 8) & 0xFF) / 255.0f;
    *b = (float)((c >> 16) & 0xFF) / 255.0f;
}

int svg_shapes_load(const wchar_t *path, float flatten_tol, SvgShapeSet *out)
{
    memset(out, 0, sizeof *out);

    DWORD fsize = 0;
    char *buf = read_file_as_buffer(path, &fsize);
    if (!buf) { log_errorf("svg: nao abriu o arquivo"); return 0; }

    NSVGimage *image = nsvgParse(buf, "px", 96.0f);
    free(buf);
    if (!image) { log_errorf("svg: parse falhou"); return 0; }

    PieceBuf pb = { 0, 0, 0 };
    float uminx = 1e30f, uminy = 1e30f, umaxx = -1e30f, umaxy = -1e30f;
    int have_bounds = 0;

    for (NSVGshape *shape = image->shapes; shape; shape = shape->next) {
        if (!(shape->flags & NSVG_FLAGS_VISIBLE)) continue;
        if (shape->fill.type == NSVG_PAINT_NONE) continue;
        if (shape->opacity <= 0.001f) continue;

        unsigned int color;
        if (shape->fill.type == NSVG_PAINT_COLOR) {
            color = shape->fill.color;
        } else {
            NSVGgradient *grad = shape->fill.gradient;
            unsigned rs = 0, gs = 0, bs = 0;
            int n = grad->nstops > 0 ? grad->nstops : 1;
            for (int i = 0; i < grad->nstops; ++i) {
                rs += grad->stops[i].color & 0xFF;
                gs += (grad->stops[i].color >> 8) & 0xFF;
                bs += (grad->stops[i].color >> 16) & 0xFF;
            }
            color = (rs / (unsigned)n) | ((gs / (unsigned)n) << 8) | ((bs / (unsigned)n) << 16);
        }

        ConList cl = { 0, 0, 0 };
        for (NSVGpath *p = shape->paths; p; p = p->next) {
            if (p->npts < 1) continue;

            float pw = p->bounds[2] - p->bounds[0];
            float ph = p->bounds[3] - p->bounds[1];
            float span = pw > ph ? pw : ph;
            if (span < 1.0f) span = 1.0f;
            float tol = flatten_tol * span;

            PtBuf pts = { 0, 0, 0 };
            pb_push(&pts, p->pts[0], p->pts[1]);
            for (int i = 0; i < p->npts - 1; i += 3) {
                float *pp = &p->pts[i * 2];
                flat_cubic(&pts, pp[0], pp[1], pp[2], pp[3], pp[4], pp[5], pp[6], pp[7], tol, 0);
            }
            if (pts.n < 3) { free(pts.p); continue; }

            if (cl.n == cl.cap) {
                cl.cap = cl.cap ? cl.cap * 2 : 4;
                cl.c = (Contour *)realloc(cl.c, (size_t)cl.cap * sizeof *cl.c);
            }
            cl.c[cl.n].pts = pts.p;
            cl.c[cl.n].count = pts.n;
            cl.n++;
        }
        if (cl.n == 0) continue;

        SvgPiece *piece = piece_push(&pb);
        piece->cs.contours = cl.c;
        piece->cs.count = cl.n;
        piece->cs.fill_rule = (shape->fillRule == NSVG_FILLRULE_EVENODD) ? 1 : 0;
        unpack_color(color, &piece->r, &piece->g, &piece->b);

        for (int i = 0; i < cl.n; ++i)
            for (int k = 0; k < cl.c[i].count; ++k) {
                v2 pt = cl.c[i].pts[k];
                if (pt.x < uminx) uminx = pt.x;
                if (pt.x > umaxx) umaxx = pt.x;
                if (pt.y < uminy) uminy = pt.y;
                if (pt.y > umaxy) umaxy = pt.y;
                have_bounds = 1;
            }
    }

    nsvgDelete(image);

    out->pieces = pb.p;
    out->count = pb.n;
    if (!have_bounds) return 1;   /* sucesso, 0 pecas com tinta (fallback e' do chamador) */

    float cx = (uminx + umaxx) * 0.5f, cy = (uminy + umaxy) * 0.5f;
    float w = umaxx - uminx, h = umaxy - uminy;
    float span = (w > h) ? w : h;
    float scale = (span > 1e-6f) ? (1.0f / span) : 1.0f;

    for (int i = 0; i < pb.n; ++i) {
        SvgPiece *piece = &pb.p[i];
        float pminx = 1e30f, pminy = 1e30f, pmaxx = -1e30f, pmaxy = -1e30f;
        for (int ci = 0; ci < piece->cs.count; ++ci) {
            Contour *co = &piece->cs.contours[ci];
            for (int k = 0; k < co->count; ++k) {
                float nx = (co->pts[k].x - cx) * scale;
                float ny = -(co->pts[k].y - cy) * scale;   /* SVG e y-para-baixo */
                co->pts[k].x = nx;
                co->pts[k].y = ny;
                if (nx < pminx) pminx = nx;
                if (nx > pmaxx) pmaxx = nx;
                if (ny < pminy) pminy = ny;
                if (ny > pmaxy) pmaxy = ny;
            }
        }
        piece->cs.minx = pminx; piece->cs.maxx = pmaxx;
        piece->cs.miny = pminy; piece->cs.maxy = pmaxy;
    }

    return 1;
}

void svg_shapes_free(SvgShapeSet *s)
{
    if (!s) return;
    for (int i = 0; i < s->count; ++i) {
        for (int c = 0; c < s->pieces[i].cs.count; ++c)
            free(s->pieces[i].cs.contours[c].pts);
        free(s->pieces[i].cs.contours);
    }
    free(s->pieces);
    memset(s, 0, sizeof *s);
}
