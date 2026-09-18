#include "test.h"
#include "geometry/contour_mesh.h"
#include "geometry/robust_offset.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static ContourSet cs_make(int nc)
{
    ContourSet cs;
    cs.count = nc;
    cs.contours = (Contour *)calloc((size_t)nc, sizeof(Contour));
    cs.minx = cs.miny = -1; cs.maxx = cs.maxy = 1;
    cs.fill_rule = 0;
    return cs;
}
static void cs_set(ContourSet *cs, int i, const float *xy, int npts)
{
    cs->contours[i].count = npts;
    cs->contours[i].pts = (v2 *)malloc((size_t)npts * sizeof(v2));
    for (int k = 0; k < npts; ++k) cs->contours[i].pts[k] = (v2){ xy[k * 2], xy[k * 2 + 1] };
}
static void cs_free(ContourSet *cs)
{
    for (int i = 0; i < cs->count; ++i) free(cs->contours[i].pts);
    free(cs->contours);
}

/* MeshParams: modo desligado (2) -> so tampa + paredes retas, sem chanfro */
static MeshParams mp_plain(float depth)
{
    MeshParams p; memset(&p, 0, sizeof p);
    p.depth = depth;
    p.bevel_mode = 2;
    p.quality = 0;
    return p;
}
static MeshParams mp_rounded(float depth, float bevel, int segs)
{
    MeshParams p; memset(&p, 0, sizeof p);
    p.depth = depth;
    p.bevel_mode = 0;
    p.bevel_size = bevel;
    p.bevel_depth = bevel;
    p.bevel_segments = segs;
    p.quality = 0;
    return p;
}
static MeshParams mp_geom(float depth, float bevel, int segs)
{
    MeshParams p; memset(&p, 0, sizeof p);
    p.depth = depth;
    p.bevel_mode = 1;
    p.bevel_size = bevel;
    p.bevel_depth = bevel;
    p.bevel_segments = segs;
    p.quality = 0;
    return p;
}

void run_contour_mesh_tests(void)
{
    /* quadrado, sem bevel */
    {
        ContourSet cs = cs_make(1);
        float sq[] = { -1,-1,  1,-1,  1,1,  -1,1 };
        cs_set(&cs, 0, sq, 4);
        MeshData md;
        EXPECT(contour_mesh_build(&cs, mp_plain(0.5f), &md) == 1);
        EXPECT(md.nverts > 0 && md.nidx > 0 && md.nidx % 3 == 0);
        EXPECT(fabsf(md.minz + 0.25f) < 1e-4f && fabsf(md.maxz - 0.25f) < 1e-4f);
        int caps = 0, walls = 0;
        for (int i = 0; i < md.nverts; ++i) {
            const MeshVertex *v = &md.verts[i];
            if (v->surf < 1.5f) { EXPECT(fabsf(fabsf(v->nz) - 1.0f) < 1e-3f); caps++; }
            else { EXPECT(fabsf(v->nz) < 1e-3f); walls++; }
        }
        EXPECT(caps > 0 && walls > 0);
        mesh_data_free(&md);
        cs_free(&cs);
    }

    /* quadrado, modo arredondado: verifica que o perfil e' mesmo curvo -
       a normal do vertice junto a tampa (z=hz) deve apontar quase reto
       pra cima (nz perto de 1), e a do vertice junto a parede (z=wall_z)
       deve ser quase toda radial (nz perto de 0) - isso so' e' verdade
       pra um arco de verdade, um chanfro reto teria a MESMA normal (um
       valor intermediario constante) nos dois extremos. */
    {
        ContourSet cs = cs_make(1);
        float sq[] = { -1,-1,  1,-1,  1,1,  -1,1 };
        cs_set(&cs, 0, sq, 4);
        MeshData md;
        EXPECT(contour_mesh_build(&cs, mp_rounded(0.4f, 0.15f, 4), &md) == 1);
        int chamfer = 0;
        float nz_at_cap = -2.0f, nz_at_wall = -2.0f;   /* sentinela: "nao achou" */
        const float hz = 0.2f, wall_z = 0.05f;         /* depth/2, hz-bevel_depth */
        for (int i = 0; i < md.nverts; ++i) {
            const MeshVertex *v = &md.verts[i];
            if (v->surf <= 2.5f || v->nz < -1e-4f) continue;
            chamfer++;
            if (fabsf(v->pz - hz) < 1e-3f) nz_at_cap = v->nz;
            if (fabsf(v->pz - wall_z) < 1e-3f) nz_at_wall = v->nz;
        }
        EXPECT(chamfer > 0);
        EXPECT(nz_at_cap > 0.98f);    /* quase reto pra cima, junto a tampa */
        EXPECT(nz_at_wall < 0.05f);   /* quase todo radial, junto a parede */
        mesh_data_free(&md);
        cs_free(&cs);
    }

    /* quadrado com furo */
    {
        ContourSet cs = cs_make(2);
        float outer[] = { -2,-2,  2,-2,  2,2,  -2,2 };
        float inner[] = { -1,1,  1,1,  1,-1,  -1,-1 };
        cs_set(&cs, 0, outer, 4);
        cs_set(&cs, 1, inner, 4);
        MeshData md;
        EXPECT(contour_mesh_build(&cs, mp_plain(0.4f), &md) == 1);
        EXPECT(md.nidx % 3 == 0 && md.nverts > 0);
        mesh_data_free(&md);
        cs_free(&cs);
    }

    /* furo via evenodd: MESMA orientacao nos dois contornos (nao seria
       furo em nonzero), mas fill_rule=1 deve mesmo assim criar um furo -
       prova que o parametro realmente chega ate a tesselacao */
    {
        ContourSet cs = cs_make(2);
        float outer[] = { -2,-2,  2,-2,  2,2,  -2,2 };   /* CCW */
        float inner[] = { -1,-1,  1,-1,  1,1,  -1,1 };   /* TAMBEM CCW */
        cs_set(&cs, 0, outer, 4);
        cs_set(&cs, 1, inner, 4);

        cs.fill_rule = 1;   /* evenodd */
        MeshData md_evenodd;
        EXPECT(contour_mesh_build(&cs, mp_plain(0.4f), &md_evenodd) == 1);

        cs.fill_rule = 0;   /* nonzero: mesma orientacao NAO cria furo */
        MeshData md_nonzero;
        EXPECT(contour_mesh_build(&cs, mp_plain(0.4f), &md_nonzero) == 1);

        /* evenodd deixa a area do furo vazia -> menos triangulos que
           nonzero, que preenche tudo (winding number 2 por dentro) */
        EXPECT(md_evenodd.nidx < md_nonzero.nidx);

        mesh_data_free(&md_evenodd);
        mesh_data_free(&md_nonzero);
        cs_free(&cs);
    }

    /* "L" concavo, arredondado (outset do chanfro nao pode crashar) */
    {
        ContourSet cs = cs_make(1);
        float L[] = { 0,0,  2,0,  2,1,  1,1,  1,3,  0,3 };
        cs_set(&cs, 0, L, 6);
        MeshData md;
        EXPECT(contour_mesh_build(&cs, mp_rounded(0.3f, 0.06f, 4), &md) == 1);
        EXPECT(md.nidx % 3 == 0);
        mesh_data_free(&md);
        cs_free(&cs);
    }

    /* modo geometrico: quadrado -> faixa de bevel, contagem coerente */
    {
        ContourSet cs = cs_make(1);
        float sq[] = { -1,-1,  1,-1,  1,1,  -1,1 };
        cs_set(&cs, 0, sq, 4);
        MeshData md;
        EXPECT(contour_mesh_build(&cs, mp_geom(0.4f, 0.15f, 4), &md) == 1);
        EXPECT(md.nidx % 3 == 0 && md.nverts > 0);
        int chamfer = 0;
        for (int i = 0; i < md.nverts; ++i) if (md.verts[i].surf > 2.5f) chamfer++;
        EXPECT(chamfer >= 4 * 4 * 2 * 4);   /* 4 arestas * 4 segs * 2 lados * 4 vertices */
        mesh_data_free(&md);
        cs_free(&cs);
    }

    /* modo geometrico: "L" concavo com bevel grande -> nao crasha */
    {
        ContourSet cs = cs_make(1);
        float L[] = { 0,0,  2,0,  2,1,  1,1,  1,3,  0,3 };
        cs_set(&cs, 0, L, 6);
        MeshData md;
        EXPECT(contour_mesh_build(&cs, mp_geom(0.3f, 0.4f, 3), &md) == 1);
        EXPECT(md.nidx % 3 == 0);
        mesh_data_free(&md);
        cs_free(&cs);
    }

    /* casca oca: quadrado grande -> anel + paredes internas */
    {
        ContourSet cs = cs_make(1);
        float sq[] = { -4,-4,  4,-4,  4,4,  -4,4 };
        cs_set(&cs, 0, sq, 4);
        MeshData md;
        MeshParams p = mp_plain(0.4f);
        p.shell = 1;
        p.wall_thickness = 0.8f;
        EXPECT(contour_mesh_build(&cs, p, &md) == 1);
        EXPECT(md.nidx % 3 == 0 && md.nverts > 0);
        mesh_data_free(&md);
        cs_free(&cs);
    }

    /* casca oca: quadrado pequeno com parede maior que a meia-largura -> shell pulado, sem crash */
    {
        ContourSet cs = cs_make(1);
        float sq[] = { -0.3f,-0.3f,  0.3f,-0.3f,  0.3f,0.3f,  -0.3f,0.3f };
        cs_set(&cs, 0, sq, 4);
        MeshData md;
        MeshParams p = mp_plain(0.4f);
        p.shell = 1;
        p.wall_thickness = 0.5f;
        EXPECT(contour_mesh_build(&cs, p, &md) == 1);
        EXPECT(md.nidx % 3 == 0);
        mesh_data_free(&md);
        cs_free(&cs);
    }

    /* modo geometrico numa curva quase circular: a normal do chanfro
       numa quina vinda de arestas vizinhas deve ficar quase igual
       (suavizada) - contraste com o quadrado logo abaixo, onde a
       quina de 90 graus deve continuar com normais bem diferentes
       (aresta viva preservada de proposito). */
    {
        const float PI = 3.14159265f;
        const int N = 32;
        float circ[64];
        for (int k = 0; k < N; ++k) {
            float ang = (float)k / (float)N * 2.0f * PI;
            circ[k * 2] = cosf(ang);
            circ[k * 2 + 1] = sinf(ang);
        }
        ContourSet cs = cs_make(1);
        cs_set(&cs, 0, circ, N);
        MeshData md;
        EXPECT(contour_mesh_build(&cs, mp_geom(0.4f, 0.1f, 1), &md) == 1);

        int corner = 8;   /* longe do wraparound */
        float cx = circ[corner * 2], cy = circ[corner * 2 + 1], cz = 0.2f;   /* hz = depth/2 */
        float nx_found[4], ny_found[4], nz_found[4];
        int nf = 0;
        for (int i = 0; i < md.nverts && nf < 4; ++i) {
            const MeshVertex *v = &md.verts[i];
            if (v->surf > 2.5f && v->nz >= -1e-4f &&
                fabsf(v->px - cx) < 1e-4f && fabsf(v->py - cy) < 1e-4f && fabsf(v->pz - cz) < 1e-4f) {
                nx_found[nf] = v->nx; ny_found[nf] = v->ny; nz_found[nf] = v->nz;
                nf++;
            }
        }
        EXPECT(nf == 2);
        float dot_smooth = nx_found[0] * nx_found[1] + ny_found[0] * ny_found[1] + nz_found[0] * nz_found[1];
        EXPECT(dot_smooth > 0.99f);
        mesh_data_free(&md);
        cs_free(&cs);
    }
    {
        ContourSet cs = cs_make(1);
        float sq[] = { -1,-1,  1,-1,  1,1,  -1,1 };
        cs_set(&cs, 0, sq, 4);
        MeshData md;
        EXPECT(contour_mesh_build(&cs, mp_geom(0.4f, 0.1f, 1), &md) == 1);

        float cx = 1.0f, cy = -1.0f, cz = 0.2f;   /* quina entre aresta 0 e aresta 1 */
        float nx_found[4], ny_found[4], nz_found[4];
        int nf = 0;
        for (int i = 0; i < md.nverts && nf < 4; ++i) {
            const MeshVertex *v = &md.verts[i];
            if (v->surf > 2.5f && v->nz >= -1e-4f &&
                fabsf(v->px - cx) < 1e-4f && fabsf(v->py - cy) < 1e-4f && fabsf(v->pz - cz) < 1e-4f) {
                nx_found[nf] = v->nx; ny_found[nf] = v->ny; nz_found[nf] = v->nz;
                nf++;
            }
        }
        EXPECT(nf == 2);
        /* as duas facetas ainda compartilham uma inclinacao Z parecida (o
           corte e' ~45 graus dos dois lados), entao o produto escalar nao
           vai a zero mesmo sem suavizacao - o que importa e' ficar bem
           abaixo do caso suavizado (~1.0) por causa do XY perpendicular
           entre as duas arestas do quadrado. */
        float dot_sharp = nx_found[0] * nx_found[1] + ny_found[0] * ny_found[1] + nz_found[0] * nz_found[1];
        EXPECT(dot_sharp < 0.8f);
        mesh_data_free(&md);
        cs_free(&cs);
    }

    /* "H" com ponte estreita: as 2 reentrancias concavas da ponte (vertices
       2 e 9) nao podem convergir pro MESMO ponto no offset robusto do
       chanfro - bug de colapso corrigido trocando o piso fixo de cosseno
       por offset robusto via Clipper2 (robust_offset.cpp); ver historico
       na memoria do projeto. */
    {
        float H[] = { 0,0, 1,0, 1,1.4f, 2,1.4f, 2,0, 3,0, 3,3, 2,3, 2,1.6f, 1,1.6f, 1,3, 0,3 };
        Contour c;
        c.count = 12;
        c.pts = (v2 *)malloc(12 * sizeof(v2));
        float area = 0.0f;
        for (int k = 0; k < 12; ++k) {
            c.pts[k] = (v2){ H[k * 2], H[k * 2 + 1] };
            v2 p0 = (v2){ H[k * 2], H[k * 2 + 1] };
            v2 p1 = (v2){ H[((k + 1) % 12) * 2], H[((k + 1) % 12) * 2 + 1] };
            area += p0.x * p1.y - p1.x * p0.y;
        }
        int ccw = area > 0.0f;

        v2 out[12];
        robust_offset_contour(&c, -0.5f, ccw, out);   /* d<0: outset do bevel */

        float dx = out[2].x - out[9].x, dy = out[2].y - out[9].y;
        EXPECT(sqrtf(dx * dx + dy * dy) > 0.05f);   /* nao colapsaram no mesmo ponto */
        free(c.pts);
    }

    /* degenerado: 2 pontos => vazio, sem crash */
    {
        ContourSet cs = cs_make(1);
        float two[] = { 0,0, 1,1 };
        cs_set(&cs, 0, two, 2);
        MeshData md;
        int r = contour_mesh_build(&cs, mp_rounded(0.3f, 0.05f, 4), &md);
        EXPECT(r == 0);
        cs_free(&cs);
    }
}
