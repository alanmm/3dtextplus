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

/* normal da aresta a->b apontando para FORA da forma (ccw = contorno anti-horario) */
static v2 edge_outN(v2 a, v2 b, int ccw)
{
    float ex = b.x - a.x, ey = b.y - a.y;
    float l = sqrtf(ex * ex + ey * ey);
    if (l < 1e-9f) return (v2){ 0.0f, 0.0f };
    ex /= l; ey /= l;
    return ccw ? (v2){ ey, -ex } : (v2){ -ey, ex };
}

/* Normais de parede suavizadas por vertice, com limiar de angulo (igual ao
   "shade smooth com angulo" do Blender): perto de curvas continuas (letras
   redondas como O/D/3), as normais das duas arestas que se encontram num
   vertice sao quase iguais -> media-se, e a parede deixa de aparecer
   facetada. Perto de uma quina de verdade (serifa, juncao de traços retos),
   o angulo e grande -> cada aresta mantem sua propria normal, preservando
   a aresta viva. */
#define WALL_SMOOTH_COS 0.7071f  /* cos(45 graus): abaixo disso e quina viva */

typedef struct { v2 *edgeN, *smoothN; int *smooth; int n; } WallNormals;

static void wall_normals_build(const v2 *pts, int n, int ccw, WallNormals *wn)
{
    wn->n = n;
    wn->edgeN   = (v2 *)malloc((size_t)n * sizeof(v2));
    wn->smoothN = (v2 *)malloc((size_t)n * sizeof(v2));
    wn->smooth  = (int *)malloc((size_t)n * sizeof(int));
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        wn->edgeN[i] = edge_outN(pts[i], pts[j], ccw);
    }
    for (int i = 0; i < n; ++i) {
        int prev = (i - 1 + n) % n;
        v2 a = wn->edgeN[prev], b = wn->edgeN[i];
        int valid = (a.x != 0.0f || a.y != 0.0f) && (b.x != 0.0f || b.y != 0.0f);
        float cosang = a.x * b.x + a.y * b.y;
        wn->smooth[i] = valid && cosang >= WALL_SMOOTH_COS;
        if (wn->smooth[i]) {
            v2 s = { a.x + b.x, a.y + b.y };
            float l = sqrtf(s.x * s.x + s.y * s.y);
            wn->smoothN[i] = (l > 1e-6f) ? (v2){ s.x / l, s.y / l } : b;
        } else {
            wn->smoothN[i] = b;   /* nao usado (smooth[i]==0), so evita lixo */
        }
    }
}

static void wall_normals_free(WallNormals *wn)
{
    free(wn->edgeN); free(wn->smoothN); free(wn->smooth);
}

/* normal a usar no vertice `vertIdx` quando ele e um dos dois extremos da
   aresta `edgeIdx` (a normal "propria" dessa aresta, para o caso de quina). */
static v2 wall_normal_at(const WallNormals *wn, int vertIdx, int edgeIdx)
{
    return wn->smooth[vertIdx] ? wn->smoothN[vertIdx] : wn->edgeN[edgeIdx];
}

/* chanfro arredondado: posicao ao longo de um arco de quarto de elipse
   (semi-eixos run=distancia XY ate o outset, bd=profundidade) entre o
   canto original (theta=0) e o outset (theta=PI/2), parametrizado por
   theta em vez de uma fracao linear - e' isso que da' curvatura de
   verdade ao perfil (compare com o modo Geometrico, que interpola reto
   entre os mesmos dois pontos). */
static v2 rounded_pos(v2 c, v2 o, float theta)
{
    float f = sinf(theta);
    return (v2){ c.x + (o.x - c.x) * f, c.y + (o.y - c.y) * f };
}

/* normal no ponto `theta` do mesmo arco: tangente = (run*cos theta,
   -bd*sin theta) num corte (XY radial, Z); normal = tangente girada
   90 graus, escolhida pra bater com a normal da tampa (0,0,1) em
   theta=0 e com a normal radial da parede em theta=PI/2 - a mesma
   suavizacao "por angulo" de `n` (ja' vem de wall_normal_at) cuida da
   direcao ao redor do contorno, isso aqui cuida da direcao ao longo
   do proprio chanfro. */
static void rounded_normal(v2 n, float theta, float run, float bd, float *nx, float *ny, float *nz)
{
    float a = bd * sinf(theta);
    float b = run * cosf(theta);
    float len = sqrtf(a * a + b * b);
    float xyk = (len > 1e-6f) ? (a / len) : 0.0f;
    float zk  = (len > 1e-6f) ? (b / len) : 1.0f;
    *nx = n.x * xyk; *ny = n.y * xyk; *nz = zk;
}

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
    const int rounded = (p.bevel_mode == 0);   /* chanfro curvo (arco) */
    const int flatbev = (p.bevel_mode == 1);   /* chanfro reto (corte fixo) */
    const int geombev = rounded || flatbev;
    const float cap_z = hz;

    /* casca oca: inset de cada contorno; valido se preserva a orientacao e mantem area */
    v2 **inner = NULL;
    int  *shell_ok = NULL;
    int   any_shell = 0;
    if (p.shell) {
        inner = (v2 **)calloc((size_t)cs->count, sizeof(v2 *));
        shell_ok = (int *)calloc((size_t)cs->count, sizeof(int));
        for (int i = 0; i < cs->count; ++i) {
            const Contour *co = &cs->contours[i];
            if (co->count < 3) continue;
            int ccw = signed_area(co) > 0.0f;
            float a0 = fabsf(signed_area(co));
            v2 *ins = (v2 *)malloc((size_t)co->count * sizeof(v2));
            inset_contour(co, p.wall_thickness, ccw, ins);
            float ai = signed_area_pts(ins, co->count);
            if ((ai > 0.0f) == ccw && fabsf(ai) > 0.15f * a0) {
                inner[i] = ins;
                shell_ok[i] = 1;
                any_shell = 1;
            } else {
                free(ins);
            }
        }
    }

    TESStesselator *t = tessNewTess(NULL);
    if (!t) { free(inner); free(shell_ok); return 0; }
    for (int i = 0; i < cs->count; ++i) {
        const Contour *co = &cs->contours[i];
        if (co->count < 3) continue;
        tessAddContour(t, 2, co->pts, (int)sizeof(v2), co->count);
        if (inner && shell_ok[i]) {
            /* buraco = inset com a ordem dos pontos invertida */
            v2 *rev = (v2 *)malloc((size_t)co->count * sizeof(v2));
            for (int k = 0; k < co->count; ++k) rev[k] = inner[i][co->count - 1 - k];
            tessAddContour(t, 2, rev, (int)sizeof(v2), co->count);
            free(rev);
        }
    }
    int winding = (cs->fill_rule == 1) ? TESS_WINDING_ODD : TESS_WINDING_NONZERO;
    if (!tessTesselate(t, winding, TESS_POLYGONS, 3, 2, NULL)) {
        tessDeleteTess(t);
        for (int i = 0; i < cs->count; ++i) free(inner ? inner[i] : NULL);
        free(inner); free(shell_ok);
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

    /* ---------- chanfro real (reto ou arredondado): faixa de bevel multi-segmento (outset robusto) ---------- */
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
            WallNormals wn;
            wall_normals_build(os, co->count, ccw, &wn);

            for (int i = 0; i < co->count; ++i) {
                int j = (i + 1) % co->count;
                v2 o0 = os[i], o1 = os[j];
                v2 c0 = co->pts[i], c1 = co->pts[j];
                v2 on = wn.edgeN[i];
                if (on.x == 0.0f && on.y == 0.0f) continue;
                v2 n0 = wall_normal_at(&wn, i, i);
                v2 n1 = wall_normal_at(&wn, j, i);

                unsigned w = (unsigned)vb.n;
                vpush(&vb, (MeshVertex){ o0.x, o0.y,  wall_z, n0.x, n0.y, 0, 2 });
                vpush(&vb, (MeshVertex){ o1.x, o1.y,  wall_z, n1.x, n1.y, 0, 2 });
                vpush(&vb, (MeshVertex){ o1.x, o1.y, -wall_z, n1.x, n1.y, 0, 2 });
                vpush(&vb, (MeshVertex){ o0.x, o0.y, -wall_z, n0.x, n0.y, 0, 2 });
                quad(&ib, w + 0, w + 1, w + 2, w + 3);

                /* faceta do chanfro: de (c @ hz) a (outset @ hz-bd), reta (flatbev)
                   ou em arco (rounded, via rounded_pos/rounded_normal). Em ambos os
                   casos a normal e' por vertice (lado A usa n0, lado B usa n1 - a
                   mesma direcao XY ja suavizada/com quina viva calculada pra parede
                   acima) em vez de uma normal constante pro retalho inteiro: isso
                   elimina a quebra de sombreamento faceta-a-faceta em trechos
                   curvos do contorno (letras redondas), onde antes cada aresta do
                   contorno virava uma faceta visivelmente distinta sob luz
                   especular/material metalico. */
                float run = sqrtf((o0.x - c0.x) * (o0.x - c0.x) + (o0.y - c0.y) * (o0.y - c0.y));
                float fl = sqrtf(run * run + bd * bd);
                float xyk = (fl > 1e-6f) ? (bd / fl) : 0.0f;
                float zk  = (fl > 1e-6f) ? (run / fl) : 1.0f;
                float fnxA = n0.x * xyk, fnyA = n0.y * xyk, fnzA = zk;
                float fnxB = n1.x * xyk, fnyB = n1.y * xyk, fnzB = zk;
                const float HALF_PI = 1.57079633f;

                for (int k = 0; k < segs; ++k) {
                    float u0 = (float)k / (float)segs, u1 = (float)(k + 1) / (float)segs;
                    float z0, z1;
                    v2 A0, B0, A1, B1;
                    float nxA0, nyA0, nzA0, nxB0, nyB0, nzB0;
                    float nxA1, nyA1, nzA1, nxB1, nyB1, nzB1;

                    if (rounded) {
                        float th0 = u0 * HALF_PI, th1 = u1 * HALF_PI;
                        z0 = hz - bd * (1.0f - cosf(th0));
                        z1 = hz - bd * (1.0f - cosf(th1));
                        A0 = rounded_pos(c0, o0, th0); B0 = rounded_pos(c1, o1, th0);
                        A1 = rounded_pos(c0, o0, th1); B1 = rounded_pos(c1, o1, th1);
                        rounded_normal(n0, th0, run, bd, &nxA0, &nyA0, &nzA0);
                        rounded_normal(n1, th0, run, bd, &nxB0, &nyB0, &nzB0);
                        rounded_normal(n0, th1, run, bd, &nxA1, &nyA1, &nzA1);
                        rounded_normal(n1, th1, run, bd, &nxB1, &nyB1, &nzB1);
                    } else {
                        z0 = hz - bd * u0; z1 = hz - bd * u1;
                        A0 = (v2){ c0.x + (o0.x - c0.x) * u0, c0.y + (o0.y - c0.y) * u0 };
                        B0 = (v2){ c1.x + (o1.x - c1.x) * u0, c1.y + (o1.y - c1.y) * u0 };
                        A1 = (v2){ c0.x + (o0.x - c0.x) * u1, c0.y + (o0.y - c0.y) * u1 };
                        B1 = (v2){ c1.x + (o1.x - c1.x) * u1, c1.y + (o1.y - c1.y) * u1 };
                        nxA0 = nxA1 = fnxA; nyA0 = nyA1 = fnyA; nzA0 = nzA1 = fnzA;
                        nxB0 = nxB1 = fnxB; nyB0 = nyB1 = fnyB; nzB0 = nzB1 = fnzB;
                    }

                    unsigned f = (unsigned)vb.n;
                    vpush(&vb, (MeshVertex){ A0.x, A0.y,  z0, nxA0, nyA0,  nzA0, 3 });
                    vpush(&vb, (MeshVertex){ B0.x, B0.y,  z0, nxB0, nyB0,  nzB0, 3 });
                    vpush(&vb, (MeshVertex){ B1.x, B1.y,  z1, nxB1, nyB1,  nzB1, 3 });
                    vpush(&vb, (MeshVertex){ A1.x, A1.y,  z1, nxA1, nyA1,  nzA1, 3 });
                    quad(&ib, f + 0, f + 1, f + 2, f + 3);
                    unsigned b = (unsigned)vb.n;
                    vpush(&vb, (MeshVertex){ A1.x, A1.y, -z1, nxA1, nyA1, -nzA1, 3 });
                    vpush(&vb, (MeshVertex){ B1.x, B1.y, -z1, nxB1, nyB1, -nzB1, 3 });
                    vpush(&vb, (MeshVertex){ B0.x, B0.y, -z0, nxB0, nyB0, -nzB0, 3 });
                    vpush(&vb, (MeshVertex){ A0.x, A0.y, -z0, nxA0, nyA0, -nzA0, 3 });
                    quad(&ib, b + 0, b + 1, b + 2, b + 3);
                }
            }
            free(os);
            wall_normals_free(&wn);
        }
        if (clamped) log_infof("bevel geom: %d contornos clampados", clamped);
    }

    /* parede reta por contorno (modo "desligado" - sem chanfro nenhum) */
    for (int ci = 0; !geombev && ci < cs->count; ++ci) {
        const Contour *co = &cs->contours[ci];
        if (co->count < 3) continue;

        int ccw = signed_area(co) > 0.0f;
        WallNormals wn;
        wall_normals_build(co->pts, co->count, ccw, &wn);

        for (int i = 0; i < co->count; ++i) {
            int j = (i + 1) % co->count;
            v2 a0 = co->pts[i], a1 = co->pts[j];
            v2 on = wn.edgeN[i];
            if (on.x == 0.0f && on.y == 0.0f) continue;
            v2 n0 = wall_normal_at(&wn, i, i);
            v2 n1 = wall_normal_at(&wn, j, i);

            /* parede: a0/a1 de +cap_z a -cap_z; normal suavizada por vertice
               (n0/n1) em vez da normal fixa da aresta, para curvas continuas. */
            unsigned w = (unsigned)vb.n;
            vpush(&vb, (MeshVertex){ a0.x, a0.y,  cap_z, n0.x, n0.y, 0, 2 });
            vpush(&vb, (MeshVertex){ a1.x, a1.y,  cap_z, n1.x, n1.y, 0, 2 });
            vpush(&vb, (MeshVertex){ a1.x, a1.y, -cap_z, n1.x, n1.y, 0, 2 });
            vpush(&vb, (MeshVertex){ a0.x, a0.y, -cap_z, n0.x, n0.y, 0, 2 });
            quad(&ib, w + 0, w + 1, w + 2, w + 3);
        }
        wall_normals_free(&wn);
    }

    /* paredes internas da casca oca */
    if (any_shell) {
        for (int ci = 0; ci < cs->count; ++ci) {
            if (!shell_ok[ci]) continue;
            const Contour *co = &cs->contours[ci];
            const v2 *in = inner[ci];
            for (int i = 0; i < co->count; ++i) {
                int j = (i + 1) % co->count;
                v2 a0 = in[i], a1 = in[j];
                float ex = a1.x - a0.x, ey = a1.y - a0.y;
                float el = sqrtf(ex * ex + ey * ey);
                if (el < 1e-9f) continue;
                float nx = -ey / el, ny = ex / el;   /* normal para dentro do oco */
                unsigned w = (unsigned)vb.n;
                vpush(&vb, (MeshVertex){ a0.x, a0.y,  cap_z, nx, ny, 0, 2 });
                vpush(&vb, (MeshVertex){ a1.x, a1.y,  cap_z, nx, ny, 0, 2 });
                vpush(&vb, (MeshVertex){ a1.x, a1.y, -cap_z, nx, ny, 0, 2 });
                vpush(&vb, (MeshVertex){ a0.x, a0.y, -cap_z, nx, ny, 0, 2 });
                quad(&ib, w + 0, w + 1, w + 2, w + 3);
            }
        }
    }
    if (inner) {
        for (int i = 0; i < cs->count; ++i) free(inner[i]);
        free(inner);
    }
    free(shell_ok);

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
