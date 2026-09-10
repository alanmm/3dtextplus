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

int contour_mesh_build(const ContourSet *cs, MeshParams p, MeshData *out)
{
    memset(out, 0, sizeof *out);
    if (!cs || cs->count == 0) return 0;

    int valid_pts = 0;
    for (int i = 0; i < cs->count; ++i)
        if (cs->contours[i].count >= 3) valid_pts += cs->contours[i].count;
    if (valid_pts < 3) return 0;

    const float hz = p.depth * 0.5f;

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

    /* tampas */
    for (int e = 0; e < nte; ++e) {
        int a = te[e * 3 + 0], b = te[e * 3 + 1], c = te[e * 3 + 2];
        if (a == TESS_UNDEF || b == TESS_UNDEF || c == TESS_UNDEF) continue;
        float ax = tv[a * 2], ay = tv[a * 2 + 1];
        float bx = tv[b * 2], by = tv[b * 2 + 1];
        float cx = tv[c * 2], cy = tv[c * 2 + 1];

        unsigned f = (unsigned)vb.n;
        vpush(&vb, (MeshVertex){ ax, ay, hz, 0, 0, 1, 0 });
        vpush(&vb, (MeshVertex){ bx, by, hz, 0, 0, 1, 0 });
        vpush(&vb, (MeshVertex){ cx, cy, hz, 0, 0, 1, 0 });
        ipush(&ib, f + 0); ipush(&ib, f + 1); ipush(&ib, f + 2);

        unsigned k = (unsigned)vb.n;
        vpush(&vb, (MeshVertex){ ax, ay, -hz, 0, 0, -1, 1 });
        vpush(&vb, (MeshVertex){ bx, by, -hz, 0, 0, -1, 1 });
        vpush(&vb, (MeshVertex){ cx, cy, -hz, 0, 0, -1, 1 });
        ipush(&ib, k + 0); ipush(&ib, k + 2); ipush(&ib, k + 1);   /* winding invertido */
    }

    /* paredes: um quad por aresta de cada contorno de entrada */
    for (int ci = 0; ci < cs->count; ++ci) {
        const Contour *co = &cs->contours[ci];
        if (co->count < 3) continue;
        for (int i = 0; i < co->count; ++i) {
            v2 p0 = co->pts[i];
            v2 p1 = co->pts[(i + 1) % co->count];
            float ex = p1.x - p0.x, ey = p1.y - p0.y;
            float el = sqrtf(ex * ex + ey * ey);
            if (el < 1e-9f) continue;
            float nx = ey / el, ny = -ex / el;   /* a direita da aresta = fora, p/ contorno CCW */

            unsigned base = (unsigned)vb.n;
            vpush(&vb, (MeshVertex){ p0.x, p0.y,  hz, nx, ny, 0, 2 });
            vpush(&vb, (MeshVertex){ p1.x, p1.y,  hz, nx, ny, 0, 2 });
            vpush(&vb, (MeshVertex){ p1.x, p1.y, -hz, nx, ny, 0, 2 });
            vpush(&vb, (MeshVertex){ p0.x, p0.y, -hz, nx, ny, 0, 2 });
            ipush(&ib, base + 0); ipush(&ib, base + 1); ipush(&ib, base + 2);
            ipush(&ib, base + 0); ipush(&ib, base + 2); ipush(&ib, base + 3);
        }
    }

    tessDeleteTess(t);

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
    return 1;
}

void mesh_data_free(MeshData *m)
{
    free(m->verts);
    free(m->idx);
    memset(m, 0, sizeof *m);
}
