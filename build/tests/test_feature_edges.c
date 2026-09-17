#include "test.h"
#include "geometry/feature_edges.h"
#include <stdlib.h>
#include <string.h>

/* cubo unitario: 8 vertices, 6 faces * 2 triangulos = 12 triangulos.
   Cada face e' um quad plano dividido em (q0,q1,q2)+(q0,q2,q3) - as 2
   metades de uma MESMA face compartilham normal identica (0 graus,
   nao deveria virar aresta "de verdade"); faces vizinhas fazem 90
   graus entre si (arestas reais do cubo). */
static const float CUBE_POS[8][3] = {
    {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},
    {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1},
};
static const unsigned CUBE_QUADS[6][4] = {
    {0,1,2,3}, {4,5,6,7}, {0,1,5,4}, {2,3,7,6}, {1,2,6,5}, {0,3,7,4},
};

static void build_cube(MeshData *md)
{
    memset(md, 0, sizeof *md);
    md->nverts = 8;
    md->verts = (MeshVertex *)calloc(8, sizeof(MeshVertex));
    for (int i = 0; i < 8; ++i) {
        md->verts[i].px = CUBE_POS[i][0];
        md->verts[i].py = CUBE_POS[i][1];
        md->verts[i].pz = CUBE_POS[i][2];
    }
    md->nidx = 6 * 2 * 3;
    md->idx = (unsigned *)malloc((size_t)md->nidx * sizeof(unsigned));
    int w = 0;
    for (int f = 0; f < 6; ++f) {
        unsigned q0 = CUBE_QUADS[f][0], q1 = CUBE_QUADS[f][1];
        unsigned q2 = CUBE_QUADS[f][2], q3 = CUBE_QUADS[f][3];
        md->idx[w++] = q0; md->idx[w++] = q1; md->idx[w++] = q2;
        md->idx[w++] = q0; md->idx[w++] = q2; md->idx[w++] = q3;
    }
    md->minx = md->miny = md->minz = -1;
    md->maxx = md->maxy = md->maxz = 1;
}

void run_feature_edges_tests(void)
{
    MeshData cube;
    build_cube(&cube);

    WireEdge *edges = NULL;
    int count = 0;
    int ok = feature_edges_build(&cube, 35.0f, &edges, &count);
    EXPECT(ok);
    /* as 12 arestas reais do cubo entram; as 6 diagonais internas
       (0 grau entre as 2 metades de cada face) NAO entram */
    EXPECT_EQ_INT(count, 12);
    free(edges);
    free(cube.verts);
    free(cube.idx);

    /* um unico triangulo solto: as 3 arestas sao bordas abertas (1 so
       vizinho cada) - todas devem entrar, mesmo sem nenhum vinco de
       verdade pra medir */
    MeshData tri;
    memset(&tri, 0, sizeof tri);
    tri.nverts = 3;
    tri.verts = (MeshVertex *)calloc(3, sizeof(MeshVertex));
    tri.verts[0].px = 0; tri.verts[0].py = 0; tri.verts[0].pz = 0;
    tri.verts[1].px = 1; tri.verts[1].py = 0; tri.verts[1].pz = 0;
    tri.verts[2].px = 0; tri.verts[2].py = 1; tri.verts[2].pz = 0;
    tri.nidx = 3;
    tri.idx = (unsigned *)malloc(3 * sizeof(unsigned));
    tri.idx[0] = 0; tri.idx[1] = 1; tri.idx[2] = 2;

    WireEdge *tedges = NULL;
    int tcount = 0;
    ok = feature_edges_build(&tri, 35.0f, &tedges, &tcount);
    EXPECT(ok);
    EXPECT_EQ_INT(tcount, 3);
    free(tedges);
    free(tri.verts);
    free(tri.idx);

    /* malha vazia (0 triangulos) - retorna 0, nao crasha */
    MeshData empty;
    memset(&empty, 0, sizeof empty);
    WireEdge *eedges = NULL;
    int ecount = -1;
    ok = feature_edges_build(&empty, 35.0f, &eedges, &ecount);
    EXPECT(!ok);
}
