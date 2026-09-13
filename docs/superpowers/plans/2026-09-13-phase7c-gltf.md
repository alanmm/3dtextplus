# Fase 7c — Malha glTF/GLB — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Adicionar `.glb`/`.gltf` como terceiro formato de malha
importada, reaproveitando toda a infraestrutura já existente
(normalização, fallback de erro, o toggle `mesh_use_file_materials`,
o esquema de peças) — fecha o conjunto de formatos da Fase 7.

**Architecture:** Novo parser em `mesh_import.c` usando `cgltf.h`
(vendorizado). Percorre a árvore de nós da cena, aplicando
`cgltf_node_transform_world()` (função pronta da biblioteca) em cada
vértice antes de gerar a geometria. `mesh_import_load()` e
`mesh_import_load_pieces()` já são chamados de forma agnóstica a
formato pelo `scene.c` — **nenhuma mudança é necessária em `scene.c`
nem em `config.h`/`config.c`** nesta fase, só dentro de
`mesh_import.c`/`.h` e no filtro do seletor de arquivo.

**Tech Stack:** C11, `cgltf.h` (single-header, MIT, mesmo padrão de
vendoring já usado pra `nanosvg.h`/`fast_obj.h`).

## Global Constraints

- Sem texturas (`base_color_texture` e qualquer outra) — só
  `pbr_metallic_roughness.base_color_factor` vira cor sólida.
- Sem `metallic_factor`/`roughness_factor`/normal maps/emissive —
  ignorados (o material único configurado já controla isso).
- Só `cgltf_primitive_type_triangles` — outras topologias são
  ignoradas.
- Normais ausentes no arquivo → calculadas por face, sem suavização
  (mesmo espírito do OBJ/STL). **Limitação conhecida e aceita**: como
  a geometria do glTF já vem indexada (vértices compartilhados entre
  triângulos), calcular a normal "por face" nesse caso sobrescreve a
  normal de vértices compartilhados a cada triângulo processado (o
  último triângulo processado "vence") — resultado ligeiramente
  diferente do OBJ/STL (que não compartilham vértices entre faces).
  Aceitável porque arquivos glTF reais quase sempre trazem normais
  próprias (ao contrário do OBJ/STL, onde a ausência é comum);
  documentado aqui, não uma omissão.
- Transformação de normais usa só a parte 3x3 (rotação+escala) da
  matriz de mundo, sem tratamento especial pra escala não-uniforme
  (que exigiria a inversa-transposta) — simplificação aceitável dado
  que não há textura nem PBR fino nesta fase.
- Build: `mingw32-make -f build/Makefile debug` (ou `release`) da raiz,
  com w64devkit no PATH. Testes: `mingw32-make -f build/Makefile test`.
  Sempre `mingw32-make -f build/Makefile clean` antes de builds depois
  de editar um `.h` ou trocar debug↔release.

---

### Task 1: Vendorizar `cgltf.h` + suporte glTF em `mesh_import.c`

**Files:**
- Create: `third_party/cgltf.h`
- Modify: `src/geometry/stb_impl.c`
- Modify: `src/geometry/mesh_import.h`
- Modify: `src/geometry/mesh_import.c`
- Modify: `build/tests/test_mesh_import.c`

**Interfaces:**
- Extends: `mesh_import_load()` e `mesh_import_load_pieces()` (já
  existentes desde as Fases 7a/7b) passam a reconhecer `.glb`/`.gltf`
  — assinatura e comportamento externo inalterados, só a lista de
  extensões suportadas cresce.

- [ ] **Step 1: Vendorizar `cgltf.h`**

```bash
curl -sL "https://raw.githubusercontent.com/jkuhlmann/cgltf/master/cgltf.h" -o third_party/cgltf.h
wc -l third_party/cgltf.h
```

Esperado: arquivo com ~7200 linhas, licença MIT no topo (confirmar que
começa com `cgltf - a single-file glTF 2.0 parser written in C99`).

- [ ] **Step 2: Instanciar a implementação em `stb_impl.c`**

Em `src/geometry/stb_impl.c`, adicionar ao final:

```c
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
```

- [ ] **Step 3: Atualizar os comentários de doc em `mesh_import.h`**

Trocar:

```c
/* Carrega um arquivo .obj ou .stl (decidido pela extensao do caminho)
   e monta um MeshData pronto pra upload direto - sem tesselacao nem
   extrusao, a malha ja e 3D. Centraliza no centroide e normaliza pra
   uma esfera envolvente de raio 1.0 * size_scale. has_sdf sempre 0
   (malha importada nao tem bevel/SDF). Retorna 1 em sucesso; 0 se o
   arquivo nao existe, a extensao e desconhecida, ou nao ha nenhum
   triangulo valido. */
int mesh_import_load(const wchar_t *path, float size_scale, MeshData *out);
```

por:

```c
/* Carrega um arquivo .obj, .stl, .glb ou .gltf (decidido pela extensao
   do caminho) e monta um MeshData pronto pra upload direto - sem
   tesselacao nem extrusao, a malha ja e 3D. Centraliza no centroide e
   normaliza pra uma esfera envolvente de raio 1.0 * size_scale.
   has_sdf sempre 0 (malha importada nao tem bevel/SDF). Retorna 1 em
   sucesso; 0 se o arquivo nao existe, a extensao e desconhecida, ou
   nao ha nenhum triangulo valido. */
int mesh_import_load(const wchar_t *path, float size_scale, MeshData *out);
```

E trocar:

```c
/* Como mesh_import_load, mas so' funciona pra .obj com pelo menos um
   material REAL (nao-fallback, vindo de um .mtl de verdade referenciado
   via mtllib/usemtl): agrupa as faces por material, uma peca por
   material, com a cor Kd de cada um. Normaliza a UNIAO de todas as
   pecas junto (centroide e escala compartilhados, senao os grupos se
   desmontariam visualmente). Retorna 0 pra qualquer outro caso (.stl,
   extensao desconhecida, .obj sem material real ou so' com o material
   "fallback" que o fast_obj cria quando nao ha mtllib) - o chamador
   deve cair no mesh_import_load() normal nesse caso. */
int mesh_import_load_pieces(const wchar_t *path, float size_scale, MeshPieceSet *out);
```

por:

```c
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
```

- [ ] **Step 4: Helpers de extração glTF em `mesh_import.c`**

Adicionar, logo após `#include "fast_obj.h"` (novo include):

```c
#include "cgltf.h"
```

Adicionar, logo antes de `static void normalize_mesh(...)` (mesmo
lugar onde os helpers de OBJ/STL já terminam):

```c
typedef struct { MeshVertex *v; int n; unsigned *idx; int ni; } GltfPrimGeom;

/* transforma so' a parte 3x3 (rotacao+escala) da matriz de mundo -
   sem translacao, adequado pra normais. Nao trata escala nao-uniforme
   corretamente (exigiria a inversa-transposta), mas essa fase nao tem
   textura/PBR fino que dependa disso. */
static v3 gltf_transform_normal(m4 world, v3 n)
{
    return (v3){
        world.m[0]*n.x + world.m[4]*n.y + world.m[8]*n.z,
        world.m[1]*n.x + world.m[5]*n.y + world.m[9]*n.z,
        world.m[2]*n.x + world.m[6]*n.y + world.m[10]*n.z
    };
}

/* extrai geometria (posicoes+normais+indices) de UM primitive, ja
   transformada pela matriz de mundo do no que a contem. Retorna 0 se
   o primitive nao e' TRIANGLES ou nao tem POSITION. */
static int gltf_extract_primitive(cgltf_primitive *prim, m4 world, GltfPrimGeom *out)
{
    memset(out, 0, sizeof *out);
    if (prim->type != cgltf_primitive_type_triangles) return 0;

    cgltf_accessor *pos_acc = NULL, *nrm_acc = NULL;
    for (cgltf_size i = 0; i < prim->attributes_count; ++i) {
        if (prim->attributes[i].type == cgltf_attribute_type_position)
            pos_acc = prim->attributes[i].data;
        else if (prim->attributes[i].type == cgltf_attribute_type_normal)
            nrm_acc = prim->attributes[i].data;
    }
    if (!pos_acc || pos_acc->count == 0) return 0;

    cgltf_size nverts = pos_acc->count;
    float *positions = (float *)malloc(nverts * 3 * sizeof(float));
    if (!positions) return 0;
    cgltf_accessor_unpack_floats(pos_acc, positions, nverts * 3);

    float *normals = NULL;
    if (nrm_acc && nrm_acc->count == nverts) {
        normals = (float *)malloc(nverts * 3 * sizeof(float));
        if (normals) cgltf_accessor_unpack_floats(nrm_acc, normals, nverts * 3);
    }

    unsigned int nidx;
    unsigned int *idx;
    if (prim->indices) {
        nidx = (unsigned int)prim->indices->count;
        idx = (unsigned int *)malloc((size_t)nidx * sizeof(unsigned int));
        if (!idx) { free(positions); free(normals); return 0; }
        cgltf_accessor_unpack_indices(prim->indices, idx, sizeof(unsigned int), nidx);
    } else {
        nidx = (unsigned int)nverts;
        idx = (unsigned int *)malloc((size_t)nidx * sizeof(unsigned int));
        if (!idx) { free(positions); free(normals); return 0; }
        for (unsigned int i = 0; i < nidx; ++i) idx[i] = i;
    }
    if (nidx < 3 || nidx % 3 != 0) { free(positions); free(normals); free(idx); return 0; }

    MeshVertex *verts = (MeshVertex *)malloc((size_t)nverts * sizeof(MeshVertex));
    if (!verts) { free(positions); free(normals); free(idx); return 0; }
    for (cgltf_size i = 0; i < nverts; ++i) {
        v3 p = { positions[i*3+0], positions[i*3+1], positions[i*3+2] };
        p = m4_mul_point(world, p);
        v3 n = { 0.0f, 0.0f, 1.0f };
        if (normals) {
            v3 nraw = { normals[i*3+0], normals[i*3+1], normals[i*3+2] };
            n = gltf_transform_normal(world, nraw);
            float ln = v3_len(n);
            if (ln > 1e-6f) n = v3_scale(n, 1.0f / ln);
        }
        verts[i] = (MeshVertex){ p.x, p.y, p.z, n.x, n.y, n.z, 0.0f };
    }
    free(positions);
    free(normals);

    if (!normals) {
        for (unsigned int t = 0; t + 2 < nidx; t += 3) {
            unsigned int i0 = idx[t], i1 = idx[t+1], i2 = idx[t+2];
            v3 p0 = { verts[i0].px, verts[i0].py, verts[i0].pz };
            v3 p1 = { verts[i1].px, verts[i1].py, verts[i1].pz };
            v3 p2 = { verts[i2].px, verts[i2].py, verts[i2].pz };
            v3 fn = v3_norm(v3_cross(v3_sub(p1, p0), v3_sub(p2, p0)));
            verts[i0].nx = fn.x; verts[i0].ny = fn.y; verts[i0].nz = fn.z;
            verts[i1].nx = fn.x; verts[i1].ny = fn.y; verts[i1].nz = fn.z;
            verts[i2].nx = fn.x; verts[i2].ny = fn.y; verts[i2].nz = fn.z;
        }
    }

    out->v = verts; out->n = (int)nverts;
    out->idx = idx; out->ni = (int)nidx;
    return 1;
}

typedef struct { MeshVertex *v; int n; unsigned *idx; int ni; float r, g, b; } GltfChunk;
typedef struct { GltfChunk *c; int n, cap; } GltfChunkList;

static void gltf_chunklist_push(GltfChunkList *l, GltfChunk c)
{
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->c = (GltfChunk *)realloc(l->c, (size_t)l->cap * sizeof(GltfChunk));
    }
    l->c[l->n++] = c;
}

static void gltf_visit_node(cgltf_node *node, GltfChunkList *out)
{
    if (node->mesh) {
        float wraw[16];
        cgltf_node_transform_world(node, wraw);
        m4 world; memcpy(world.m, wraw, sizeof world.m);

        for (cgltf_size p = 0; p < node->mesh->primitives_count; ++p) {
            cgltf_primitive *prim = &node->mesh->primitives[p];
            GltfPrimGeom g;
            if (!gltf_extract_primitive(prim, world, &g)) continue;

            float r = 1.0f, gg = 1.0f, b = 1.0f;
            if (prim->material && prim->material->has_pbr_metallic_roughness) {
                r = prim->material->pbr_metallic_roughness.base_color_factor[0];
                gg = prim->material->pbr_metallic_roughness.base_color_factor[1];
                b = prim->material->pbr_metallic_roughness.base_color_factor[2];
            }
            GltfChunk c = { g.v, g.n, g.idx, g.ni, r, gg, b };
            gltf_chunklist_push(out, c);
        }
    }
    for (cgltf_size i = 0; i < node->children_count; ++i)
        gltf_visit_node(node->children[i], out);
}

/* percorre a cena default (ou a primeira, se nenhuma for marcada como
   default) e devolve uma lista de pedacos de geometria ja
   transformados por seus respectivos nos - usada tanto pelo caminho
   fundido quanto pelo de pecas. Retorna 0 se o parse/load falhar ou
   nao houver nenhum triangulo valido. */
static int gltf_collect_chunks(const wchar_t *path, GltfChunkList *out)
{
    memset(out, 0, sizeof *out);

    char u8[1024];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, u8, (int)sizeof u8, NULL, NULL);

    cgltf_options options;
    memset(&options, 0, sizeof options);
    cgltf_data *data = NULL;
    if (cgltf_parse_file(&options, u8, &data) != cgltf_result_success) return 0;
    if (cgltf_load_buffers(&options, data, u8) != cgltf_result_success) {
        cgltf_free(data);
        return 0;
    }

    if (data->scenes_count == 0) { cgltf_free(data); return 0; }
    cgltf_scene *scene = data->scene ? data->scene : &data->scenes[0];

    for (cgltf_size i = 0; i < scene->nodes_count; ++i)
        gltf_visit_node(scene->nodes[i], out);

    cgltf_free(data);

    if (out->n == 0) { free(out->c); out->c = NULL; return 0; }
    return 1;
}

static void gltf_chunklist_free(GltfChunkList *l)
{
    for (int i = 0; i < l->n; ++i) { free(l->c[i].v); free(l->c[i].idx); }
    free(l->c);
    memset(l, 0, sizeof *l);
}

static int load_gltf(const wchar_t *path, MeshData *out)
{
    GltfChunkList list;
    if (!gltf_collect_chunks(path, &list)) return 0;

    unsigned int total_v = 0, total_i = 0;
    for (int i = 0; i < list.n; ++i) {
        total_v += (unsigned)list.c[i].n;
        total_i += (unsigned)list.c[i].ni;
    }

    MeshVertex *verts = (MeshVertex *)malloc((size_t)total_v * sizeof(MeshVertex));
    unsigned *idx = (unsigned *)malloc((size_t)total_i * sizeof(unsigned));
    if (!verts || !idx) {
        free(verts); free(idx);
        gltf_chunklist_free(&list);
        return 0;
    }

    unsigned int vbase = 0, ibase = 0;
    for (int i = 0; i < list.n; ++i) {
        GltfChunk *c = &list.c[i];
        memcpy(verts + vbase, c->v, (size_t)c->n * sizeof(MeshVertex));
        for (int k = 0; k < c->ni; ++k) idx[ibase + (unsigned)k] = c->idx[k] + vbase;
        vbase += (unsigned)c->n;
        ibase += (unsigned)c->ni;
    }
    gltf_chunklist_free(&list);

    out->verts = verts; out->nverts = (int)total_v;
    out->idx = idx; out->nidx = (int)total_i;
    out->has_sdf = 0;
    memset(&out->sdf, 0, sizeof out->sdf);
    return 1;
}

static int load_gltf_pieces(const wchar_t *path, float size_scale, MeshPieceSet *out)
{
    memset(out, 0, sizeof *out);

    GltfChunkList list;
    if (!gltf_collect_chunks(path, &list)) return 0;

    MeshPiece *pieces = (MeshPiece *)calloc((size_t)list.n, sizeof(MeshPiece));
    if (!pieces) { gltf_chunklist_free(&list); return 0; }

    for (int i = 0; i < list.n; ++i) {
        pieces[i].data.verts = list.c[i].v;
        pieces[i].data.nverts = list.c[i].n;
        pieces[i].data.idx = list.c[i].idx;
        pieces[i].data.nidx = list.c[i].ni;
        pieces[i].data.has_sdf = 0;
        memset(&pieces[i].data.sdf, 0, sizeof pieces[i].data.sdf);
        pieces[i].r = list.c[i].r;
        pieces[i].g = list.c[i].g;
        pieces[i].b = list.c[i].b;
    }
    free(list.c);   /* os buffers v/idx de cada chunk foram passados pras pecas, nao liberar aqui */

    out->pieces = pieces;
    out->count = list.n;
    normalize_pieces(out, size_scale);
    return 1;
}
```

- [ ] **Step 5: Despachar pela extensão em `mesh_import_load` e
  `mesh_import_load_pieces`**

Trocar:

```c
int mesh_import_load(const wchar_t *path, float size_scale, MeshData *out)
{
    memset(out, 0, sizeof *out);
    if (!path || !path[0]) return 0;

    int ok;
    if (has_ext(path, L".obj"))
        ok = load_obj(path, out);
    else if (has_ext(path, L".stl"))
        ok = load_stl(path, out);
    else {
        log_errorf("mesh_import: extensao desconhecida");
        return 0;
    }
    if (!ok) return 0;

    normalize_mesh(out, size_scale);
    return 1;
}
```

por:

```c
int mesh_import_load(const wchar_t *path, float size_scale, MeshData *out)
{
    memset(out, 0, sizeof *out);
    if (!path || !path[0]) return 0;

    int ok;
    if (has_ext(path, L".obj"))
        ok = load_obj(path, out);
    else if (has_ext(path, L".stl"))
        ok = load_stl(path, out);
    else if (has_ext(path, L".glb") || has_ext(path, L".gltf"))
        ok = load_gltf(path, out);
    else {
        log_errorf("mesh_import: extensao desconhecida");
        return 0;
    }
    if (!ok) return 0;

    normalize_mesh(out, size_scale);
    return 1;
}
```

E, no início de `mesh_import_load_pieces` (logo após
`memset(out, 0, sizeof *out);`), adicionar o desvio pro glTF antes do
código de OBJ já existente:

```c
int mesh_import_load_pieces(const wchar_t *path, float size_scale, MeshPieceSet *out)
{
    memset(out, 0, sizeof *out);

    if (has_ext(path, L".glb") || has_ext(path, L".gltf"))
        return load_gltf_pieces(path, size_scale, out);

    char u8[1024];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, u8, (int)sizeof u8, NULL, NULL);
    /* ... resto do corpo existente da funcao, inalterado ... */
```

(o resto do corpo — parse OBJ, checagem de material fallback,
agrupamento por `face_materials`, etc. — continua exatamente como
está.)

- [ ] **Step 6: Testes glTF em `build/tests/test_mesh_import.c`**

Adicionar, logo após `write_obj_with_mtl` (novo helper de escrita de
`.glb` binário):

```c
static void write_glb(const char *json, const void *bin, size_t bin_len, wchar_t *out_path)
{
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"tmp", 0, out_path);
    size_t n = wcslen(out_path);
    wcsncpy(out_path + n - 3, L"glb", 3);

    size_t json_len = strlen(json);
    size_t json_pad = (4 - (json_len % 4)) % 4;
    size_t bin_pad = bin_len > 0 ? (4 - (bin_len % 4)) % 4 : 0;

    unsigned int magic = 0x46546C67, version = 2;
    unsigned int total_len = (unsigned int)(12 + 8 + json_len + json_pad
                                            + (bin_len > 0 ? 8 + bin_len + bin_pad : 0));
    unsigned char header[12];
    memcpy(header + 0, &magic, 4);
    memcpy(header + 4, &version, 4);
    memcpy(header + 8, &total_len, 4);

    unsigned int json_chunk_len = (unsigned int)(json_len + json_pad);
    unsigned int json_chunk_type = 0x4E4F534A;
    unsigned char json_chunk_header[8];
    memcpy(json_chunk_header + 0, &json_chunk_len, 4);
    memcpy(json_chunk_header + 4, &json_chunk_type, 4);

    HANDLE h = CreateFileW(out_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written = 0;
    WriteFile(h, header, 12, &written, NULL);
    WriteFile(h, json_chunk_header, 8, &written, NULL);
    WriteFile(h, json, (DWORD)json_len, &written, NULL);
    for (size_t i = 0; i < json_pad; ++i) { char sp = ' '; WriteFile(h, &sp, 1, &written, NULL); }

    if (bin_len > 0) {
        unsigned int bin_chunk_len = (unsigned int)(bin_len + bin_pad);
        unsigned int bin_chunk_type = 0x004E4942;
        unsigned char bin_chunk_header[8];
        memcpy(bin_chunk_header + 0, &bin_chunk_len, 4);
        memcpy(bin_chunk_header + 4, &bin_chunk_type, 4);
        WriteFile(h, bin_chunk_header, 8, &written, NULL);
        WriteFile(h, bin, (DWORD)bin_len, &written, NULL);
        for (size_t i = 0; i < bin_pad; ++i) { char z = 0; WriteFile(h, &z, 1, &written, NULL); }
    }
    CloseHandle(h);
}
```

E, ao final de `run_mesh_import_tests(void)` (antes do `}` que fecha a
função, depois dos casos de OBJ+material da Fase 7b):

```c
    /* .glb: triangulo sem normais/material -> normal calculada, cor fallback branca */
    {
        const char *json =
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"byteLength\":36}],"
            "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
            "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
                            "\"min\":[0.0,0.0,0.0],\"max\":[1.0,1.0,0.0]}],"
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"mode\":4}]}],"
            "\"nodes\":[{\"mesh\":0}],"
            "\"scenes\":[{\"nodes\":[0]}],"
            "\"scene\":0}";
        float bin[9] = { 0,0,0, 1,0,0, 0,1,0 };
        wchar_t path[MAX_PATH];
        write_glb(json, bin, sizeof bin, path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(md.nverts == 3);
        EXPECT(fabsf(md.verts[0].nz) > 0.9f);   /* normal calculada, triangulo plano em XY */
        mesh_data_free(&md);
    }

    /* .glb: 2 nos com 2 materiais (baseColorFactor diferentes) -> 2 pecas com cores distintas */
    {
        const char *json =
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"byteLength\":72}],"
            "\"bufferViews\":["
              "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
              "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36}],"
            "\"accessors\":["
              "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
               "\"min\":[0.0,0.0,0.0],\"max\":[1.0,1.0,0.0]},"
              "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
               "\"min\":[2.0,0.0,0.0],\"max\":[3.0,1.0,0.0]}],"
            "\"materials\":["
              "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[1.0,0.0,0.0,1.0]}},"
              "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.0,0.0,1.0,1.0]}}],"
            "\"meshes\":["
              "{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"material\":0,\"mode\":4}]},"
              "{\"primitives\":[{\"attributes\":{\"POSITION\":1},\"material\":1,\"mode\":4}]}],"
            "\"nodes\":[{\"mesh\":0},{\"mesh\":1}],"
            "\"scenes\":[{\"nodes\":[0,1]}],"
            "\"scene\":0}";
        float bin[18] = { 0,0,0, 1,0,0, 0,1,0,   2,0,0, 3,0,0, 2,1,0 };
        wchar_t path[MAX_PATH];
        write_glb(json, bin, sizeof bin, path);

        MeshPieceSet ps;
        int ok = mesh_import_load_pieces(path, 1.0f, &ps);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(ps.count == 2);
        if (ps.count == 2) {
            EXPECT(ps.pieces[0].r > 0.9f && ps.pieces[0].b < 0.1f);
            EXPECT(ps.pieces[1].r < 0.1f && ps.pieces[1].b > 0.9f);
        }
        mesh_import_pieces_free(&ps);
    }

    /* .glb: no com translation aplicada corretamente (cgltf_node_transform_world) */
    {
        const char *json =
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"byteLength\":72}],"
            "\"bufferViews\":["
              "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
              "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36}],"
            "\"accessors\":["
              "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
               "\"min\":[0.0,0.0,0.0],\"max\":[1.0,1.0,0.0]},"
              "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
               "\"min\":[0.0,0.0,0.0],\"max\":[1.0,1.0,0.0]}],"
            "\"meshes\":["
              "{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"mode\":4}]},"
              "{\"primitives\":[{\"attributes\":{\"POSITION\":1},\"mode\":4}]}],"
            "\"nodes\":[{\"mesh\":0},{\"mesh\":1,\"translation\":[100.0,0.0,0.0]}],"
            "\"scenes\":[{\"nodes\":[0,1]}],"
            "\"scene\":0}";
        /* mesmo triangulo local nos dois nos - o node 1 so' difere pela translation */
        float bin[18] = { 0,0,0, 1,0,0, 0,1,0,   0,0,0, 1,0,0, 0,1,0 };
        wchar_t path[MAX_PATH];
        write_glb(json, bin, sizeof bin, path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(md.nverts == 6);
        float minx = 1e30f, maxx = -1e30f;
        for (int i = 0; i < md.nverts; ++i) {
            if (md.verts[i].px < minx) minx = md.verts[i].px;
            if (md.verts[i].px > maxx) maxx = md.verts[i].px;
        }
        /* sem a translacao de 100 unidades no node 1, os dois triangulos
           coincidiriam e o espalhamento em X seria ~1 unidade (o tamanho
           de um triangulo isolado); com ela aplicada antes da normalizacao,
           a separacao original domina e o espalhamento final fica bem maior */
        EXPECT((maxx - minx) > 1.5f);
        mesh_data_free(&md);
    }

    /* .glb inexistente -> falha graciosa */
    {
        MeshData md;
        int ok = mesh_import_load(L"C:\\caminho\\que\\nao\\existe.glb", 1.0f, &md);
        EXPECT(!ok);
    }
```

- [ ] **Step 7: Rodar os testes**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```

Esperado: `all tests passed`, incluindo os 4 novos casos de glTF.

- [ ] **Step 8: Commit**

```bash
git add third_party/cgltf.h src/geometry/stb_impl.c src/geometry/mesh_import.h \
        src/geometry/mesh_import.c build/tests/test_mesh_import.c
git commit -m "feat: add glTF/GLB support to mesh_import.c via cgltf.h"
```

---

### Task 2: Filtro do seletor de arquivo

**Files:**
- Modify: `src/config_dialog.c`

**Interfaces:** nenhuma nova — só o texto do filtro do
`OPENFILENAMEW` já usado pelo `IDC_MESHPICK`.

- [ ] **Step 1: Estender o filtro em `content_proc`**

Trocar:

```c
                    ofn.lpstrFilter =
                        L"Malha 3D (*.obj, *.stl)\0*.obj;*.stl\0"
                        L"Arquivos OBJ (*.obj)\0*.obj\0"
                        L"Arquivos STL (*.stl)\0*.stl\0"
                        L"Todos os arquivos (*.*)\0*.*\0";
```

por:

```c
                    ofn.lpstrFilter =
                        L"Malha 3D (*.obj, *.stl, *.glb, *.gltf)\0*.obj;*.stl;*.glb;*.gltf\0"
                        L"Arquivos OBJ (*.obj)\0*.obj\0"
                        L"Arquivos STL (*.stl)\0*.stl\0"
                        L"Arquivos GLB (*.glb)\0*.glb\0"
                        L"Arquivos glTF (*.gltf)\0*.gltf\0"
                        L"Todos os arquivos (*.*)\0*.*\0";
```

- [ ] **Step 2: Build de depuração + checagem de abertura da aba**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile debug
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

```powershell
$env:M3DT_SELFTEST = "1"
$env:M3DT_TAB = "0"
$env:M3DT_HOLD_MS = "4000"
$p = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p | Wait-Process -Timeout 8
Write-Output "exitcode: $($p.ExitCode)"
```

Esperado: `exitcode: 0`, sem crash.

- [ ] **Step 3: Commit**

```bash
git add src/config_dialog.c
git commit -m "feat: accept .glb/.gltf in the mesh file picker filter"
```

---

### Task 3: Verificação final

**Files:** nenhum (só execução/validação).

**Aviso ao usuário**: tentar `PushNotification` antes das capturas
reais deste task; se vier "not sent", mandar mensagem de chat e
**esperar confirmação explícita** (`AskUserQuestion`) antes de
prosseguir. Reforçar: sempre re-setar TODAS as variáveis de ambiente em
CADA chamada do PowerShell.

- [ ] **Step 1: Suite de testes completa**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```

- [ ] **Step 2: Criar um `.gltf`+`.bin` de exemplo (2 nós, 2 materiais)**

Reaproveita a mesma estrutura JSON do teste unitário de 2 materiais,
mas como `.gltf` + `.bin` externo (prova que a resolução de buffer
externo do `cgltf_load_buffers` funciona, não só o caminho `.glb`
embutido testado nos unitários):

```powershell
$bin_path = "$env:TEMP\m3dt_test_gltf.bin"
$floats = @(0,0,0, 1,0,0, 0,1,0,   2,0,0, 3,0,0, 2,1,0)
$bytes = New-Object byte[] (4 * $floats.Length)
for ($i = 0; $i -lt $floats.Length; $i++) {
    [BitConverter]::GetBytes([float]$floats[$i]).CopyTo($bytes, $i * 4)
}
[System.IO.File]::WriteAllBytes($bin_path, $bytes)

$gltf = @'
{
  "asset": {"version": "2.0"},
  "buffers": [{"uri": "m3dt_test_gltf.bin", "byteLength": 72}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 36},
    {"buffer": 0, "byteOffset": 36, "byteLength": 36}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0,0,0], "max": [1,1,0]},
    {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3", "min": [2,0,0], "max": [3,1,0]}
  ],
  "materials": [
    {"pbrMetallicRoughness": {"baseColorFactor": [0.9, 0.2, 0.2, 1.0]}},
    {"pbrMetallicRoughness": {"baseColorFactor": [0.2, 0.3, 0.9, 1.0]}}
  ],
  "meshes": [
    {"primitives": [{"attributes": {"POSITION": 0}, "material": 0, "mode": 4}]},
    {"primitives": [{"attributes": {"POSITION": 1}, "material": 1, "mode": 4}]}
  ],
  "nodes": [{"mesh": 0}, {"mesh": 1}],
  "scenes": [{"nodes": [0, 1]}],
  "scene": 0
}
'@
Set-Content -Path "$env:TEMP\m3dt_test_multimat.gltf" -Value $gltf -Encoding ASCII
```

- [ ] **Step 3: Capturar com o toggle ligado**

```powershell
$key = "HKCU:\Software\Modern3DText"
New-Item -Path $key -Force | Out-Null
Set-ItemProperty -Path $key -Name "content_mode" -Value "3"
Set-ItemProperty -Path $key -Name "mesh_path" -Value "$env:TEMP\m3dt_test_multimat.gltf"
Set-ItemProperty -Path $key -Name "mesh_size_scale" -Value "1.0"
Set-ItemProperty -Path $key -Name "mesh_use_file_materials" -Value "1"

$env:M3DT_SELFTEST = "1"
$env:M3DT_SHOT = "$env:TEMP\m3dt_gltf_material_on.png"
$env:M3DT_HOLD_MS = "4000"
$p = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p | Wait-Process -Timeout 10
Write-Output "exists: $(Test-Path $env:TEMP\m3dt_gltf_material_on.png)"
```

Usar `Read`: deve mostrar 2 triângulos com cores distintas (vermelho e
azul).

- [ ] **Step 4: Capturar com o toggle desligado**

```powershell
Set-ItemProperty -Path $key -Name "mesh_use_file_materials" -Value "0"
$env:M3DT_SELFTEST = "1"
$env:M3DT_HOLD_MS = "4000"
$env:M3DT_SHOT = "$env:TEMP\m3dt_gltf_material_off.png"
$p2 = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p2 | Wait-Process -Timeout 10
Write-Output "exists: $(Test-Path $env:TEMP\m3dt_gltf_material_off.png)"
```

Usar `Read`: os dois triângulos devem aparecer na cor única do
material configurado.

- [ ] **Step 5: Confirmar que a Fase 7a/7b (OBJ/STL) não regrediram**

```powershell
Set-ItemProperty -Path $key -Name "mesh_use_file_materials" -Value "1"
Set-ItemProperty -Path $key -Name "mesh_path" -Value "$env:TEMP\m3dt_test_multimat.obj"
$env:M3DT_SELFTEST = "1"
$env:M3DT_HOLD_MS = "4000"
$env:M3DT_SHOT = "$env:TEMP\m3dt_obj_regression.png"
$p3 = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p3 | Wait-Process -Timeout 10
Write-Output "exists: $(Test-Path $env:TEMP\m3dt_obj_regression.png)"
```

(reaproveita o cubo `.obj`+`.mtl` de 2 materiais da Fase 7b, se ainda
existir em `%TEMP%`; recriar com o mesmo conteúdo do plano da 7b se
necessário.) Usar `Read`: mesmo resultado da Fase 7b - cubo com
tampa/base vermelhas e laterais azuis.

- [ ] **Step 6: Restaurar o registro**

```powershell
Remove-Item -Path "HKCU:\Software\Modern3DText" -Recurse -Force -ErrorAction SilentlyContinue
```

- [ ] **Step 7: Build release final + refresh do `.exe`**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile release
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

---

## Self-Review (executado antes de apresentar o plano)

1. **Cobertura do spec/design**: §2 arquitetura (parser via cgltf,
   traversal de nós + `cgltf_node_transform_world`, só TRIANGLES, sem
   textura/PBR fino, reaproveita o toggle e o esquema de peças) →
   Task 1; §3 (nenhum campo novo) → nenhuma tarefa necessária,
   confirmado; §4 interface (filtro do seletor) → Task 2; §5 testes →
   Task 1 (unitário) e Task 3 (visual); §6 fora de escopo não gera
   tarefas.
2. **Placeholders**: nenhum "TBD"/depois — todo trecho cola direto no
   arquivo indicado. A vendorização do `cgltf.h` usa o comando real de
   download.
3. **Consistência de tipos**: `gltf_extract_primitive`/
   `gltf_visit_node`/`gltf_collect_chunks`/`load_gltf`/
   `load_gltf_pieces` usam os mesmos tipos (`GltfPrimGeom`,
   `GltfChunk`, `GltfChunkList`) de ponta a ponta dentro do próprio
   Task 1; `load_gltf`/`load_gltf_pieces` produzem exatamente o que
   `mesh_import_load`/`mesh_import_load_pieces` (já existentes) esperam
   (`MeshData`/`MeshPieceSet`), sem mudar essas assinaturas públicas.
4. **Risco verificado à mão**: conferi a convenção de matriz do
   `cgltf_node_transform_world` direto no código-fonte da biblioteca
   (não de memória) — confirmado column-major, **igual** à convenção
   `m4` já usada no projeto (`m[col*4+row]`), permitindo reaproveitar
   `m4_mul_point()` já existente sem transposição nem código de
   composição de matriz próprio. Também confirmei que `cgltf_options`
   deve ser zero-inicializado (documentado explicitamente no próprio
   header) e que `cgltf_scene.nodes` é um array de ponteiros
   (`cgltf_node**`), não de structs por valor.
5. **Sem mudança em `scene.c`/`config.h`/`config.c`/`resource.h`/
   `screensaver.rc`**: confirmado que `mesh_import_load()`/
   `mesh_import_load_pieces()` já são chamados de forma agnóstica ao
   formato desde as Fases 7a/7b — só a extensão do arquivo muda o
   parser interno, então nada além de `mesh_import.c`/`.h` e o filtro
   do seletor precisa mudar nesta fase.

---

**Plano completo e salvo em `docs/superpowers/plans/2026-09-13-phase7c-gltf.md`.**

Duas opções de execução:

1. **Subagent-Driven (recomendado)** — dispatco um subagente novo por
   task, com revisão entre elas.
2. **Inline Execution** — executo as tasks nesta sessão via
   `executing-plans`, em lote com checkpoints.

Qual prefere?
