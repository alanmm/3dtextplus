#include "geometry/sdf.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct { float dx, dy; } Vec;

static float d2(Vec v) { return v.dx * v.dx + v.dy * v.dy; }

static void compare(Vec *g, int res, int x, int y, int ox, int oy)
{
    int nx = x + ox, ny = y + oy;
    if (nx < 0 || nx >= res || ny < 0 || ny >= res) return;
    Vec other = g[ny * res + nx];
    other.dx += (float)ox;
    other.dy += (float)oy;
    if (d2(other) < d2(g[y * res + x])) g[y * res + x] = other;
}

static void edt_8ssedt(Vec *g, int res)
{
    for (int y = 0; y < res; ++y) {
        for (int x = 0; x < res; ++x) {
            compare(g, res, x, y, -1, 0);
            compare(g, res, x, y, 0, -1);
            compare(g, res, x, y, -1, -1);
            compare(g, res, x, y, 1, -1);
        }
        for (int x = res - 1; x >= 0; --x)
            compare(g, res, x, y, 1, 0);
    }
    for (int y = res - 1; y >= 0; --y) {
        for (int x = res - 1; x >= 0; --x) {
            compare(g, res, x, y, 1, 0);
            compare(g, res, x, y, 0, 1);
            compare(g, res, x, y, -1, 1);
            compare(g, res, x, y, 1, 1);
        }
        for (int x = 0; x < res; ++x)
            compare(g, res, x, y, -1, 0);
    }
}

int sdf_build(const ContourSet *cs, int res, Sdf *out)
{
    memset(out, 0, sizeof *out);
    if (!cs || cs->count == 0 || res < 8) return 0;

    int valid = 0;
    for (int i = 0; i < cs->count; ++i)
        if (cs->contours[i].count >= 3) valid++;
    if (valid == 0) return 0;

    /* bbox da uniao */
    float bx0 = 1e30f, by0 = 1e30f, bx1 = -1e30f, by1 = -1e30f;
    for (int i = 0; i < cs->count; ++i) {
        const Contour *c = &cs->contours[i];
        if (c->count < 3) continue;
        for (int k = 0; k < c->count; ++k) {
            if (c->pts[k].x < bx0) bx0 = c->pts[k].x;
            if (c->pts[k].x > bx1) bx1 = c->pts[k].x;
            if (c->pts[k].y < by0) by0 = c->pts[k].y;
            if (c->pts[k].y > by1) by1 = c->pts[k].y;
        }
    }
    float bw = bx1 - bx0, bh = by1 - by0;
    if (bw <= 1e-6f || bh <= 1e-6f) return 0;

    float span = (bw > bh ? bw : bh);
    float margin = 2.0f * span / (float)res + 0.25f * span;
    /* grade quadrada cobrindo a bbox + margem, mesma escala nos dois eixos */
    float grid = span + 2.0f * margin;
    float texel = grid / (float)res;
    float cx = 0.5f * (bx0 + bx1), cy = 0.5f * (by0 + by1);
    out->min_x = cx - 0.5f * grid;
    out->min_y = cy - 0.5f * grid;
    out->size_x = grid;
    out->size_y = grid;
    out->res = res;

    int n = res * res;
    unsigned char *inside = (unsigned char *)calloc((size_t)n, 1);
    out->dist = (float *)malloc((size_t)n * sizeof(float));
    out->gx = (float *)malloc((size_t)n * sizeof(float));
    out->gy = (float *)malloc((size_t)n * sizeof(float));
    if (!inside || !out->dist || !out->gx || !out->gy) {
        free(inside); sdf_free(out); return 0;
    }

    /* dentro/fora por scanline (regra nonzero) */
    typedef struct { float x; int dir; } Cross;
    Cross *xs = (Cross *)malloc(sizeof(Cross) * (size_t)(valid * 64 + 64));
    for (int j = 0; j < res; ++j) {
        float y = out->min_y + ((float)j + 0.5f) * texel;
        int m = 0;
        for (int ci = 0; ci < cs->count; ++ci) {
            const Contour *c = &cs->contours[ci];
            if (c->count < 3) continue;
            for (int k = 0; k < c->count; ++k) {
                v2 p0 = c->pts[k];
                v2 p1 = c->pts[(k + 1) % c->count];
                if ((p0.y <= y && p1.y > y) || (p1.y <= y && p0.y > y)) {
                    float t = (y - p0.y) / (p1.y - p0.y);
                    xs[m].x = p0.x + t * (p1.x - p0.x);
                    xs[m].dir = (p1.y > p0.y) ? 1 : -1;
                    ++m;
                }
            }
        }
        for (int a = 1; a < m; ++a) {   /* insertion sort por x */
            Cross key = xs[a];
            int b = a - 1;
            while (b >= 0 && xs[b].x > key.x) { xs[b + 1] = xs[b]; --b; }
            xs[b + 1] = key;
        }
        int wind = 0, cur = 0;
        for (int i = 0; i < res; ++i) {
            float x = out->min_x + ((float)i + 0.5f) * texel;
            while (cur < m && xs[cur].x <= x) { wind += xs[cur].dir; ++cur; }
            inside[j * res + i] = (wind != 0) ? 1 : 0;
        }
    }
    free(xs);

    /* 8SSEDT a partir dos texels de fronteira */
    Vec *g = (Vec *)malloc((size_t)n * sizeof(Vec));
    if (!g) { free(inside); sdf_free(out); return 0; }
    for (int j = 0; j < res; ++j)
        for (int i = 0; i < res; ++i) {
            int p = j * res + i;
            int boundary = 0;
            if (i > 0        && inside[p - 1]   != inside[p]) boundary = 1;
            if (i < res - 1  && inside[p + 1]   != inside[p]) boundary = 1;
            if (j > 0        && inside[p - res] != inside[p]) boundary = 1;
            if (j < res - 1  && inside[p + res] != inside[p]) boundary = 1;
            if (boundary) { g[p].dx = 0.0f; g[p].dy = 0.0f; }
            else          { g[p].dx = 1e9f;  g[p].dy = 1e9f; }
        }
    edt_8ssedt(g, res);

    for (int p = 0; p < n; ++p) {
        float d = sqrtf(d2(g[p])) * texel;
        out->dist[p] = inside[p] ? -d : d;
    }
    free(g);
    free(inside);

    /* gradiente (diferencas centrais) */
    for (int j = 0; j < res; ++j)
        for (int i = 0; i < res; ++i) {
            int p = j * res + i;
            int il = i > 0 ? i - 1 : i, ir = i < res - 1 ? i + 1 : i;
            int jd = j > 0 ? j - 1 : j, ju = j < res - 1 ? j + 1 : j;
            float dx = (out->dist[j * res + ir] - out->dist[j * res + il]) / ((float)(ir - il) * texel);
            float dy = (out->dist[ju * res + i] - out->dist[jd * res + i]) / ((float)(ju - jd) * texel);
            float l = sqrtf(dx * dx + dy * dy);
            if (l > 1e-6f) { out->gx[p] = dx / l; out->gy[p] = dy / l; }
            else           { out->gx[p] = 0.0f;   out->gy[p] = 0.0f; }
        }
    return 1;
}

void sdf_free(Sdf *out)
{
    free(out->dist);
    free(out->gx);
    free(out->gy);
    memset(out, 0, sizeof *out);
}

float sdf_sample(const Sdf *s, float lx, float ly)
{
    if (!s->dist || s->res <= 0) return 0.0f;
    float texel = s->size_x / (float)s->res;
    float fx = (lx - s->min_x) / texel - 0.5f;
    float fy = (ly - s->min_y) / texel - 0.5f;
    int i0 = (int)floorf(fx), j0 = (int)floorf(fy);
    float tx = fx - (float)i0, ty = fy - (float)j0;
    int i1 = i0 + 1, j1 = j0 + 1;
    int hi = s->res - 1;
    i0 = i0 < 0 ? 0 : (i0 > hi ? hi : i0);
    i1 = i1 < 0 ? 0 : (i1 > hi ? hi : i1);
    j0 = j0 < 0 ? 0 : (j0 > hi ? hi : j0);
    j1 = j1 < 0 ? 0 : (j1 > hi ? hi : j1);
    float a = s->dist[j0 * s->res + i0], b = s->dist[j0 * s->res + i1];
    float c = s->dist[j1 * s->res + i0], d = s->dist[j1 * s->res + i1];
    return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty;
}
