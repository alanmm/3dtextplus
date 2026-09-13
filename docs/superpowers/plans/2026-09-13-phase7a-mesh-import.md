# Fase 7a — Malha 3D Importada (OBJ + STL) — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Adicionar um quarto modo de conteúdo, `mesh`, que carrega um
arquivo `.obj` ou `.stl` e o exibe diretamente (sem tesselação/
extrusão), sempre com o material único do screensaver.

**Architecture:** Novo módulo `src/geometry/mesh_import.c` produz um
`MeshData` (o mesmo tipo que `contour_mesh_build()` já produz pra
texto/SVG) direto a partir do arquivo — sem passar por `ContourSet`/
libtess2. Como esta fase usa um único material pra malha inteira (sem
cor por submesh), o resultado é **uma peça só**, reaproveitando o
mesmo campo único `s->mesh`/`have_mesh` que texto e relógio já usam —
sem precisar do esquema de múltiplas peças criado pro SVG.

**Tech Stack:** C11, `fast_obj.h` (vendorizado, parse de `.obj`), parser
de `.stl` próprio (binário + ASCII, sem biblioteca externa).

## Global Constraints

- Sem materiais/texturas do arquivo nesta fase (sempre o material
  compartilhado do screensaver) — fica pra Fase 7b.
- Sem `.glb`/`.gltf` nesta fase — fica inteiro pra 7b.
- Sem WBOIT pro vidro — mantém o blend simples já existente.
- Normais ausentes no arquivo → calculadas por face, sem suavização.
- Build: `mingw32-make -f build/Makefile debug` (ou `release`) da raiz,
  com w64devkit no PATH. Testes: `mingw32-make -f build/Makefile test`.
  Sempre `mingw32-make -f build/Makefile clean` antes de builds depois
  de editar um `.h` ou trocar debug↔release.

---

### Task 1: Vendorizar `fast_obj.h` + módulo `src/geometry/mesh_import.c`

**Files:**
- Create: `third_party/fast_obj.h`
- Modify: `src/geometry/stb_impl.c`
- Create: `src/geometry/mesh_import.h`
- Create: `src/geometry/mesh_import.c`
- Create: `build/tests/test_mesh_import.c`
- Modify: `build/tests/test_main.c`
- Modify: `build/Makefile`

**Interfaces:**
- Produces: `int mesh_import_load(const wchar_t *path, float size_scale, MeshData *out);`
  (usa `MeshData`/`MeshVertex` de `contour_mesh.h`/`gl_core.h`, já
  existentes). Consumido pela Task 3 (`scene.c`).

- [ ] **Step 1: Vendorizar `fast_obj.h`**

```bash
curl -sL "https://raw.githubusercontent.com/thisistherk/fast_obj/master/fast_obj.h" -o third_party/fast_obj.h
wc -l third_party/fast_obj.h
```

Esperado: arquivo com ~1600 linhas, licença MIT no topo (confirmar que
começa com `Copyright (c) 2018-2021 Richard Knight`).

- [ ] **Step 2: Instanciar a implementação em `stb_impl.c`**

Em `src/geometry/stb_impl.c`, adicionar ao final:

```c
#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"
```

- [ ] **Step 3: `src/geometry/mesh_import.h`**

```c
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
```

- [ ] **Step 4: `src/geometry/mesh_import.c`**

```c
#include "geometry/mesh_import.h"
#include "util/log.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wctype.h>
#include <math.h>

#include "fast_obj.h"

static int has_ext(const wchar_t *path, const wchar_t *ext)
{
    size_t pl = wcslen(path), el = wcslen(ext);
    if (pl < el) return 0;
    const wchar_t *suffix = path + (pl - el);
    for (size_t i = 0; i < el; ++i)
        if (towlower(suffix[i]) != towlower(ext[i])) return 0;
    return 1;
}

static int load_obj(const wchar_t *path, MeshData *out)
{
    char u8[1024];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, u8, (int)sizeof u8, NULL, NULL);

    fastObjMesh *m = fast_obj_read(u8);
    if (!m) return 0;

    unsigned int ntris = 0;
    for (unsigned int f = 0; f < m->face_count; ++f)
        if (m->face_vertices[f] >= 3) ntris += m->face_vertices[f] - 2;

    if (ntris == 0) { fast_obj_destroy(m); return 0; }

    MeshVertex *verts = (MeshVertex *)malloc((size_t)ntris * 3 * sizeof(MeshVertex));
    unsigned *idx = (unsigned *)malloc((size_t)ntris * 3 * sizeof(unsigned));
    if (!verts || !idx) {
        free(verts); free(idx);
        fast_obj_destroy(m);
        return 0;
    }

    unsigned int vi = 0;
    unsigned int cursor = 0;

    for (unsigned int f = 0; f < m->face_count; ++f) {
        unsigned int fv = m->face_vertices[f];
        if (fv < 3) { cursor += fv; continue; }

        fastObjIndex face_idx[64];
        unsigned int use = fv > 64 ? 64 : fv;
        for (unsigned int k = 0; k < use; ++k) face_idx[k] = m->indices[cursor + k];

        v3 p0 = { m->positions[face_idx[0].p*3+0], m->positions[face_idx[0].p*3+1], m->positions[face_idx[0].p*3+2] };
        v3 p1 = { m->positions[face_idx[1].p*3+0], m->positions[face_idx[1].p*3+1], m->positions[face_idx[1].p*3+2] };
        v3 p2 = { m->positions[face_idx[2].p*3+0], m->positions[face_idx[2].p*3+1], m->positions[face_idx[2].p*3+2] };
        v3 fn = v3_norm(v3_cross(v3_sub(p1, p0), v3_sub(p2, p0)));

        for (unsigned int k = 1; k + 1 < use; ++k) {
            unsigned int tri[3] = { 0, k, k + 1 };
            for (int c = 0; c < 3; ++c) {
                fastObjIndex fi = face_idx[tri[c]];
                v3 pos = { m->positions[fi.p*3+0], m->positions[fi.p*3+1], m->positions[fi.p*3+2] };
                v3 nrm = fn;
                if (fi.n != 0 && m->normal_count > fi.n) {
                    v3 fnrm = { m->normals[fi.n*3+0], m->normals[fi.n*3+1], m->normals[fi.n*3+2] };
                    if (v3_len(fnrm) > 1e-6f) nrm = fnrm;
                }
                verts[vi] = (MeshVertex){ pos.x, pos.y, pos.z, nrm.x, nrm.y, nrm.z, 0.0f };
                idx[vi] = vi;
                vi++;
            }
        }
        cursor += fv;
    }

    fast_obj_destroy(m);

    out->verts = verts; out->nverts = (int)vi;
    out->idx = idx; out->nidx = (int)vi;
    out->has_sdf = 0;
    memset(&out->sdf, 0, sizeof out->sdf);
    return 1;
}

static int load_stl(const wchar_t *path, MeshData *out)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD fsize = GetFileSize(h, NULL);
    unsigned char *buf = (unsigned char *)malloc(fsize ? fsize : 1);
    DWORD rd = 0;
    if (!buf || (fsize > 0 && (!ReadFile(h, buf, fsize, &rd, NULL) || rd != fsize))) {
        CloseHandle(h);
        free(buf);
        return 0;
    }
    CloseHandle(h);

    int is_binary = 0;
    unsigned int ntris_bin = 0;
    if (fsize >= 84) {
        memcpy(&ntris_bin, buf + 80, 4);
        unsigned long long expected = 84ull + (unsigned long long)ntris_bin * 50ull;
        if (expected == (unsigned long long)fsize) is_binary = 1;
    }

    unsigned int ntris = 0;
    MeshVertex *verts = NULL;
    unsigned *idx = NULL;

    if (is_binary) {
        ntris = ntris_bin;
        if (ntris == 0) { free(buf); return 0; }
        verts = (MeshVertex *)malloc((size_t)ntris * 3 * sizeof(MeshVertex));
        idx = (unsigned *)malloc((size_t)ntris * 3 * sizeof(unsigned));
        if (!verts || !idx) { free(verts); free(idx); free(buf); return 0; }

        const unsigned char *p = buf + 84;
        for (unsigned int t = 0; t < ntris; ++t) {
            float rec[12];
            memcpy(rec, p, 48);
            p += 50;
            v3 n = { rec[0], rec[1], rec[2] };
            v3 pos[3];
            for (int c = 0; c < 3; ++c)
                pos[c] = (v3){ rec[3 + c*3 + 0], rec[3 + c*3 + 1], rec[3 + c*3 + 2] };
            if (v3_len(n) < 1e-6f)
                n = v3_norm(v3_cross(v3_sub(pos[1], pos[0]), v3_sub(pos[2], pos[0])));
            for (int c = 0; c < 3; ++c) {
                unsigned int v = t * 3 + (unsigned)c;
                verts[v] = (MeshVertex){ pos[c].x, pos[c].y, pos[c].z, n.x, n.y, n.z, 0.0f };
                idx[v] = v;
            }
        }
    } else {
        unsigned int vertex_count = 0;
        for (DWORD i = 0; i + 6 <= fsize; ++i)
            if (memcmp(buf + i, "vertex", 6) == 0) vertex_count++;
        if (vertex_count == 0 || vertex_count % 3 != 0) { free(buf); return 0; }
        ntris = vertex_count / 3;

        verts = (MeshVertex *)malloc((size_t)ntris * 3 * sizeof(MeshVertex));
        idx = (unsigned *)malloc((size_t)ntris * 3 * sizeof(unsigned));
        if (!verts || !idx) { free(verts); free(idx); free(buf); return 0; }

        char *text = (char *)malloc((size_t)fsize + 1);
        if (!text) { free(verts); free(idx); free(buf); return 0; }
        memcpy(text, buf, fsize);
        text[fsize] = 0;

        unsigned int vi = 0;
        char *cursor = text;
        while (*cursor && vi < ntris * 3) {
            char *facet = strstr(cursor, "facet normal");
            if (!facet) break;
            v3 n = { 0, 0, 0 };
            sscanf(facet, "facet normal %f %f %f", &n.x, &n.y, &n.z);
            cursor = facet + 12;

            v3 pos[3];
            int got = 0;
            for (int c = 0; c < 3; ++c) {
                char *vtx = strstr(cursor, "vertex");
                if (!vtx) break;
                pos[c] = (v3){ 0, 0, 0 };
                sscanf(vtx, "vertex %f %f %f", &pos[c].x, &pos[c].y, &pos[c].z);
                cursor = vtx + 6;
                got++;
            }
            if (got != 3) break;
            if (v3_len(n) < 1e-6f)
                n = v3_norm(v3_cross(v3_sub(pos[1], pos[0]), v3_sub(pos[2], pos[0])));
            for (int c = 0; c < 3; ++c) {
                verts[vi] = (MeshVertex){ pos[c].x, pos[c].y, pos[c].z, n.x, n.y, n.z, 0.0f };
                idx[vi] = vi;
                vi++;
            }
        }
        free(text);
        if (vi != ntris * 3) { free(verts); free(idx); free(buf); return 0; }
    }

    free(buf);
    out->verts = verts; out->nverts = (int)(ntris * 3);
    out->idx = idx; out->nidx = (int)(ntris * 3);
    out->has_sdf = 0;
    memset(&out->sdf, 0, sizeof out->sdf);
    return 1;
}

static void normalize_mesh(MeshData *out, float size_scale)
{
    if (out->nverts == 0) return;

    v3 centroid = { 0, 0, 0 };
    for (int i = 0; i < out->nverts; ++i)
        centroid = v3_add(centroid, (v3){ out->verts[i].px, out->verts[i].py, out->verts[i].pz });
    centroid = v3_scale(centroid, 1.0f / (float)out->nverts);

    float max_r = 0.0f;
    for (int i = 0; i < out->nverts; ++i) {
        v3 p = v3_sub((v3){ out->verts[i].px, out->verts[i].py, out->verts[i].pz }, centroid);
        float r = v3_len(p);
        if (r > max_r) max_r = r;
    }
    if (max_r < 1e-6f) max_r = 1.0f;

    float scale = size_scale / max_r;
    float minx = 1e30f, miny = 1e30f, minz = 1e30f, maxx = -1e30f, maxy = -1e30f, maxz = -1e30f;
    for (int i = 0; i < out->nverts; ++i) {
        MeshVertex *v = &out->verts[i];
        v->px = (v->px - centroid.x) * scale;
        v->py = (v->py - centroid.y) * scale;
        v->pz = (v->pz - centroid.z) * scale;
        if (v->px < minx) minx = v->px; if (v->px > maxx) maxx = v->px;
        if (v->py < miny) miny = v->py; if (v->py > maxy) maxy = v->py;
        if (v->pz < minz) minz = v->pz; if (v->pz > maxz) maxz = v->pz;
    }
    out->minx = minx; out->miny = miny; out->minz = minz;
    out->maxx = maxx; out->maxy = maxy; out->maxz = maxz;
}

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

- [ ] **Step 5: `build/tests/test_mesh_import.c`**

```c
#include "test.h"
#include "geometry/mesh_import.h"

#include <windows.h>
#include <string.h>
#include <math.h>

static void write_text_file(const char *content, const wchar_t *ext, wchar_t *out_path)
{
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"tmp", 0, out_path);
    /* GetTempFileNameW sempre cria com extensao .tmp - troca pra extensao pedida,
       ja que mesh_import_load decide o parser pela extensao do caminho */
    size_t n = wcslen(out_path);
    wcsncpy(out_path + n - 3, ext, 3);

    HANDLE h = CreateFileW(out_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written = 0;
    WriteFile(h, content, (DWORD)strlen(content), &written, NULL);
    CloseHandle(h);
}

static void write_binary_stl_triangle(wchar_t *out_path)
{
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"tmp", 0, out_path);
    size_t n = wcslen(out_path);
    wcsncpy(out_path + n - 3, L"stl", 3);

    unsigned char buf[84 + 50];
    memset(buf, 0, 80);
    unsigned int ntris = 1;
    memcpy(buf + 80, &ntris, 4);

    float rec[12] = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };
    memcpy(buf + 84, rec, 48);
    buf[84 + 48] = 0; buf[84 + 49] = 0;

    HANDLE h = CreateFileW(out_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written = 0;
    WriteFile(h, buf, sizeof buf, &written, NULL);
    CloseHandle(h);
}

static int nearf(float a, float b) { return fabsf(a - b) < 1e-3f; }

void run_mesh_import_tests(void)
{
    /* triangulo OBJ simples, sem normais -> normal calculada por face */
    {
        const char *obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
        wchar_t path[MAX_PATH];
        write_text_file(obj, L"obj", path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(md.nverts == 3);
        EXPECT(md.nidx == 3);
        EXPECT(fabsf(md.verts[0].nz) > 0.9f);   /* normal da face plana no XY -> +-Z */
        mesh_data_free(&md);
    }

    /* quad OBJ (n-gon) -> triangulado por leque em 2 triangulos */
    {
        const char *obj = "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n";
        wchar_t path[MAX_PATH];
        write_text_file(obj, L"obj", path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(md.nverts == 6);    /* 2 triangulos * 3 vertices, sem compartilhamento */
        EXPECT(md.nidx == 6);
        mesh_data_free(&md);
    }

    /* OBJ com normais explicitas -> usa a normal do arquivo, nao a calculada */
    {
        const char *obj =
            "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
            "vn 0 0 -1\nvn 0 0 -1\nvn 0 0 -1\n"
            "f 1//1 2//2 3//3\n";
        wchar_t path[MAX_PATH];
        write_text_file(obj, L"obj", path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(md.verts[0].nz < -0.9f);   /* normal do arquivo aponta pra -Z */
        mesh_data_free(&md);
    }

    /* STL binario: mesmo triangulo do primeiro teste */
    {
        wchar_t path[MAX_PATH];
        write_binary_stl_triangle(path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(md.nverts == 3);
        EXPECT(md.verts[0].nz > 0.9f);
        mesh_data_free(&md);
    }

    /* STL ASCII: mesmo triangulo, deve produzir geometria equivalente ao binario */
    {
        const char *stl =
            "solid teste\n"
            "facet normal 0 0 1\n"
            "outer loop\n"
            "vertex 0 0 0\n"
            "vertex 1 0 0\n"
            "vertex 0 1 0\n"
            "endloop\n"
            "endfacet\n"
            "endsolid teste\n";
        wchar_t path[MAX_PATH];
        write_text_file(stl, L"stl", path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(ok);
        EXPECT(md.nverts == 3);
        EXPECT(md.verts[0].nz > 0.9f);
        mesh_data_free(&md);
    }

    /* normalizacao: centroide perto de zero, raio da bbox perto do alvo */
    {
        const char *obj = "v 10 10 10\nv 11 10 10\nv 10 11 10\nf 1 2 3\n";
        wchar_t path[MAX_PATH];
        write_text_file(obj, L"obj", path);

        MeshData md;
        int ok = mesh_import_load(path, 2.0f, &md);
        DeleteFileW(path);

        EXPECT(ok);
        float cx = 0, cy = 0, cz = 0;
        for (int i = 0; i < md.nverts; ++i) { cx += md.verts[i].px; cy += md.verts[i].py; cz += md.verts[i].pz; }
        cx /= md.nverts; cy /= md.nverts; cz /= md.nverts;
        EXPECT(fabsf(cx) < 1e-3f && fabsf(cy) < 1e-3f && fabsf(cz) < 1e-3f);
        float maxr = 0.0f;
        for (int i = 0; i < md.nverts; ++i) {
            float r = sqrtf(md.verts[i].px*md.verts[i].px + md.verts[i].py*md.verts[i].py + md.verts[i].pz*md.verts[i].pz);
            if (r > maxr) maxr = r;
        }
        EXPECT(nearf(maxr, 2.0f));   /* size_scale=2.0 -> raio da esfera envolvente = 2.0 */
        mesh_data_free(&md);
    }

    /* arquivo inexistente -> falha graciosa, sem crash */
    {
        MeshData md;
        int ok = mesh_import_load(L"C:\\caminho\\que\\nao\\existe.obj", 1.0f, &md);
        EXPECT(!ok);
    }

    /* extensao desconhecida -> falha graciosa */
    {
        MeshData md;
        int ok = mesh_import_load(L"C:\\arquivo.xyz", 1.0f, &md);
        EXPECT(!ok);
    }
}
```

- [ ] **Step 6: Registrar em `build/tests/test_main.c`**

Adicionar `void run_mesh_import_tests(void);` junto das outras
declarações, e `run_mesh_import_tests();` junto das outras chamadas em
`main()`.

- [ ] **Step 7: `build/Makefile`**

Em `SRC_C`, adicionar `src/geometry/mesh_import.c`:

```makefile
SRC_C := src/main.c src/cmdline.c src/util/log.c src/util/mathx.c src/util/clockfmt.c src/gl_core.c \
         src/gl_window.c src/host_win32.c src/config_dialog.c src/config.c \
         src/material.c src/scene.c src/env.c src/post.c src/particles.c \
         src/render_tiers.c src/render_tiers_gl.c \
         src/geometry/font_outline.c src/geometry/stb_impl.c src/geometry/contour_mesh.c \
         src/geometry/sdf.c src/geometry/svg_shapes.c src/geometry/mesh_import.c
```

Em `TEST_SRC`/`TEST_UNITS`, adicionar as novas entradas:

```makefile
TEST_SRC   := build/tests/test_main.c build/tests/test_cmdline.c build/tests/test_log.c \
              build/tests/test_mathx.c build/tests/test_font_outline.c \
              build/tests/test_contour_mesh.c build/tests/test_config.c build/tests/test_sdf.c \
              build/tests/test_render_tiers.c build/tests/test_clockfmt.c build/tests/test_svg_shapes.c \
              build/tests/test_mesh_import.c
TEST_UNITS := src/cmdline.c src/util/log.c src/util/mathx.c src/util/clockfmt.c \
              src/geometry/font_outline.c src/geometry/contour_mesh.c src/config.c \
              src/geometry/sdf.c src/render_tiers.c src/geometry/svg_shapes.c src/geometry/mesh_import.c
```

- [ ] **Step 8: Rodar os testes**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```

Esperado: `all tests passed`, incluindo os 8 novos casos de
`mesh_import`.

- [ ] **Step 9: Commit**

```bash
git add third_party/fast_obj.h src/geometry/stb_impl.c src/geometry/mesh_import.h \
        src/geometry/mesh_import.c build/tests/test_mesh_import.c build/tests/test_main.c \
        build/Makefile
git commit -m "feat: add mesh_import.c - parse OBJ/STL into a normalized MeshData"
```

---

### Task 2: Config schema — `CONTENT_MESH`, `mesh_path`, `mesh_size_scale`

**Files:**
- Modify: `src/config.h`
- Modify: `src/config.c`
- Modify: `build/tests/test_config.c`

**Interfaces:**
- Produces: `CONTENT_MESH` (valor 3 de `ContentMode`), `Config.mesh_path`
  (`wchar_t[512]`), `Config.mesh_size_scale` (`float`, 0..2). Consumidos
  pela Task 3 (`scene.c`) e Task 5 (`config_dialog.c`).

- [ ] **Step 1: `ContentMode` e novos campos**

Em `src/config.h`, trocar:

```c
typedef enum { CONTENT_TEXT = 0, CONTENT_CLOCK = 1, CONTENT_SVG = 2 } ContentMode;
```

por:

```c
typedef enum { CONTENT_TEXT = 0, CONTENT_CLOCK = 1, CONTENT_SVG = 2, CONTENT_MESH = 3 } ContentMode;
```

E logo após `int svg_color_mode;` (antes de `} Config;`):

```c
    wchar_t      mesh_path[512];       /* caminho do .obj ou .stl escolhido */
    float        mesh_size_scale;      /* 0..2, ajuste fino sobre o tamanho normalizado */
```

- [ ] **Step 2: Defaults**

Em `src/config.c`, `config_defaults`, logo após `c->svg_color_mode = 0;`:

```c
    c->mesh_path[0] = 0;
    c->mesh_size_scale = 1.0f;
```

- [ ] **Step 3: Carregar em `config_load_from`**

Logo após os `reg_get_w`/`reg_get_i` de `svg_path`/`svg_color_mode`:

```c
    reg_get_w(k, L"mesh_path", c->mesh_path, 512);
    if (reg_get_f(k, L"mesh_size_scale", &f)) c->mesh_size_scale = f;
```

No bloco de saneamento, trocar o clamp de `content_mode` (0..2 →
0..3) e adicionar o clamp de `mesh_size_scale`:

```c
    if (c->content_mode < 0 || c->content_mode > 3) c->content_mode = CONTENT_TEXT;
    c->clock_show_date = c->clock_show_date ? 1 : 0;
    c->clock_show_seconds = c->clock_show_seconds ? 1 : 0;
    c->svg_color_mode = c->svg_color_mode ? 1 : 0;
    c->mesh_size_scale = clampf(c->mesh_size_scale, 0.0f, 2.0f);
```

(o `> 2` antigo era o bug a evitar de novo — sem esse ajuste,
`content_mode = CONTENT_MESH` seria sempre resetado pra `CONTENT_TEXT`
no load, mesmo erro já corrigido duas vezes nas fases anteriores para
o relógio e o SVG.)

- [ ] **Step 4: Salvar em `config_save_to`**

Logo após `set_f(k, L"svg_color_mode", ...)`:

```c
    set_w(k, L"mesh_path", c->mesh_path);
    set_f(k, L"mesh_size_scale", c->mesh_size_scale);
```

- [ ] **Step 5: Estender `build/tests/test_config.c`**

No bloco de defaults:

```c
    EXPECT(d.mesh_path[0] == 0);
    EXPECT(nearf(d.mesh_size_scale, 1.0f));
```

Round-trip dedicado (mesmo padrão usado pro SVG na Fase 6b — não
reaproveita o par `a`/`b` existente, que já fixa `content_mode` noutro
valor), logo após o bloco de round-trip do SVG:

```c
    /* round-trip dedicado a malha importada - mesmo motivo do SVG acima:
       nao reusa pares que ja fixam content_mode noutro valor */
    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);
    Config ma, mb;
    config_defaults(&ma);
    ma.content_mode = CONTENT_MESH;
    wcscpy(ma.mesh_path, L"C:\\modelos\\objeto.obj");
    ma.mesh_size_scale = 1.5f;
    config_save_to(&ma, TESTKEY);

    config_load_from(&mb, TESTKEY);
    EXPECT(mb.content_mode == CONTENT_MESH);
    EXPECT(wcscmp(mb.mesh_path, L"C:\\modelos\\objeto.obj") == 0);
    EXPECT(nearf(mb.mesh_size_scale, 1.5f));

    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);
```

No bloco de clamp (`kv[]`), adicionar mais uma entrada e ajustar o
contador do loop (de 25 para 26):

```c
            { L"svg_color_mode", L"9" },
            { L"mesh_size_scale", L"9" },
        };
        for (int i = 0; i < 26; ++i)
```

E na verificação final:

```c
    EXPECT(e.mesh_size_scale <= 2.0f);   /* 9 -> clamp */
```

Também corrigir o comentário do clamp de `content_mode` (de `0..2` pra
`0..3`, já que o valor "9" de teste continua fora de faixa de qualquer
forma, só o comentário fica desatualizado):

```c
    EXPECT(e.content_mode == CONTENT_TEXT);         /* 9 -> fora de 0..3 -> 0 */
```

- [ ] **Step 6: Rodar os testes**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```

- [ ] **Step 7: Commit**

```bash
git add src/config.h src/config.c build/tests/test_config.c
git commit -m "feat: add mesh content mode config fields (CONTENT_MESH, mesh_path, mesh_size_scale)"
```

---

### Task 3: Integração em `scene.c`

**Files:**
- Modify: `src/scene.c`

**Interfaces:**
- Consumes: `mesh_import_load()` (Task 1); `Config.mesh_path`/
  `mesh_size_scale`/`CONTENT_MESH` (Task 2).

- [ ] **Step 1: Include**

Em `src/scene.c`, adicionar:

```c
#include "geometry/mesh_import.h"
```

(junto de `#include "geometry/svg_shapes.h"`.)

- [ ] **Step 2: Novos campos em `SceneRenderer`**

Logo após `int have_svg_mesh;` (antes do `};` que fecha o struct):

```c
    wchar_t  mesh_path[512];
    float    mesh_size_scale;
```

(Nota: ao contrário do SVG, a malha importada **reaproveita** o campo
único `s->mesh`/`have_mesh` já existente — não precisa de um array de
peças nem de um `have_mesh_import` separado, porque esta fase sempre
usa um material só pra malha inteira.)

- [ ] **Step 3: `rebuild_imported_mesh`**

Logo após `rebuild_svg_mesh` (antes de `SceneRenderer *scene_create`):

```c
static int rebuild_imported_mesh(SceneRenderer *s)
{
    MeshData md;
    int ok = s->mesh_path[0] != 0 && mesh_import_load(s->mesh_path, s->mesh_size_scale, &md);
    if (!ok) {
        strncpy(s->text, "Malha nao encontrada ou invalida", sizeof s->text - 1);
        s->text[sizeof s->text - 1] = 0;
        return rebuild_mesh(s);
    }

    if (s->have_mesh) gl_mesh_free(&s->mesh);
    s->mesh = gl_mesh_upload(md.verts, md.nverts, md.idx, md.nidx);
    s->hx = 0.5f * (md.maxx - md.minx);
    s->hy = 0.5f * (md.maxy - md.miny);
    s->hz = 0.5f * (md.maxz - md.minz);
    if (s->hx < 1e-3f) s->hx = 1.0f;
    if (s->hy < 1e-3f) s->hy = 1.0f;
    s->wall_cache_count = 0;   /* malha importada nao tem paredes - faiscas nao emitem nela */
    upload_sdf(s, NULL);       /* sem bevel/SDF pra malha importada */
    int nv = md.nverts;
    mesh_data_free(&md);
    s->have_mesh = 1;
    log_infof("scene: malha '%ls' -> %d verts", s->mesh_path, nv);
    return 1;
}
```

- [ ] **Step 4: `scene_set_config` — comparação e despacho**

Logo após a declaração de `svg_relevant_change`, adicionar:

```c
    int mesh_relevant_change = (cfg->content_mode == CONTENT_MESH)
        && (wcscmp(s->mesh_path, cfg->mesh_path) != 0 || s->mesh_size_scale != cfg->mesh_size_scale);
```

No cálculo de `mesh_dirty`, adicionar `|| mesh_relevant_change` ao
final da cadeia (depois de `|| svg_relevant_change;`):

```c
        || svg_relevant_change
        || mesh_relevant_change;
```

Logo após `s->svg_color_mode = cfg->svg_color_mode;`, adicionar:

```c
    wcsncpy(s->mesh_path, cfg->mesh_path, 511);
    s->mesh_path[511] = 0;
    s->mesh_size_scale = cfg->mesh_size_scale;
```

No despacho de rebuild, trocar:

```c
    if (mesh_dirty) {
        int ok = (s->content_mode == CONTENT_SVG) ? rebuild_svg_mesh(s) : rebuild_mesh(s);
        if (!ok)
            log_errorf("scene: rebuild falhou (content_mode=%d)", s->content_mode);
    }
```

por:

```c
    if (mesh_dirty) {
        int ok;
        if (s->content_mode == CONTENT_SVG) ok = rebuild_svg_mesh(s);
        else if (s->content_mode == CONTENT_MESH) ok = rebuild_imported_mesh(s);
        else ok = rebuild_mesh(s);
        if (!ok)
            log_errorf("scene: rebuild falhou (content_mode=%d)", s->content_mode);
    }
```

(Nota: `scene_create()`'s bootstrap check e `scene_render()`'s early-
return já checam `!s->have_mesh && s->svg_mesh_count == 0` desde a
Fase 6b — como o modo `mesh` popula `s->mesh`/`have_mesh` do mesmo
jeito que texto/relógio, tanto no sucesso quanto no fallback de erro,
essas duas checagens e o laço de desenho em `scene_render` **já
funcionam corretamente pro modo `mesh` sem nenhuma mudança adicional**.)

- [ ] **Step 5: Build de depuração completo**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile debug
```

Esperado: `Built dist/Modern3DText.scr (N bytes)`, sem erros.

- [ ] **Step 6: Commit**

```bash
git add src/scene.c
git commit -m "feat: render imported OBJ/STL mesh content in scene.c"
```

---

### Task 4: IDs de recurso + layout da aba "Conteúdo" para malha

**Files:**
- Modify: `src/resource.h`
- Modify: `res/screensaver.rc`

**Interfaces:**
- Produces: `IDC_MESHPATHLABEL`, `IDC_MESHPATH`, `IDC_MESHPICK`,
  `IDC_MESHCLEAR`, `IDC_MESHSCALELABEL`, `IDC_MESHSCALE_VAL`,
  `IDC_MESHSCALE` (1116-1122). Consumidos pela Task 5.

- [ ] **Step 1: Novos IDs em `src/resource.h`**

Logo após `#define IDC_SVGCOLORMODE  1115`:

```c
#define IDC_MESHPATHLABEL  1116
#define IDC_MESHPATH       1117
#define IDC_MESHPICK       1118
#define IDC_MESHCLEAR      1119
#define IDC_MESHSCALELABEL 1120
#define IDC_MESHSCALE_VAL  1121
#define IDC_MESHSCALE      1122
```

- [ ] **Step 2: Layout de `IDD_TAB_CONTENT` em `res/screensaver.rc`**

Os controles de malha ocupam a MESMA área de tela que Texto/Fonte/SVG
(y=42 a ~118) — só um grupo fica visível por vez, trocado via
`ShowWindow` em `content_enable()` (Task 5). Adicionar logo após o
bloco do SVG (depois de `COMBOBOX IDC_SVGCOLORMODE, ...` e antes de
`AUTOCHECKBOX "Negrito", ...`):

```rc
    LTEXT           "Arquivo:", IDC_MESHPATHLABEL, 8, 42, 60, 9
    LTEXT           "(nenhum)", IDC_MESHPATH, 8, 54, 224, 9, SS_PATHELLIPSIS
    PUSHBUTTON      "Escolher...", IDC_MESHPICK, 8, 66, 60, 14
    PUSHBUTTON      "Limpar", IDC_MESHCLEAR, 72, 66, 50, 14
    LTEXT           "Escala:", IDC_MESHSCALELABEL, 8, 92, 60, 9
    LTEXT           "", IDC_MESHSCALE_VAL, 182, 92, 50, 9
    CONTROL         "", IDC_MESHSCALE, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 102, 224, 16
```

- [ ] **Step 3: Verificar que o `.rc` compila**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
windres --include-dir res --include-dir src -O coff res/screensaver.rc -o build/obj/screensaver.res
echo "rc ok"
```

- [ ] **Step 4: Commit**

```bash
git add src/resource.h res/screensaver.rc
git commit -m "feat: add mesh file picker + scale slider controls to Conteudo tab layout"
```

---

### Task 5: Wiring em `config_dialog.c`

**Files:**
- Modify: `src/config_dialog.c`

**Interfaces:**
- Consumes: IDs da Task 4; `Config.mesh_path`/`mesh_size_scale`/
  `CONTENT_MESH` (Task 2); `set_slider()` (já existe no arquivo, mas
  definido depois de `content_proc` — precisa de declaração antecipada).

- [ ] **Step 1: Declaração antecipada de `set_slider`**

`set_slider()` já existe no arquivo (usado por várias abas), mas está
definido mais abaixo (seção "aba Movimento"), depois de `content_proc`.
Adicionar uma declaração antecipada logo antes de `content_svg_label`
(início da seção "aba Conteudo"):

```c
static void set_slider(HWND h, int id, int lo, int hi, int pos);
```

- [ ] **Step 2: `content_mesh_label` + `content_enable()` — 4 modos**

Trocar:

```c
static void content_svg_label(HWND h)
{
    SetDlgItemTextW(h, IDC_SVGPATH, g_work.svg_path[0] ? g_work.svg_path : L"(nenhum)");
}

static void content_enable(HWND h)
{
    int is_text = g_work.content_mode == CONTENT_TEXT;
    int is_svg  = g_work.content_mode == CONTENT_SVG;

    ShowWindow(GetDlgItem(h, IDC_TEXTLABEL), is_text ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_TEXT),      is_text ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_FONTLABEL), is_text ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_FONT),      is_text ? SW_SHOW : SW_HIDE);

    ShowWindow(GetDlgItem(h, IDC_SVGPATHLABEL),  is_svg ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_SVGPATH),       is_svg ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_SVGPICK),       is_svg ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_SVGCLEAR),      is_svg ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_SVGCOLORLABEL), is_svg ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_SVGCOLORMODE),  is_svg ? SW_SHOW : SW_HIDE);

    EnableWindow(GetDlgItem(h, IDC_CLOCKDATE), g_work.content_mode == CONTENT_CLOCK);
    EnableWindow(GetDlgItem(h, IDC_CLOCKSEC), g_work.content_mode == CONTENT_CLOCK);
}
```

por:

```c
static void content_svg_label(HWND h)
{
    SetDlgItemTextW(h, IDC_SVGPATH, g_work.svg_path[0] ? g_work.svg_path : L"(nenhum)");
}

static void content_mesh_label(HWND h)
{
    SetDlgItemTextW(h, IDC_MESHPATH, g_work.mesh_path[0] ? g_work.mesh_path : L"(nenhum)");
    wchar_t b[32];
    swprintf(b, 32, L"%.2f", (double)g_work.mesh_size_scale);
    SetDlgItemTextW(h, IDC_MESHSCALE_VAL, b);
}

static void content_enable(HWND h)
{
    int is_text = g_work.content_mode == CONTENT_TEXT;
    int is_svg  = g_work.content_mode == CONTENT_SVG;
    int is_mesh = g_work.content_mode == CONTENT_MESH;

    ShowWindow(GetDlgItem(h, IDC_TEXTLABEL), is_text ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_TEXT),      is_text ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_FONTLABEL), is_text ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_FONT),      is_text ? SW_SHOW : SW_HIDE);

    ShowWindow(GetDlgItem(h, IDC_SVGPATHLABEL),  is_svg ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_SVGPATH),       is_svg ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_SVGPICK),       is_svg ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_SVGCLEAR),      is_svg ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_SVGCOLORLABEL), is_svg ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_SVGCOLORMODE),  is_svg ? SW_SHOW : SW_HIDE);

    ShowWindow(GetDlgItem(h, IDC_MESHPATHLABEL),  is_mesh ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_MESHPATH),       is_mesh ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_MESHPICK),       is_mesh ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_MESHCLEAR),      is_mesh ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_MESHSCALELABEL), is_mesh ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_MESHSCALE_VAL),  is_mesh ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_MESHSCALE),      is_mesh ? SW_SHOW : SW_HIDE);

    EnableWindow(GetDlgItem(h, IDC_CLOCKDATE), g_work.content_mode == CONTENT_CLOCK);
    EnableWindow(GetDlgItem(h, IDC_CLOCKSEC), g_work.content_mode == CONTENT_CLOCK);
}
```

- [ ] **Step 3: `WM_INITDIALOG` — 4ª opção no combo de modo + slider de
  escala**

Trocar:

```c
            static const wchar_t *modes[] = { L"Texto", L"Relogio", L"SVG" };
            for (int i = 0; i < 3; ++i)
                SendDlgItemMessageW(h, IDC_CONTMODE, CB_ADDSTRING, 0, (LPARAM)modes[i]);
```

por:

```c
            static const wchar_t *modes[] = { L"Texto", L"Relogio", L"SVG", L"Malha 3D" };
            for (int i = 0; i < 4; ++i)
                SendDlgItemMessageW(h, IDC_CONTMODE, CB_ADDSTRING, 0, (LPARAM)modes[i]);
```

Logo após a inicialização do combo `IDC_SVGCOLORMODE` (antes de
`content_svg_label(h);`), adicionar:

```c
            set_slider(h, IDC_MESHSCALE, 0, 200, (int)(g_work.mesh_size_scale * 100.0f + 0.5f));
            content_mesh_label(h);
```

- [ ] **Step 4: `WM_HSCROLL` — novo caso em `content_proc`**

`content_proc` ainda não tem um `case WM_HSCROLL:` (só
`WM_INITDIALOG` e `WM_COMMAND`). Adicionar, logo depois do bloco
`case WM_INITDIALOG: { ... return TRUE; }` e antes de
`case WM_COMMAND:`:

```c
        case WM_HSCROLL:
            g_work.mesh_size_scale =
                (float)SendDlgItemMessageW(h, IDC_MESHSCALE, TBM_GETPOS, 0, 0) / 100.0f;
            content_mesh_label(h);
            preview_dirty(h);
            return TRUE;
```

- [ ] **Step 5: `WM_COMMAND` — seletor de arquivo**

Dentro do `switch (LOWORD(w))` de `content_proc`, adicionar (antes do
`case IDC_TEXT:` já existente, junto dos outros casos de SVG):

```c
                case IDC_MESHPICK: {
                    wchar_t file[512] = L"";
                    OPENFILENAMEW ofn;
                    memset(&ofn, 0, sizeof ofn);
                    ofn.lStructSize = sizeof ofn;
                    ofn.hwndOwner = h;
                    ofn.lpstrFilter = L"Malha 3D\0*.obj;*.stl\0Todos\0*.*\0";
                    ofn.lpstrFile = file;
                    ofn.nMaxFile = 512;
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                    if (GetOpenFileNameW(&ofn)) {
                        wcsncpy(g_work.mesh_path, file, 511);
                        g_work.mesh_path[511] = 0;
                        content_mesh_label(h);
                        preview_dirty(h);
                    }
                    break;
                }
                case IDC_MESHCLEAR:
                    g_work.mesh_path[0] = 0;
                    content_mesh_label(h);
                    preview_dirty(h);
                    break;
```

- [ ] **Step 6: Build de depuração + checagem de abertura da aba**

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

- [ ] **Step 7: Commit**

```bash
git add src/config_dialog.c
git commit -m "feat: wire mesh file picker and scale slider into Conteudo tab"
```

---

### Task 6: Verificação final

**Files:** nenhum (só execução/validação).

**Aviso ao usuário**: tentar `PushNotification` antes das capturas
reais deste task; se vier "not sent", mandar mensagem de chat e
**esperar confirmação explícita** (`AskUserQuestion`) antes de
prosseguir — combinado nesta sessão. Reforçar: sempre re-setar TODAS
as variáveis de ambiente (`M3DT_SELFTEST`, `M3DT_HOLD_MS`, `M3DT_SHOT`)
em CADA chamada do PowerShell — cada chamada é um processo novo, nada
persiste da anterior (gotcha já visto na Fase 6b).

- [ ] **Step 1: Suite de testes completa**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```

- [ ] **Step 2: Criar um `.obj` de exemplo simples pra teste manual**

```powershell
$obj = @'
v -1 -1 -1
v 1 -1 -1
v 1 1 -1
v -1 1 -1
v -1 -1 1
v 1 -1 1
v 1 1 1
v -1 1 1
f 1 2 3 4
f 5 8 7 6
f 1 5 6 2
f 2 6 7 3
f 3 7 8 4
f 4 8 5 1
'@
Set-Content -Path "$env:TEMP\m3dt_test_cube.obj" -Value $obj -Encoding ASCII
```

(cubo simples, 6 faces quad — prova a triangulação por fan em geometria
real, não só nas fixtures do teste unitário.)

- [ ] **Step 3: Capturar o modo Malha 3D**

```powershell
$key = "HKCU:\Software\Modern3DText"
New-Item -Path $key -Force | Out-Null
Set-ItemProperty -Path $key -Name "content_mode" -Value "3"
Set-ItemProperty -Path $key -Name "mesh_path" -Value "$env:TEMP\m3dt_test_cube.obj"
Set-ItemProperty -Path $key -Name "mesh_size_scale" -Value "1.0"

$env:M3DT_SELFTEST = "1"
$env:M3DT_SHOT = "$env:TEMP\m3dt_mesh_cube.png"
$env:M3DT_HOLD_MS = "4000"
$p = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p | Wait-Process -Timeout 10
Write-Output "exists: $(Test-Path $env:TEMP\m3dt_mesh_cube.png)"
```

Usar `Read` no PNG: deve mostrar um cubo extrudado (sem bevel, sem
suavização de silhueta), com o material configurado.

- [ ] **Step 4: Fallback com arquivo inexistente**

```powershell
Set-ItemProperty -Path $key -Name "mesh_path" -Value "$env:TEMP\nao_existe_de_verdade.obj"
$env:M3DT_SELFTEST = "1"
$env:M3DT_HOLD_MS = "4000"
$env:M3DT_SHOT = "$env:TEMP\m3dt_mesh_fallback.png"
$p2 = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p2 | Wait-Process -Timeout 10
Write-Output "exists: $(Test-Path $env:TEMP\m3dt_mesh_fallback.png)"
```

Usar `Read`: deve mostrar o texto de aviso, sem travar o processo.

- [ ] **Step 5: Confirmar que texto/relógio/SVG não regrediram**

```powershell
Set-ItemProperty -Path $key -Name "content_mode" -Value "0"
$env:M3DT_SELFTEST = "1"
$env:M3DT_HOLD_MS = "4000"
$env:M3DT_SHOT = "$env:TEMP\m3dt_text_regression2.png"
$p3 = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p3 | Wait-Process -Timeout 10
Write-Output "exists: $(Test-Path $env:TEMP\m3dt_text_regression2.png)"
```

- [ ] **Step 6: Captura real em tela cheia**

```powershell
Set-ItemProperty -Path $key -Name "content_mode" -Value "3"
Set-ItemProperty -Path $key -Name "mesh_path" -Value "$env:TEMP\m3dt_test_cube.obj"
$env:M3DT_SELFTEST = "0"
$env:M3DT_SHOT = "$env:TEMP\m3dt_mesh_fullres.png"
$env:M3DT_SHOT_T = "2.25"
$p4 = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/s" -PassThru
$p4 | Wait-Process -Timeout 10
Write-Output "exists: $(Test-Path $env:TEMP\m3dt_mesh_fullres.png)"
```

(matar o processo manualmente com `Stop-Process` se `Wait-Process`
estourar o timeout — `/s` real não fecha sozinho sem `M3DT_SELFTEST`.)

- [ ] **Step 7: Restaurar o registro**

```powershell
Remove-Item -Path "HKCU:\Software\Modern3DText" -Recurse -Force -ErrorAction SilentlyContinue
```

- [ ] **Step 8: Build release final + refresh do `.exe`**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile release
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

---

## Self-Review (executado antes de apresentar o plano)

1. **Cobertura do spec/design**: §2 arquitetura (parsers OBJ/STL,
   sem tesselação/extrusão, reaproveitar `s->mesh` único, normais sem
   suavização, normalização, fallback) → Tasks 1 e 3; §3 campos →
   Task 2; §4 interface → Tasks 4-5; §5 testes → Task 1 (unitário) e
   Task 6 (visual); §6 fora de escopo não gera tarefas.
2. **Placeholders**: nenhum "TBD"/depois — todo trecho cola direto no
   arquivo indicado. A vendorização do `fast_obj.h` usa o comando real
   de download, igual seria feito na prática.
3. **Consistência de tipos**: `mesh_import_load()` tem a mesma
   assinatura na declaração (Task 1, `mesh_import.h`) e no uso (Task 3,
   `scene.c`, e no teste unitário, Task 1); reaproveita `MeshData`/
   `MeshVertex` de `contour_mesh.h`/`gl_core.h` sem alterar esses
   arquivos; os IDs de recurso usados em `config_dialog.c` (Task 5) são
   exatamente os declarados em `resource.h` (Task 4); `set_slider()` é
   declarado antecipadamente (Task 5, Step 1) com a MESMA assinatura da
   definição já existente mais abaixo no arquivo.
4. **Risco verificado à mão**: a API do `fast_obj.h` (nomes de
   struct/campo, convenção de índice 1-based com 0=ausente) foi
   conferida contra o arquivo real baixado do repositório oficial, não
   de memória — mesmo cuidado que evitou um erro real na Fase 6b com o
   `nanosvg.h`.
5. **Reaproveitamento confirmado, não assumido**: verifiquei o uso real
   de `vSurf`/`uBevelMode` em `shaders/model.frag` antes de decidir que
   `surf=0` + `sdf_tex=0` bastam pra malha importada renderizar sem
   bevel/AO indevido — e verifiquei o guard `wall_count <= 0` em
   `particles.c` antes de decidir que `wall_cache_count=0` é seguro
   (sparks simplesmente não emitem, sem crash).

---

**Plano completo e salvo em `docs/superpowers/plans/2026-09-13-phase7a-mesh-import.md`.**

Duas opções de execução:

1. **Subagent-Driven (recomendado)** — dispatco um subagente novo por
   task, com revisão entre elas.
2. **Inline Execution** — executo as tasks nesta sessão via
   `executing-plans`, em lote com checkpoints.

Qual prefere?
