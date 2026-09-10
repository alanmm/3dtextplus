#include "test.h"
#include "geometry/contour_mesh.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static ContourSet cs_make(int nc)
{
    ContourSet cs;
    cs.count = nc;
    cs.contours = (Contour *)calloc((size_t)nc, sizeof(Contour));
    cs.minx = cs.miny = -1; cs.maxx = cs.maxy = 1;
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

/* MeshParams: modo desligado (2) -> so tampa + paredes, sem SDF, sem micro-bevel */
static MeshParams mp_plain(float depth)
{
    MeshParams p; memset(&p, 0, sizeof p);
    p.depth = depth;
    p.bevel_mode = 2;
    p.quality = 0;
    return p;
}
static MeshParams mp_shading(float depth, float bevel)
{
    MeshParams p; memset(&p, 0, sizeof p);
    p.depth = depth;
    p.bevel_mode = 0;
    p.bevel_size = bevel;
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
        EXPECT(md.has_sdf == 0);
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

    /* quadrado, modo sombreado: gera SDF, dentro < 0 */
    {
        ContourSet cs = cs_make(1);
        float sq[] = { -1,-1,  1,-1,  1,1,  -1,1 };
        cs_set(&cs, 0, sq, 4);
        MeshData md;
        EXPECT(contour_mesh_build(&cs, mp_shading(0.5f, 0.08f), &md) == 1);
        EXPECT(md.has_sdf == 1 && md.sdf.res > 0);
        EXPECT(sdf_sample(&md.sdf, 0.0f, 0.0f) < 0.0f);
        int chamfer = 0;
        for (int i = 0; i < md.nverts; ++i)
            if (md.verts[i].surf > 2.5f) chamfer++;
        EXPECT(chamfer > 0);                       /* micro-bevel presente */
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

    /* "L" concavo, sombreado (offset do micro-bevel nao pode crashar) */
    {
        ContourSet cs = cs_make(1);
        float L[] = { 0,0,  2,0,  2,1,  1,1,  1,3,  0,3 };
        cs_set(&cs, 0, L, 6);
        MeshData md;
        EXPECT(contour_mesh_build(&cs, mp_shading(0.3f, 0.06f), &md) == 1);
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
        EXPECT(md.has_sdf == 0);
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

    /* degenerado: 2 pontos => vazio, sem crash */
    {
        ContourSet cs = cs_make(1);
        float two[] = { 0,0, 1,1 };
        cs_set(&cs, 0, two, 2);
        MeshData md;
        int r = contour_mesh_build(&cs, mp_shading(0.3f, 0.05f), &md);
        EXPECT(r == 0);
        EXPECT(md.has_sdf == 0);
        cs_free(&cs);
    }
}
