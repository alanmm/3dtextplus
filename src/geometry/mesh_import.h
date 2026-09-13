#ifndef M3DT_MESH_IMPORT_H
#define M3DT_MESH_IMPORT_H
#include "geometry/contour_mesh.h"
#include <wchar.h>

/* Carrega um arquivo .obj, .stl, .glb ou .gltf (decidido pela extensao
   do caminho) e monta um MeshData pronto pra upload direto - sem
   tesselacao nem extrusao, a malha ja e 3D. Centraliza no centroide e
   normaliza pra uma esfera envolvente de raio 1.0 * size_scale.
   has_sdf sempre 0 (malha importada nao tem bevel/SDF). Retorna 1 em
   sucesso; 0 se o arquivo nao existe, a extensao e desconhecida, ou
   nao ha nenhum triangulo valido. */
int mesh_import_load(const wchar_t *path, float size_scale, MeshData *out);

typedef struct {
    MeshData data;   /* has_sdf sempre 0, mesmo espirito de mesh_import_load */
    float r, g, b;   /* cor Kd do material desta peca (0..1) */
} MeshPiece;

typedef struct {
    MeshPiece *pieces;
    int        count;
} MeshPieceSet;

/* Como mesh_import_load, mas so' funciona pra .obj com pelo menos um
   material REAL (nao-fallback, vindo de um .mtl de verdade referenciado
   via mtllib/usemtl) ou pra .glb/.gltf: agrupa por material (.obj) ou
   uma peca por primitive (.glb/.gltf, cor = pbrMetallicRoughness.
   baseColorFactor, branco se o primitive nao tiver material). Normaliza
   a UNIAO de todas as pecas junto (centroide e escala compartilhados,
   senao os grupos se desmontariam visualmente). Retorna 0 pra qualquer
   outro caso (.stl, extensao desconhecida, .obj sem material real ou
   so' com o material "fallback" que o fast_obj cria quando nao ha
   mtllib) - o chamador deve cair no mesh_import_load() normal nesse
   caso. */
int mesh_import_load_pieces(const wchar_t *path, float size_scale, MeshPieceSet *out);
void mesh_import_pieces_free(MeshPieceSet *s);

#endif
