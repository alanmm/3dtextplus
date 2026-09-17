#include "geometry/feature_edges.h"
#include <stdlib.h>
#include <math.h>

typedef struct {
    unsigned lo, hi;       /* indices canonicos do par, lo < hi; neighbor_count==0 = slot livre */
    v3  n1;                /* normal do 1o triangulo que tocou essa aresta */
    int neighbor_count;
    int is_feature;
} EdgeEntry;

typedef struct { EdgeEntry *slots; int cap; } EdgeTable;

typedef struct {
    long long key;
    unsigned  canon;   /* menor indice de vertice original visto nesta posicao */
    int       used;
} PosSlot;

static unsigned hash_pair(unsigned lo, unsigned hi)
{
    return lo * 2654435761u ^ hi * 40503u;
}

/* quantiza uma posicao pra uma chave inteira de 63 bits (21 por eixo) -
   tolera ruido de ponto flutuante entre triangulos gerados
   independentemente que deveriam compartilhar o mesmo vertice
   geometrico. Necessario porque a malha deste projeto NAO garante
   indices compartilhados entre triangulos vizinhos - o gerador de
   tampa/parede emite vertices proprios por triangulo mesmo quando 2
   triangulos tocam na mesma posicao (confirmado por instrumentacao:
   90%+ das arestas apareciam como "borda aberta" mesmo no meio de uma
   tampa plana, sem nenhum vinco real ali). */
static long long quantize_key(v3 p)
{
    long long qx = (long long)lroundf(p.x * 100000.0f);
    long long qy = (long long)lroundf(p.y * 100000.0f);
    long long qz = (long long)lroundf(p.z * 100000.0f);
    long long mask = (1LL << 21) - 1;
    return ((qx & mask) << 42) | ((qy & mask) << 21) | (qz & mask);
}

static unsigned hash_key(long long k)
{
    unsigned long long u = (unsigned long long)k;
    u ^= u >> 33; u *= 0xff51afd7ed558ccdULL; u ^= u >> 33;
    return (unsigned)u;
}

/* Constroi um mapeamento indice-original -> indice "canonico" (o menor
   indice original que ocupa a mesma posicao quantizada) - ver
   comentario de quantize_key(). Retorna NULL em falha de alocacao. */
static unsigned *build_canon_map(const MeshData *md)
{
    unsigned *canon = (unsigned *)malloc((size_t)md->nverts * sizeof(unsigned));
    if (!canon) return NULL;

    int cap = md->nverts * 2 + 17;
    PosSlot *slots = (PosSlot *)calloc((size_t)cap, sizeof(PosSlot));
    if (!slots) { free(canon); return NULL; }

    for (int i = 0; i < md->nverts; ++i) {
        v3 p = { md->verts[i].px, md->verts[i].py, md->verts[i].pz };
        long long key = quantize_key(p);
        unsigned idx = hash_key(key) % (unsigned)cap;
        for (int probe = 0; probe < cap; ++probe) {
            PosSlot *s = &slots[idx];
            if (!s->used) {
                s->used = 1; s->key = key; s->canon = (unsigned)i;
                canon[i] = (unsigned)i;
                break;
            }
            if (s->key == key) { canon[i] = s->canon; break; }
            idx = (idx + 1) % (unsigned)cap;
        }
    }
    free(slots);
    return canon;
}

/* acha o slot da aresta (i0,i1) na tabela, criando um vazio se for a
   1a vez que essa aresta aparece. Enderecamento aberto, sondagem
   linear - a capacidade (2x o maximo teorico de arestas distintas)
   mantem a tabela esparsa o bastante pra nunca precisar de rehash. */
static EdgeEntry *edge_find_or_insert(EdgeTable *t, unsigned i0, unsigned i1)
{
    unsigned lo = i0 < i1 ? i0 : i1;
    unsigned hi = i0 < i1 ? i1 : i0;
    unsigned idx = hash_pair(lo, hi) % (unsigned)t->cap;
    for (int probe = 0; probe < t->cap; ++probe) {
        EdgeEntry *e = &t->slots[idx];
        if (e->neighbor_count == 0) { e->lo = lo; e->hi = hi; return e; }
        if (e->lo == lo && e->hi == hi) return e;
        idx = (idx + 1) % (unsigned)t->cap;
    }
    return NULL;   /* tabela cheia - nao deveria acontecer com a capacidade abaixo */
}

int feature_edges_build(const MeshData *md, float crease_deg,
                         WireEdge **out, int *out_count)
{
    int ntri = md->nidx / 3;
    if (ntri <= 0) return 0;

    unsigned *canon = build_canon_map(md);
    if (!canon) return 0;

    EdgeTable table;
    table.cap = ntri * 6 + 17;   /* ate' 3*ntri arestas distintas - folga generosa */
    table.slots = (EdgeEntry *)calloc((size_t)table.cap, sizeof(EdgeEntry));
    if (!table.slots) { free(canon); return 0; }

    float crease_cos = cosf(crease_deg * (3.14159265f / 180.0f));

    for (int t = 0; t < ntri; ++t) {
        unsigned i0 = canon[md->idx[t * 3 + 0]];
        unsigned i1 = canon[md->idx[t * 3 + 1]];
        unsigned i2 = canon[md->idx[t * 3 + 2]];
        v3 p0 = { md->verts[i0].px, md->verts[i0].py, md->verts[i0].pz };
        v3 p1 = { md->verts[i1].px, md->verts[i1].py, md->verts[i1].pz };
        v3 p2 = { md->verts[i2].px, md->verts[i2].py, md->verts[i2].pz };
        v3 faceN = v3_norm(v3_cross(v3_sub(p1, p0), v3_sub(p2, p0)));

        unsigned edges[3][2] = { { i0, i1 }, { i1, i2 }, { i2, i0 } };
        for (int e = 0; e < 3; ++e) {
            EdgeEntry *entry = edge_find_or_insert(&table, edges[e][0], edges[e][1]);
            if (!entry) continue;   /* tabela cheia (nao deveria acontecer) */
            if (entry->neighbor_count == 0) {
                entry->n1 = faceN;
                entry->neighbor_count = 1;
                entry->is_feature = 1;   /* borda aberta ate' aparecer um 2o vizinho */
            } else if (entry->neighbor_count == 1) {
                /* fabsf: "coplanar" e' sobre o ANGULO entre os planos,
                   nao a orientacao de winding de cada triangulo -
                   contornos externos/internos (buracos, ex. o miolo do
                   "D") podem sair da triangulacao com winding relativo
                   invertido mesmo sendo geometricamente a mesma
                   superficie plana; sem o fabsf isso lia como uma
                   "dobra de 180 graus" e enchia o miolo da letra de
                   arestas falsas. */
                entry->is_feature = (fabsf(v3_dot(entry->n1, faceN)) < crease_cos);
                entry->neighbor_count = 2;
            } else {
                /* 3o+ triangulo na mesma aresta - malha nao-manifold,
                   caso degenerado. Mais seguro mostrar do que esconder. */
                entry->is_feature = 1;
                entry->neighbor_count++;
            }
        }
    }
    free(canon);

    int count = 0;
    for (int i = 0; i < table.cap; ++i)
        if (table.slots[i].neighbor_count > 0 && table.slots[i].is_feature) count++;

    if (count == 0) { free(table.slots); return 0; }

    WireEdge *edges_out = (WireEdge *)malloc((size_t)count * sizeof(WireEdge));
    if (!edges_out) { free(table.slots); return 0; }

    int w = 0;
    for (int i = 0; i < table.cap; ++i) {
        EdgeEntry *e = &table.slots[i];
        if (e->neighbor_count == 0 || !e->is_feature) continue;
        const MeshVertex *va = &md->verts[e->lo];
        const MeshVertex *vb = &md->verts[e->hi];
        edges_out[w].a = (v3){ va->px, va->py, va->pz };
        edges_out[w].b = (v3){ vb->px, vb->py, vb->pz };
        w++;
    }

    free(table.slots);
    *out = edges_out;
    *out_count = count;
    return 1;
}
