# Fase 7b — Materiais do Arquivo OBJ (.mtl) — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Adicionar um toggle `mesh_use_file_materials` que, quando
ligado, extrai a cor `Kd` de cada material de um `.obj` (via `.mtl`) e
renderiza cada grupo de faces como uma peça com sua própria cor — sem
texturas, só cor sólida.

**Architecture:** `mesh_import.c` ganha `mesh_import_load_pieces()`,
que reaproveita o parse do OBJ já feito na Fase 7a mas agrupa por
`face_materials[]` em vez de fundir tudo. `scene.c` ganha um array de
peças específico pra malha (paralelo ao já existente pro SVG, sem
reaproveitar — os dois nunca ficam ativos ao mesmo tempo, mas dar nome
"svg_*" a algo usado pela malha seria confuso). Quando o toggle está
desligado, o `.obj` não tem material real, ou o arquivo é `.stl`, cai
automaticamente no caminho de malha única já existente da Fase 7a, sem
nenhuma mudança nesse caminho.

**Tech Stack:** C11, `fast_obj.h` (já vendorizado na Fase 7a — os dados
de material já são parseados, só não eram usados ainda).

## Global Constraints

- Sem texturas (`map_Kd`) — só a cor `Kd` (difusa) vira a cor da peça.
- `.stl` nunca tem material — o toggle não tem efeito nesse formato.
- Materiais "fallback" do `fast_obj.h` (arquivo sem `.mtl` real) são
  ignorados — se não houver nenhum material real, cai no caminho de
  malha única.
- Build: `mingw32-make -f build/Makefile debug` (ou `release`) da raiz,
  com w64devkit no PATH. Testes: `mingw32-make -f build/Makefile test`.
  Sempre `mingw32-make -f build/Makefile clean` antes de builds depois
  de editar um `.h` ou trocar debug↔release.

---

### Task 1: `mesh_import_load_pieces()` — agrupar por material

**Files:**
- Modify: `src/geometry/mesh_import.h`
- Modify: `src/geometry/mesh_import.c`
- Modify: `build/tests/test_mesh_import.c`

**Interfaces:**
- Produces: `MeshPiece { MeshData data; float r, g, b; }`,
  `MeshPieceSet { MeshPiece *pieces; int count; }`,
  `int mesh_import_load_pieces(const wchar_t *path, float size_scale, MeshPieceSet *out);`,
  `void mesh_import_pieces_free(MeshPieceSet *s);`. Consumido pela
  Task 3 (`scene.c`).

- [ ] **Step 1: Novos tipos em `src/geometry/mesh_import.h`**

Adicionar, antes da declaração de `mesh_import_load`:

```c
typedef struct {
    MeshData data;   /* has_sdf sempre 0, mesmo espirito de mesh_import_load */
    float r, g, b;   /* cor Kd do material desta peca (0..1) */
} MeshPiece;

typedef struct {
    MeshPiece *pieces;
    int        count;
} MeshPieceSet;
```

E, após a declaração de `mesh_import_load`:

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
void mesh_import_pieces_free(MeshPieceSet *s);
```

- [ ] **Step 2: `mesh_import_load_pieces()` em `src/geometry/mesh_import.c`**

Adicionar, logo após `load_obj()`:

```c
typedef struct { MeshVertex *v; int n, cap; unsigned *idx; int ni, icap; } ObjGroupBuf;

static void group_push_tri(ObjGroupBuf *g, MeshVertex a, MeshVertex b, MeshVertex c)
{
    if (g->n + 3 > g->cap) {
        g->cap = g->cap ? g->cap * 2 : 64;
        g->v = (MeshVertex *)realloc(g->v, (size_t)g->cap * sizeof(MeshVertex));
    }
    if (g->ni + 3 > g->icap) {
        g->icap = g->icap ? g->icap * 2 : 64;
        g->idx = (unsigned *)realloc(g->idx, (size_t)g->icap * sizeof(unsigned));
    }
    g->v[g->n] = a; g->idx[g->ni] = (unsigned)g->n; g->n++; g->ni++;
    g->v[g->n] = b; g->idx[g->ni] = (unsigned)g->n; g->n++; g->ni++;
    g->v[g->n] = c; g->idx[g->ni] = (unsigned)g->n; g->n++; g->ni++;
}

static void normalize_pieces(MeshPieceSet *out, float size_scale)
{
    v3 centroid = { 0, 0, 0 };
    int total = 0;
    for (int p = 0; p < out->count; ++p) {
        MeshData *d = &out->pieces[p].data;
        for (int i = 0; i < d->nverts; ++i) {
            centroid = v3_add(centroid, (v3){ d->verts[i].px, d->verts[i].py, d->verts[i].pz });
            total++;
        }
    }
    if (total == 0) return;
    centroid = v3_scale(centroid, 1.0f / (float)total);

    float max_r = 0.0f;
    for (int p = 0; p < out->count; ++p) {
        MeshData *d = &out->pieces[p].data;
        for (int i = 0; i < d->nverts; ++i) {
            v3 pos = v3_sub((v3){ d->verts[i].px, d->verts[i].py, d->verts[i].pz }, centroid);
            float r = v3_len(pos);
            if (r > max_r) max_r = r;
        }
    }
    if (max_r < 1e-6f) max_r = 1.0f;
    float scale = size_scale / max_r;

    for (int p = 0; p < out->count; ++p) {
        MeshData *d = &out->pieces[p].data;
        float minx = 1e30f, miny = 1e30f, minz = 1e30f, maxx = -1e30f, maxy = -1e30f, maxz = -1e30f;
        for (int i = 0; i < d->nverts; ++i) {
            MeshVertex *v = &d->verts[i];
            v->px = (v->px - centroid.x) * scale;
            v->py = (v->py - centroid.y) * scale;
            v->pz = (v->pz - centroid.z) * scale;
            if (v->px < minx) minx = v->px;
            if (v->px > maxx) maxx = v->px;
            if (v->py < miny) miny = v->py;
            if (v->py > maxy) maxy = v->py;
            if (v->pz < minz) minz = v->pz;
            if (v->pz > maxz) maxz = v->pz;
        }
        d->minx = minx; d->miny = miny; d->minz = minz;
        d->maxx = maxx; d->maxy = maxy; d->maxz = maxz;
    }
}

int mesh_import_load_pieces(const wchar_t *path, float size_scale, MeshPieceSet *out)
{
    memset(out, 0, sizeof *out);

    char u8[1024];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, u8, (int)sizeof u8, NULL, NULL);

    fastObjMesh *m = fast_obj_read(u8);
    if (!m) return 0;

    int has_real_material = 0;
    for (unsigned int i = 0; i < m->material_count; ++i)
        if (!m->materials[i].fallback) { has_real_material = 1; break; }
    if (!has_real_material) { fast_obj_destroy(m); return 0; }

    ObjGroupBuf *groups = (ObjGroupBuf *)calloc(m->material_count, sizeof(ObjGroupBuf));
    if (!groups) { fast_obj_destroy(m); return 0; }

    unsigned int cursor = 0;
    for (unsigned int f = 0; f < m->face_count; ++f) {
        unsigned int fv = m->face_vertices[f];
        if (fv < 3) { cursor += fv; continue; }
        unsigned int matidx = m->face_materials[f];
        if (matidx >= m->material_count) { cursor += fv; continue; }

        fastObjIndex face_idx[64];
        unsigned int use = fv > 64 ? 64 : fv;
        for (unsigned int k = 0; k < use; ++k) face_idx[k] = m->indices[cursor + k];

        v3 p0 = { m->positions[face_idx[0].p*3+0], m->positions[face_idx[0].p*3+1], m->positions[face_idx[0].p*3+2] };
        v3 p1 = { m->positions[face_idx[1].p*3+0], m->positions[face_idx[1].p*3+1], m->positions[face_idx[1].p*3+2] };
        v3 p2 = { m->positions[face_idx[2].p*3+0], m->positions[face_idx[2].p*3+1], m->positions[face_idx[2].p*3+2] };
        v3 fn = v3_norm(v3_cross(v3_sub(p1, p0), v3_sub(p2, p0)));

        for (unsigned int k = 1; k + 1 < use; ++k) {
            unsigned int tri[3] = { 0, k, k + 1 };
            MeshVertex tv[3];
            for (int c = 0; c < 3; ++c) {
                fastObjIndex fi = face_idx[tri[c]];
                v3 pos = { m->positions[fi.p*3+0], m->positions[fi.p*3+1], m->positions[fi.p*3+2] };
                v3 nrm = fn;
                if (fi.n != 0 && m->normal_count > fi.n) {
                    v3 fnrm = { m->normals[fi.n*3+0], m->normals[fi.n*3+1], m->normals[fi.n*3+2] };
                    if (v3_len(fnrm) > 1e-6f) nrm = fnrm;
                }
                tv[c] = (MeshVertex){ pos.x, pos.y, pos.z, nrm.x, nrm.y, nrm.z, 0.0f };
            }
            group_push_tri(&groups[matidx], tv[0], tv[1], tv[2]);
        }
        cursor += fv;
    }

    int npieces = 0;
    for (unsigned int i = 0; i < m->material_count; ++i)
        if (groups[i].n > 0) npieces++;

    if (npieces == 0) {
        for (unsigned int i = 0; i < m->material_count; ++i) { free(groups[i].v); free(groups[i].idx); }
        free(groups);
        fast_obj_destroy(m);
        return 0;
    }

    MeshPiece *pieces = (MeshPiece *)calloc((size_t)npieces, sizeof(MeshPiece));
    if (!pieces) {
        for (unsigned int i = 0; i < m->material_count; ++i) { free(groups[i].v); free(groups[i].idx); }
        free(groups);
        fast_obj_destroy(m);
        return 0;
    }

    int pi = 0;
    for (unsigned int i = 0; i < m->material_count; ++i) {
        if (groups[i].n == 0) { free(groups[i].v); free(groups[i].idx); continue; }
        pieces[pi].data.verts = groups[i].v;
        pieces[pi].data.nverts = groups[i].n;
        pieces[pi].data.idx = groups[i].idx;
        pieces[pi].data.nidx = groups[i].ni;
        pieces[pi].data.has_sdf = 0;
        memset(&pieces[pi].data.sdf, 0, sizeof pieces[pi].data.sdf);
        pieces[pi].r = m->materials[i].Kd[0];
        pieces[pi].g = m->materials[i].Kd[1];
        pieces[pi].b = m->materials[i].Kd[2];
        pi++;
    }
    free(groups);
    fast_obj_destroy(m);

    out->pieces = pieces;
    out->count = npieces;
    normalize_pieces(out, size_scale);
    return 1;
}

void mesh_import_pieces_free(MeshPieceSet *s)
{
    if (!s) return;
    for (int i = 0; i < s->count; ++i) {
        free(s->pieces[i].data.verts);
        free(s->pieces[i].data.idx);
    }
    free(s->pieces);
    memset(s, 0, sizeof *s);
}
```

- [ ] **Step 3: Testes em `build/tests/test_mesh_import.c`**

O arquivo ainda não inclui `<stdio.h>` (precisa de `snprintf` pro novo
helper abaixo). Trocar:

```c
#include <windows.h>
#include <string.h>
#include <math.h>
```

por:

```c
#include <windows.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
```

Adicionar, no topo do arquivo (junto dos outros helpers de escrita de
arquivo temporário):

```c
static void write_obj_with_mtl(const char *obj_body_fmt, const char *mtl_content,
                               wchar_t *out_obj_path, wchar_t *out_mtl_path)
{
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"tmp", 0, out_obj_path);
    size_t n = wcslen(out_obj_path);
    wcsncpy(out_obj_path + n - 3, L"obj", 3);

    wcscpy(out_mtl_path, out_obj_path);
    wcsncpy(out_mtl_path + n - 3, L"mtl", 3);

    wchar_t *base = wcsrchr(out_mtl_path, L'\\');
    char mtl_name[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, base ? base + 1 : out_mtl_path, -1, mtl_name, MAX_PATH, NULL, NULL);

    char obj_content[2048];
    snprintf(obj_content, sizeof obj_content, obj_body_fmt, mtl_name);

    HANDLE h1 = CreateFileW(out_mtl_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written = 0;
    WriteFile(h1, mtl_content, (DWORD)strlen(mtl_content), &written, NULL);
    CloseHandle(h1);

    HANDLE h2 = CreateFileW(out_obj_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h2, obj_content, (DWORD)strlen(obj_content), &written, NULL);
    CloseHandle(h2);
}
```

E, ao final de `run_mesh_import_tests(void)` (antes do `}` que fecha a
função):

```c
    /* .obj com 2 materiais reais (.mtl de verdade) -> 2 pecas com cores distintas */
    {
        const char *mtl = "newmtl vermelho\nKd 1.0 0.0 0.0\nnewmtl azul\nKd 0.0 0.0 1.0\n";
        const char *obj_fmt =
            "mtllib %s\n"
            "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
            "v 2 0 0\nv 3 0 0\nv 2 1 0\n"
            "usemtl vermelho\nf 1 2 3\n"
            "usemtl azul\nf 4 5 6\n";
        wchar_t obj_path[MAX_PATH], mtl_path[MAX_PATH];
        write_obj_with_mtl(obj_fmt, mtl, obj_path, mtl_path);

        MeshPieceSet ps;
        int ok = mesh_import_load_pieces(obj_path, 1.0f, &ps);
        DeleteFileW(obj_path);
        DeleteFileW(mtl_path);

        EXPECT(ok);
        EXPECT(ps.count == 2);
        if (ps.count == 2) {
            /* ordem de declaracao no .mtl: vermelho (indice 0) primeiro */
            EXPECT(ps.pieces[0].r > 0.9f && ps.pieces[0].g < 0.1f && ps.pieces[0].b < 0.1f);
            EXPECT(ps.pieces[1].r < 0.1f && ps.pieces[1].g < 0.1f && ps.pieces[1].b > 0.9f);
        }
        mesh_import_pieces_free(&ps);
    }

    /* .obj sem mtllib/usemtl -> nenhum material real -> falha graciosa */
    {
        const char *obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
        wchar_t path[MAX_PATH];
        write_text_file(obj, L"obj", path);

        MeshPieceSet ps;
        int ok = mesh_import_load_pieces(path, 1.0f, &ps);
        DeleteFileW(path);

        EXPECT(!ok);
    }
```

- [ ] **Step 4: Rodar os testes**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```

Esperado: `all tests passed`, incluindo os 2 novos casos.

- [ ] **Step 5: Commit**

```bash
git add src/geometry/mesh_import.h src/geometry/mesh_import.c build/tests/test_mesh_import.c
git commit -m "feat: mesh_import_load_pieces() - group OBJ faces by .mtl material"
```

---

### Task 2: Config schema — `mesh_use_file_materials`

**Files:**
- Modify: `src/config.h`
- Modify: `src/config.c`
- Modify: `build/tests/test_config.c`

**Interfaces:**
- Produces: `Config.mesh_use_file_materials` (`int`, 0/1). Consumido
  pela Task 3 (`scene.c`) e Task 5 (`config_dialog.c`).

- [ ] **Step 1: Novo campo em `src/config.h`**

Logo após `float mesh_size_scale;`:

```c
    int          mesh_use_file_materials; /* 0/1, so' tem efeito com .obj com material real */
```

- [ ] **Step 2: Default em `src/config.c`**

Em `config_defaults`, logo após `c->mesh_size_scale = 1.0f;`:

```c
    c->mesh_use_file_materials = 0;
```

- [ ] **Step 3: Carregar em `config_load_from`**

Logo após `if (reg_get_f(k, L"mesh_size_scale", &f)) c->mesh_size_scale = f;`:

```c
    reg_get_i(k, L"mesh_use_file_materials", &c->mesh_use_file_materials);
```

No bloco de saneamento, logo após `c->mesh_size_scale = clampf(c->mesh_size_scale, 0.0f, 2.0f);`:

```c
    c->mesh_use_file_materials = c->mesh_use_file_materials ? 1 : 0;
```

- [ ] **Step 4: Salvar em `config_save_to`**

Logo após `set_f(k, L"mesh_size_scale", c->mesh_size_scale);`:

```c
    set_f(k, L"mesh_use_file_materials", (float)c->mesh_use_file_materials);
```

- [ ] **Step 5: Estender `build/tests/test_config.c`**

No bloco de defaults:

```c
    EXPECT(d.mesh_use_file_materials == 0);
```

No round-trip dedicado da malha (o par `ma`/`mb` da Fase 7a):

```c
    ma.mesh_use_file_materials = 1;
```

(logo após `ma.mesh_size_scale = 1.5f;`, antes de `config_save_to(&ma, TESTKEY);`)

```c
    EXPECT(mb.mesh_use_file_materials == 1);
```

(logo após `EXPECT(nearf(mb.mesh_size_scale, 1.5f));`)

No bloco de clamp (`kv[]`), adicionar mais uma entrada e ajustar o
contador do loop (de 26 para 27):

```c
            { L"mesh_size_scale", L"9" },
            { L"mesh_use_file_materials", L"5" },
        };
        for (int i = 0; i < 27; ++i)
```

E na verificação final:

```c
    EXPECT(e.mesh_use_file_materials == 1);   /* 5 -> !=0 -> 1 */
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
git commit -m "feat: add mesh_use_file_materials config field"
```

---

### Task 3: Integração em `scene.c`

**Files:**
- Modify: `src/scene.c`

**Interfaces:**
- Consumes: `mesh_import_load_pieces()`/`mesh_import_pieces_free()`
  (Task 1); `Config.mesh_use_file_materials` (Task 2).

- [ ] **Step 1: Novos campos em `SceneRenderer`**

Logo após `float mesh_size_scale;` (antes do `};` que fecha o struct):

```c
    int      mesh_use_file_materials;
    GlMesh  *mesh_pieces;
    v3      *mesh_piece_colors;
    int      mesh_piece_count;
    int      have_mesh_content;   /* "ja tentamos montar a malha atual pelo menos uma vez" -
                                      cobre tanto o caminho unico quanto o de pecas */
```

- [ ] **Step 2: `free_mesh_pieces` + reescrever `rebuild_imported_mesh`**

Trocar:

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

por:

```c
static void free_mesh_pieces(SceneRenderer *s)
{
    for (int i = 0; i < s->mesh_piece_count; ++i) gl_mesh_free(&s->mesh_pieces[i]);
    free(s->mesh_pieces); s->mesh_pieces = NULL;
    free(s->mesh_piece_colors); s->mesh_piece_colors = NULL;
    s->mesh_piece_count = 0;
}

static int rebuild_imported_mesh(SceneRenderer *s)
{
    free_mesh_pieces(s);
    s->have_mesh_content = 1;

    if (s->mesh_path[0] != 0 && s->mesh_use_file_materials) {
        MeshPieceSet ps;
        if (mesh_import_load_pieces(s->mesh_path, s->mesh_size_scale, &ps)) {
            s->mesh_pieces = (GlMesh *)calloc((size_t)ps.count, sizeof(GlMesh));
            s->mesh_piece_colors = (v3 *)calloc((size_t)ps.count, sizeof(v3));
            if (s->mesh_pieces && s->mesh_piece_colors) {
                float uhx = 0.0f, uhy = 0.0f, uhz = 0.0f;
                for (int i = 0; i < ps.count; ++i) {
                    MeshData *d = &ps.pieces[i].data;
                    s->mesh_pieces[i] = gl_mesh_upload(d->verts, d->nverts, d->idx, d->nidx);
                    s->mesh_piece_colors[i] = (v3){ ps.pieces[i].r, ps.pieces[i].g, ps.pieces[i].b };
                    float phx = 0.5f * (d->maxx - d->minx);
                    float phy = 0.5f * (d->maxy - d->miny);
                    float phz = 0.5f * (d->maxz - d->minz);
                    if (phx > uhx) uhx = phx;
                    if (phy > uhy) uhy = phy;
                    if (phz > uhz) uhz = phz;
                }
                s->mesh_piece_count = ps.count;
                if (uhx < 1e-3f) uhx = 1.0f;
                if (uhy < 1e-3f) uhy = 1.0f;
                s->hx = uhx; s->hy = uhy; s->hz = uhz;
                s->wall_cache_count = 0;
                upload_sdf(s, NULL);
                log_infof("scene: malha '%ls' -> %d peca(s) com material do arquivo",
                          s->mesh_path, s->mesh_piece_count);
                mesh_import_pieces_free(&ps);
                return 1;
            }
            free_mesh_pieces(s);
            mesh_import_pieces_free(&ps);
            return 0;
        }
    }

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

- [ ] **Step 3: `scene_set_config` — comparação e cópia**

Trocar:

```c
    int mesh_relevant_change = (cfg->content_mode == CONTENT_MESH)
        && (wcscmp(s->mesh_path, cfg->mesh_path) != 0 || s->mesh_size_scale != cfg->mesh_size_scale);

    int mesh_dirty = (cfg->content_mode == CONTENT_SVG ? !s->have_svg_mesh : !s->have_mesh)
```

por:

```c
    int mesh_relevant_change = (cfg->content_mode == CONTENT_MESH)
        && (wcscmp(s->mesh_path, cfg->mesh_path) != 0
            || s->mesh_size_scale != cfg->mesh_size_scale
            || s->mesh_use_file_materials != cfg->mesh_use_file_materials);

    int mesh_dirty = (cfg->content_mode == CONTENT_SVG ? !s->have_svg_mesh
                      : cfg->content_mode == CONTENT_MESH ? !s->have_mesh_content
                      : !s->have_mesh)
```

Logo após `s->mesh_size_scale = cfg->mesh_size_scale;`:

```c
    s->mesh_use_file_materials = cfg->mesh_use_file_materials;
```

- [ ] **Step 4: `scene_create` e `scene_render` — gates de 3 vias**

Em `scene_create`, trocar:

```c
    if (!s->have_mesh && s->svg_mesh_count == 0) {
```

por:

```c
    if (!s->have_mesh && s->svg_mesh_count == 0 && s->mesh_piece_count == 0) {
```

Em `scene_render`, trocar:

```c
    if (!s->have_mesh && s->svg_mesh_count == 0) return;
```

por:

```c
    if (!s->have_mesh && s->svg_mesh_count == 0 && s->mesh_piece_count == 0) return;
```

- [ ] **Step 5: `scene_render` — laço de desenho de 3 vias**

Trocar:

```c
    if (s->content_mode == CONTENT_SVG && s->svg_mesh_count > 0) {
        for (int i = 0; i < s->svg_mesh_count; ++i) {
            v3 piece_color = (s->svg_color_mode == 0) ? s->svg_colors[i] : s->base_color;
            material_set_piece_color(&s->mat, piece_color);
            gl_mesh_draw(&s->svg_meshes[i]);
        }
    } else {
        gl_mesh_draw(&s->mesh);
    }
```

por:

```c
    if (s->content_mode == CONTENT_SVG && s->svg_mesh_count > 0) {
        for (int i = 0; i < s->svg_mesh_count; ++i) {
            v3 piece_color = (s->svg_color_mode == 0) ? s->svg_colors[i] : s->base_color;
            material_set_piece_color(&s->mat, piece_color);
            gl_mesh_draw(&s->svg_meshes[i]);
        }
    } else if (s->content_mode == CONTENT_MESH && s->mesh_piece_count > 0) {
        for (int i = 0; i < s->mesh_piece_count; ++i) {
            material_set_piece_color(&s->mat, s->mesh_piece_colors[i]);
            gl_mesh_draw(&s->mesh_pieces[i]);
        }
    } else {
        gl_mesh_draw(&s->mesh);
    }
```

- [ ] **Step 6: `scene_destroy` — liberar as peças de malha**

Logo após `free_svg_pieces(s);`:

```c
    free_mesh_pieces(s);
```

- [ ] **Step 7: Build de depuração completo**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile debug
```

Esperado: `Built dist/Modern3DText.scr (N bytes)`, sem erros nem
warnings novos.

- [ ] **Step 8: Commit**

```bash
git add src/scene.c
git commit -m "feat: render OBJ file materials as multiple pieces in scene.c"
```

---

### Task 4: ID de recurso + layout — checkbox "Usar materiais do arquivo"

**Files:**
- Modify: `src/resource.h`
- Modify: `res/screensaver.rc`

**Interfaces:**
- Produces: `IDC_MESHUSEMAT` (1123). Consumido pela Task 5.

- [ ] **Step 1: Novo ID em `src/resource.h`**

Logo após `#define IDC_MESHSCALE      1122`:

```c
#define IDC_MESHUSEMAT     1123
```

- [ ] **Step 2: Layout de `IDD_TAB_CONTENT` em `res/screensaver.rc`**

Logo após a linha do `CONTROL "" IDC_MESHSCALE ...` (trackbar de
escala), antes do bloco `AUTOCHECKBOX "Negrito"...`:

```rc
    AUTOCHECKBOX     "Usar materiais do arquivo", IDC_MESHUSEMAT, 8, 122, 180, 12
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
git commit -m "feat: add 'usar materiais do arquivo' checkbox to Conteudo tab layout"
```

---

### Task 5: Wiring em `config_dialog.c`

**Files:**
- Modify: `src/config_dialog.c`

**Interfaces:**
- Consumes: `IDC_MESHUSEMAT` (Task 4); `Config.mesh_use_file_materials`
  (Task 2).

- [ ] **Step 1: `content_enable()` — mostrar/esconder o checkbox**

Logo após `ShowWindow(GetDlgItem(h, IDC_MESHSCALE), is_mesh ? SW_SHOW : SW_HIDE);`:

```c
    ShowWindow(GetDlgItem(h, IDC_MESHUSEMAT), is_mesh ? SW_SHOW : SW_HIDE);
```

- [ ] **Step 2: `WM_INITDIALOG` — estado inicial do checkbox**

Logo após `content_mesh_label(h);` (a chamada que já existe, antes de
`content_enable(h);`):

```c
            CheckDlgButton(h, IDC_MESHUSEMAT, g_work.mesh_use_file_materials ? BST_CHECKED : BST_UNCHECKED);
```

- [ ] **Step 3: `WM_COMMAND` — handler do checkbox**

Logo após o `case IDC_MESHCLEAR:` já existente:

```c
                case IDC_MESHUSEMAT:
                    g_work.mesh_use_file_materials =
                        (IsDlgButtonChecked(h, IDC_MESHUSEMAT) == BST_CHECKED);
                    preview_dirty(h);
                    break;
```

- [ ] **Step 4: Build de depuração + checagem de abertura da aba**

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

- [ ] **Step 5: Commit**

```bash
git add src/config_dialog.c
git commit -m "feat: wire 'usar materiais do arquivo' checkbox into Conteudo tab"
```

---

### Task 6: Verificação final

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

- [ ] **Step 2: Criar um `.obj`+`.mtl` de exemplo multi-material**

```powershell
$mtl = @'
newmtl vermelho
Kd 0.9 0.2 0.2
newmtl azul
Kd 0.2 0.3 0.9
'@
Set-Content -Path "$env:TEMP\m3dt_test_multimat.mtl" -Value $mtl -Encoding ASCII

$obj = @'
mtllib m3dt_test_multimat.mtl
v -1 -1 -1
v 1 -1 -1
v 1 1 -1
v -1 1 -1
v -1 -1 1
v 1 -1 1
v 1 1 1
v -1 1 1
usemtl vermelho
f 1 2 3 4
f 5 8 7 6
usemtl azul
f 1 5 6 2
f 2 6 7 3
f 3 7 8 4
f 4 8 5 1
'@
Set-Content -Path "$env:TEMP\m3dt_test_multimat.obj" -Value $obj -Encoding ASCII
```

(cubo com a tampa/base vermelhas e as 4 laterais azuis - prova visual
clara de que o agrupamento por material está funcionando.)

- [ ] **Step 3: Capturar com o toggle ligado**

```powershell
$key = "HKCU:\Software\Modern3DText"
New-Item -Path $key -Force | Out-Null
Set-ItemProperty -Path $key -Name "content_mode" -Value "3"
Set-ItemProperty -Path $key -Name "mesh_path" -Value "$env:TEMP\m3dt_test_multimat.obj"
Set-ItemProperty -Path $key -Name "mesh_size_scale" -Value "1.0"
Set-ItemProperty -Path $key -Name "mesh_use_file_materials" -Value "1"

$env:M3DT_SELFTEST = "1"
$env:M3DT_SHOT = "$env:TEMP\m3dt_mesh_material_on.png"
$env:M3DT_HOLD_MS = "4000"
$p = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p | Wait-Process -Timeout 10
Write-Output "exists: $(Test-Path $env:TEMP\m3dt_mesh_material_on.png)"
```

Usar `Read`: deve mostrar o cubo com tampa/base vermelhas e laterais
azuis.

- [ ] **Step 4: Capturar com o toggle desligado**

```powershell
Set-ItemProperty -Path $key -Name "mesh_use_file_materials" -Value "0"
$env:M3DT_SELFTEST = "1"
$env:M3DT_HOLD_MS = "4000"
$env:M3DT_SHOT = "$env:TEMP\m3dt_mesh_material_off.png"
$p2 = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p2 | Wait-Process -Timeout 10
Write-Output "exists: $(Test-Path $env:TEMP\m3dt_mesh_material_off.png)"
```

Usar `Read`: deve mostrar o cubo inteiro na cor do material único
configurado (sem vermelho/azul).

- [ ] **Step 5: Confirmar que a Fase 7a (sem `.mtl`) não regrediu**

```powershell
Set-ItemProperty -Path $key -Name "mesh_use_file_materials" -Value "1"
Set-ItemProperty -Path $key -Name "mesh_path" -Value "$env:TEMP\m3dt_test_cube.obj"
$env:M3DT_SELFTEST = "1"
$env:M3DT_HOLD_MS = "4000"
$env:M3DT_SHOT = "$env:TEMP\m3dt_mesh_nomaterial_regression.png"
$p3 = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p3 | Wait-Process -Timeout 10
Write-Output "exists: $(Test-Path $env:TEMP\m3dt_mesh_nomaterial_regression.png)"
```

(reaproveita o cubo sem material da Fase 7a, `m3dt_test_cube.obj`, se
ainda existir em `%TEMP%`; recriar com o mesmo conteúdo do plano da
7a se necessário.) Usar `Read`: mesmo resultado da Fase 7a - toggle
ligado não muda nada quando não há material real no arquivo.

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

1. **Cobertura do spec/design**: §2 arquitetura (agrupar por material,
   ignorar fallback, `.stl` sem efeito, cair no caminho 7a quando não
   aplicável) → Tasks 1 e 3; §3 campos → Task 2; §4 interface → Tasks
   4-5; §5 testes → Task 1 (unitário) e Task 6 (visual); §6 fora de
   escopo não gera tarefas.
2. **Placeholders**: nenhum "TBD"/depois — todo trecho cola direto no
   arquivo indicado.
3. **Consistência de tipos**: `MeshPieceSet`/`MeshPiece` usados
   identicamente em `mesh_import.h` (Task 1) e em `scene.c` (Task 3);
   `mesh_import_load_pieces()`/`mesh_import_pieces_free()` têm a mesma
   assinatura na declaração e no uso; os campos novos de
   `SceneRenderer` (Task 3, Step 1) são exatamente os usados no resto
   da Task 3; o ID `IDC_MESHUSEMAT` usado em `config_dialog.c` (Task 5)
   é exatamente o declarado em `resource.h` (Task 4).
4. **Risco verificado à mão**: conferi `mtl_default()` no
   `fast_obj.h` vendorizado (não de memória) pra confirmar que
   materiais "fallback" recebem `Kd = (1,1,1)` branco e são marcados
   com `fallback = 1` — esse é o sinal exato usado pra decidir quando
   `mesh_import_load_pieces()` deve falhar e delegar pro caminho de
   malha única já existente.
5. **Regressão explicitamente coberta**: Task 6 Step 5 reconfirma que
   um `.obj` sem material real (o mesmo arquivo de teste da Fase 7a)
   continua se comportando exatamente como antes mesmo com o toggle
   ligado - a decisão de "cair no caminho normal" é neutra por
   construção (não pode piorar o comportamento da 7a).

---

**Plano completo e salvo em `docs/superpowers/plans/2026-09-13-phase7b-mesh-materials.md`.**

Duas opções de execução:

1. **Subagent-Driven (recomendado)** — dispatco um subagente novo por
   task, com revisão entre elas.
2. **Inline Execution** — executo as tasks nesta sessão via
   `executing-plans`, em lote com checkpoints.

Qual prefere?
