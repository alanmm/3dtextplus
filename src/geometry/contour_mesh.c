#include "geometry/contour_mesh.h"
#include "util/log.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "tesselator.h"

typedef struct { MeshVertex *v; int n, cap; } VBuf;
typedef struct { unsigned *i; int n, cap; } IBuf;

static void vpush(VBuf *b, MeshVertex mv)
{
    if (b->n == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 256;
        b->v = (MeshVertex *)realloc(b->v, (size_t)b->cap * sizeof *b->v);
    }
    b->v[b->n++] = mv;
}
static void ipush(IBuf *b, unsigned x)
{
    if (b->n == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 512;
        b->i = (unsigned *)realloc(b->i, (size_t)b->cap * sizeof *b->i);
    }
    b->i[b->n++] = x;
}
static void tri(IBuf *b, unsigned a, unsigned c, unsigned d) { ipush(b, a); ipush(b, c); ipush(b, d); }
static void quad(IBuf *b, unsigned a, unsigned c, unsigned d, unsigned e)
{
    tri(b, a, c, d);
    tri(b, a, d, e);
}

static int sdf_res_for(int quality)
{
    return quality <= 0 ? 256 : (quality == 1 ? 512 : 768);
}

static float signed_area_pts(const v2 *p, int n)
{
    float a = 0.0f;
    for (int i = 0; i < n; ++i) {
        v2 p0 = p[i], p1 = p[(i + 1) % n];
        a += p0.x * p1.y - p1.x * p0.y;
    }
    return 0.5f * a;
}
static float signed_area(const Contour *c) { return signed_area_pts(c->pts, c->count); }

/* offset de `c` para o interior por `d`. `inward_left` = 1 se o interior fica a
   esquerda das arestas (contorno CCW). Fallback ao ponto original quando o miter
   estoura. Escreve `c->count` pontos em `out`. */
static void inset_contour(const Contour *c, float d, int inward_left, v2 *out)
{
    for (int i = 0; i < c->count; ++i) {
        v2 pm = c->pts[(i - 1 + c->count) % c->count];
        v2 p  = c->pts[i];
        v2 pp = c->pts[(i + 1) % c->count];

        v2 e0 = { p.x - pm.x, p.y - pm.y };
        v2 e1 = { pp.x - p.x, pp.y - p.y };
        float l0 = sqrtf(e0.x * e0.x + e0.y * e0.y);
        float l1 = sqrtf(e1.x * e1.x + e1.y * e1.y);
        if (l0 < 1e-9f || l1 < 1e-9f) { out[i] = p; continue; }
        e0.x /= l0; e0.y /= l0;
        e1.x /= l1; e1.y /= l1;

        /* normal interior de cada aresta */
        v2 n0, n1;
        if (inward_left) { n0 = (v2){ -e0.y, e0.x }; n1 = (v2){ -e1.y, e1.x }; }
        else             { n0 = (v2){ e0.y, -e0.x }; n1 = (v2){ e1.y, -e1.x }; }

        v2 bis = { n0.x + n1.x, n0.y + n1.y };
        float bl = sqrtf(bis.x * bis.x + bis.y * bis.y);
        if (bl < 1e-4f) { out[i] = p; continue; }   /* reversao de 180 graus */
        bis.x /= bl; bis.y /= bl;

        float cosang = bis.x * n0.x + bis.y * n0.y;    /* = cos(theta/2) */
        float step = d / (cosang > 0.3f ? cosang : 0.3f);
        out[i] = (v2){ p.x + bis.x * step, p.y + bis.y * step };
    }
}

int contour_mesh_build(const ContourSet *cs, MeshParams p, MeshData *out)
{
    memset(out, 0, sizeof *out);
    if (!cs || cs->count == 0) return 0;

    int valid_pts = 0;
    for (int i = 0; i < cs->count; ++i)
        if (cs->contours[i].count >= 3) valid_pts += cs->contours[i].count;
    if (valid_pts < 3) return 0;

    const float hz = p.depth * 0.5f;
    const int shading = (p.bevel_mode == 0);
    const int geombev = (p.bevel_mode == 1);
    float mb = 0.0f;
    if (shading) {
        mb = p.bevel_size;
        float cap = p.depth * 0.03f;
        if (mb > cap) mb = cap;
        if (mb < 1e-4f) mb = 0.0f;
    }
    const float cap_z = hz - mb;   /* tampa recuada pelo micro-bevel */

    TESStesselator *t = tessNewTess(NULL);
    if (!t) return 0;
    for (int i = 0; i < cs->count; ++i) {
        const Contour *co = &cs->contours[i];
        if (co->count < 3) continue;
        tessAddContour(t, 2, co->pts, (int)sizeof(v2), co->count);
    }
    if (!tessTesselate(t, TESS_WINDING_NONZERO, TESS_POLYGONS, 3, 2, NULL)) {
        tessDeleteTess(t);
        return 0;
    }
    const float *tv = tessGetVertices(t);
    const int   *te = tessGetElements(t);
    int nte = tessGetElementCount(t);

    VBuf vb = { 0, 0, 0 };
    IBuf ib = { 0, 0, 0 };

    /* tampas em cap_z */
    for (int e = 0; e < nte; ++e) {
        int a = te[e * 3 + 0], b = te[e * 3 + 1], c = te[e * 3 + 2];
        if (a == TESS_UNDEF || b == TESS_UNDEF || c == TESS_UNDEF) continue;
        float ax = tv[a * 2], ay = tv[a * 2 + 1];
        float bx = tv[b * 2], by = tv[b * 2 + 1];
        float cx = tv[c * 2], cy = tv[c * 2 + 1];

        unsigned f = (unsigned)vb.n;
        vpush(&vb, (MeshVertex){ ax, ay, cap_z, 0, 0, 1, 0 });
        vpush(&vb, (MeshVertex){ bx, by, cap_z, 0, 0, 1, 0 });
        vpush(&vb, (MeshVertex){ cx, cy, cap_z, 0, 0, 1, 0 });
        tri(&ib, f + 0, f + 1, f + 2);

        unsigned k = (unsigned)vb.n;
        vpush(&vb, (MeshVertex){ ax, ay, -cap_z, 0, 0, -1, 1 });
        vpush(&vb, (MeshVertex){ bx, by, -cap_z, 0, 0, -1, 1 });
        vpush(&vb, (MeshVertex){ cx, cy, -cap_z, 0, 0, -1, 1 });
        tri(&ib, k + 0, k + 2, k + 1);
    }
    tessDeleteTess(t);

    /* ---------- modo geometrico: faixa de bevel multi-segmento (outset robusto) ---------- */
    if (geombev) {
        float bs = p.bevel_size;
        float bd = fminf(p.bevel_depth > 1e-4f ? p.bevel_depth : bs, hz * 0.9f);
        int segs = p.bevel_segments;
        if (segs < 2) segs = 2;
        if (segs > 8) segs = 8;
        float wall_z = hz - bd;
        int clamped = 0;

        for (int ci = 0; ci < cs->count; ++ci) {
            const Contour *co = &cs->contours[ci];
            if (co->count < 3) continue;
            int ccw = signed_area(co) > 0.0f;

            v2 *os = (v2 *)malloc((size_t)co->count * sizeof(v2));
            inset_contour(co, -bs, ccw, os);   /* outset */
            if ((signed_area_pts(os, co->count) > 0.0f) != ccw) {
                for (int i = 0; i < co->count; ++i) os[i] = co->pts[i];
                clamped++;
            }

            for (int i = 0; i < co->count; ++i) {
                int j = (i + 1) % co->count;
                v2 o0 = os[i], o1 = os[j];
                v2 c0 = co->pts[i], c1 = co->pts[j];
                float ex = o1.x - o0.x, ey = o1.y - o0.y;
                float el = sqrtf(ex * ex + ey * ey);
                if (el < 1e-9f) continue;
                float nx = ey / el, ny = -ex / el;

                unsigned w = (unsigned)vb.n;
                vpush(&vb, (MeshVertex){ o0.x, o0.y,  wall_z, nx, ny, 0, 2 });
                vpush(&vb, (MeshVertex){ o1.x, o1.y,  wall_z, nx, ny, 0, 2 });
                vpush(&vb, (MeshVertex){ o1.x, o1.y, -wall_z, nx, ny, 0, 2 });
                vpush(&vb, (MeshVertex){ o0.x, o0.y, -wall_z, nx, ny, 0, 2 });
                quad(&ib, w + 0, w + 1, w + 2, w + 3);

                for (int k = 0; k < segs; ++k) {
                    float a0 = (float)k / (float)segs * 1.5707963f;
                    float a1 = (float)(k + 1) / (float)segs * 1.5707963f;
                    float s0 = sinf(a0), s1 = sinf(a1), cn0 = cosf(a0), cn1 = cosf(a1);
                    float z0 = hz - bd * (1.0f - cn0), z1 = hz - bd * (1.0f - cn1);
                    v2 A0 = { c0.x + (o0.x - c0.x) * s0, c0.y + (o0.y - c0.y) * s0 };
                    v2 B0 = { c1.x + (o1.x - c1.x) * s0, c1.y + (o1.y - c1.y) * s0 };
                    v2 A1 = { c0.x + (o0.x - c0.x) * s1, c0.y + (o0.y - c0.y) * s1 };
                    v2 B1 = { c1.x + (o1.x - c1.x) * s1, c1.y + (o1.y - c1.y) * s1 };
                    unsigned f = (unsigned)vb.n;
                    vpush(&vb, (MeshVertex){ A0.x, A0.y,  z0, nx * s0, ny * s0,  cn0, 3 });
                    vpush(&vb, (MeshVertex){ B0.x, B0.y,  z0, nx * s0, ny * s0,  cn0, 3 });
                    vpush(&vb, (MeshVertex){ B1.x, B1.y,  z1, nx * s1, ny * s1,  cn1, 3 });
                    vpush(&vb, (MeshVertex){ A1.x, A1.y,  z1, nx * s1, ny * s1,  cn1, 3 });
                    quad(&ib, f + 0, f + 1, f + 2, f + 3);
                    unsigned b = (unsigned)vb.n;
                    vpush(&vb, (MeshVertex){ A1.x, A1.y, -z1, nx * s1, ny * s1, -cn1, 3 });
                    vpush(&vb, (MeshVertex){ B1.x, B1.y, -z1, nx * s1, ny * s1, -cn1, 3 });
                    vpush(&vb, (MeshVertex){ B0.x, B0.y, -z0, nx * s0, ny * s0, -cn0, 3 });
                    vpush(&vb, (MeshVertex){ A0.x, A0.y, -z0, nx * s0, ny * s0, -cn0, 3 });
                    quad(&ib, b + 0, b + 1, b + 2, b + 3);
                }
            }
            free(os);
        }
        if (clamped) log_infof("bevel geom: %d contornos clampados", clamped);
    }

    /* paredes + micro-bevel por contorno (modos sombreado e desligado) */
    for (int ci = 0; !geombev && ci < cs->count; ++ci) {
        const Contour *co = &cs->contours[ci];
        if (co->count < 3) continue;

        int ccw = signed_area(co) > 0.0f;
        v2 *inset = NULL;
        if (mb > 0.0f) {
            inset = (v2 *)malloc((size_t)co->count * sizeof(v2));
            inset_contour(co, mb, ccw, inset);
        }

        for (int i = 0; i < co->count; ++i) {
            int j = (i + 1) % co->count;
            v2 a0 = co->pts[i], a1 = co->pts[j];
            float ex = a1.x - a0.x, ey = a1.y - a0.y;
            float el = sqrtf(ex * ex + ey * ey);
            if (el < 1e-9f) continue;
            float nx = ey / el, ny = -ex / el;   /* normal da aresta */

            /* parede: a0/a1 de +cap_z a -cap_z */
            unsigned w = (unsigned)vb.n;
            vpush(&vb, (MeshVertex){ a0.x, a0.y,  cap_z, nx, ny, 0, 2 });
            vpush(&vb, (MeshVertex){ a1.x, a1.y,  cap_z, nx, ny, 0, 2 });
            vpush(&vb, (MeshVertex){ a1.x, a1.y, -cap_z, nx, ny, 0, 2 });
            vpush(&vb, (MeshVertex){ a0.x, a0.y, -cap_z, nx, ny, 0, 2 });
            quad(&ib, w + 0, w + 1, w + 2, w + 3);

            if (mb > 0.0f) {
                v2 b0 = inset[i], b1 = inset[j];
                float s = 0.70710678f;                 /* chanfro ~45 graus */
                float fnx = nx * s, fny = ny * s;       /* componente horizontal */
                /* chanfro frontal: inset @ +hz  ->  original @ +cap_z */
                unsigned cf = (unsigned)vb.n;
                vpush(&vb, (MeshVertex){ b0.x, b0.y,  hz,    fnx, fny,  s, 3 });
                vpush(&vb, (MeshVertex){ b1.x, b1.y,  hz,    fnx, fny,  s, 3 });
                vpush(&vb, (MeshVertex){ a1.x, a1.y,  cap_z, fnx, fny,  s, 3 });
                vpush(&vb, (MeshVertex){ a0.x, a0.y,  cap_z, fnx, fny,  s, 3 });
                quad(&ib, cf + 0, cf + 1, cf + 2, cf + 3);
                /* chanfro traseiro */
                unsigned cb = (unsigned)vb.n;
                vpush(&vb, (MeshVertex){ a0.x, a0.y, -cap_z, fnx, fny, -s, 3 });
                vpush(&vb, (MeshVertex){ a1.x, a1.y, -cap_z, fnx, fny, -s, 3 });
                vpush(&vb, (MeshVertex){ b1.x, b1.y, -hz,    fnx, fny, -s, 3 });
                vpush(&vb, (MeshVertex){ b0.x, b0.y, -hz,    fnx, fny, -s, 3 });
                quad(&ib, cb + 0, cb + 1, cb + 2, cb + 3);
            }
        }
        free(inset);
    }

    if (vb.n == 0 || ib.n == 0) { free(vb.v); free(ib.i); return 0; }

    float mnx = 1e30f, mny = 1e30f, mnz = 1e30f, mxx = -1e30f, mxy = -1e30f, mxz = -1e30f;
    for (int i = 0; i < vb.n; ++i) {
        const MeshVertex *v = &vb.v[i];
        if (v->px < mnx) mnx = v->px;
        if (v->px > mxx) mxx = v->px;
        if (v->py < mny) mny = v->py;
        if (v->py > mxy) mxy = v->py;
        if (v->pz < mnz) mnz = v->pz;
        if (v->pz > mxz) mxz = v->pz;
    }

    out->verts = vb.v; out->nverts = vb.n;
    out->idx = ib.i;   out->nidx = ib.n;
    out->minx = mnx; out->miny = mny; out->minz = mnz;
    out->maxx = mxx; out->maxy = mxy; out->maxz = mxz;

    if (shading) {
        if (sdf_build(cs, sdf_res_for(p.quality), &out->sdf))
            out->has_sdf = 1;
    }
    return 1;
}

void mesh_data_free(MeshData *m)
{
    free(m->verts);
    free(m->idx);
    if (m->has_sdf) sdf_free(&m->sdf);
    memset(m, 0, sizeof *m);
}
