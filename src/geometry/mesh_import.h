#ifndef M3DT_MESH_IMPORT_H
#define M3DT_MESH_IMPORT_H
#include "geometry/contour_mesh.h"
#include <wchar.h>

/* Carrega um arquivo .obj ou .stl (decidido pela extensao do caminho)
   e monta um MeshData pronto pra upload direto - sem tesselacao nem
   extrusao, a malha ja e 3D. Centraliza no centroide e normaliza pra
   uma esfera envolvente de raio 1.0 * size_scale. has_sdf sempre 0
   (malha importada nao tem bevel/SDF). Retorna 1 em sucesso; 0 se o
   arquivo nao existe, a extensao e desconhecida, ou nao ha nenhum
   triangulo valido. */
int mesh_import_load(const wchar_t *path, float size_scale, MeshData *out);

#endif
