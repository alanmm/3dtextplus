# Fase 7d — Limite de Tamanho + Placa de Erro 2D — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Dois ajustes de robustez/UX surgidos do uso real da Fase 7c: (1)
recusar arquivos de malha acima de 10MB antes de tentar carregá-los, e (2)
trocar a mensagem de erro (SVG/malha ausente ou inválida) de texto 3D
extrudado ilegível por uma "placa" achatada (caixa cinza + texto preto),
posicionada e enquadrada como qualquer conteúdo 3D normal.

**Architecture:** Camada de checagem de tamanho em `mesh_import.c` (usada
tanto pelo carregamento quanto pelo seletor de arquivo), e uma nova função
`build_error_plaque()` em `scene.c` que reaproveita o pipeline de texto já
existente (`font_build_contours`/`contour_mesh_build` com `depth=0` e bevel
desligado) mais um quad achatado desenhado à mão, ambos renderizados com o
esquema de "peças coloridas" já usado 3x no projeto.

**Tech Stack:** C11, WinAPI (`GetFileAttributesExW`, `MessageBoxW`), cgltf
(já vendorizado), pipeline de texto/malha já existente.

## Global Constraints

- Sem novo campo de `Config` — nenhuma das duas partes desta fase precisa
  de persistência.
- Sem novo controle de UI além do `MessageBoxW` de aviso.
- Build: `mingw32-make -f build/Makefile debug` (ou `release`) da raiz, com
  w64devkit no PATH. Testes: `mingw32-make -f build/Makefile test`. Sempre
  `mingw32-make -f build/Makefile clean` antes de builds depois de editar
  um `.h` ou trocar debug↔release.

---

### Task 1: Limite de tamanho de arquivo em `mesh_import.c`/`.h`

**Files:**
- Modify: `src/geometry/mesh_import.h`
- Modify: `src/geometry/mesh_import.c`
- Modify: `build/tests/test_mesh_import.c`

**Interfaces:**
- Produces: `#define MESH_IMPORT_MAX_BYTES` e
  `int mesh_import_file_too_big(const wchar_t *path)`, exportados de
  `mesh_import.h` — consumidos pela Task 2 (`config_dialog.c`).

- [ ] **Step 1: Constante + função exportada em `mesh_import.h`**

Adicionar logo após os includes, antes do comentário de
`mesh_import_load`:

```c
/* Tamanho maximo (em bytes) de um arquivo de malha aceito por
   mesh_import_load()/mesh_import_load_pieces() - acima disso, ambas
   retornam 0 sem tentar parsear. Existe pra evitar que um arquivo gigante
   (por engano, ex.: um .obj de centenas de MB) trave a thread de preview
   ou a tela cheia por um bom tempo. Para .gltf com buffer(s) externo(s), a
   soma dos tamanhos DECLARADOS no JSON tambem e' checada contra este mesmo
   limite, antes mesmo de tentar carregar o(s) arquivo(s) externo(s). */
#define MESH_IMPORT_MAX_BYTES ((unsigned long long)10 * 1024 * 1024)

/* Retorna 1 se o arquivo em 'path' excede MESH_IMPORT_MAX_BYTES (0 se nao
   existir ou estiver dentro do limite - nesse ultimo caso quem for
   realmente abrir o arquivo reporta o erro certo). Usado pelo dialogo de
   configuracao pra recusar a escolha imediatamente, antes mesmo de chamar
   mesh_import_load()/mesh_import_load_pieces(). */
int mesh_import_file_too_big(const wchar_t *path);
```

Também atualizar o comentário de `mesh_import_load` (trocar "Retorna 1 em
sucesso; 0 se o arquivo nao existe, a extensao e desconhecida, ou nao ha
nenhum triangulo valido." por "Retorna 1 em sucesso; 0 se o arquivo nao
existe, excede MESH_IMPORT_MAX_BYTES, a extensao e desconhecida, ou nao ha
nenhum triangulo valido.") e o de `mesh_import_load_pieces` de forma
equivalente (acrescentar "ou excede MESH_IMPORT_MAX_BYTES" à lista de
casos que retornam 0).

- [ ] **Step 2: Implementar a checagem em `mesh_import.c`**

Adicionar logo após `has_ext()` (antes das declarações forward de
`load_gltf`/`load_gltf_pieces`):

```c
static int mesh_file_too_big(const wchar_t *path, unsigned long long max_bytes)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad)) return 0;
    unsigned long long size = ((unsigned long long)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    return size > max_bytes;
}

int mesh_import_file_too_big(const wchar_t *path)
{
    if (!path || !path[0]) return 0;
    return mesh_file_too_big(path, MESH_IMPORT_MAX_BYTES);
}
```

- [ ] **Step 3: Checar no início de `mesh_import_load`**

Trocar:

```c
int mesh_import_load(const wchar_t *path, float size_scale, MeshData *out)
{
    memset(out, 0, sizeof *out);
    if (!path || !path[0]) return 0;

    int ok;
```

por:

```c
int mesh_import_load(const wchar_t *path, float size_scale, MeshData *out)
{
    memset(out, 0, sizeof *out);
    if (!path || !path[0]) return 0;
    if (mesh_file_too_big(path, MESH_IMPORT_MAX_BYTES)) {
        log_errorf("mesh_import: arquivo excede o limite de %.0f MB",
                   MESH_IMPORT_MAX_BYTES / (1024.0 * 1024.0));
        return 0;
    }

    int ok;
```

- [ ] **Step 4: Checar no início de `mesh_import_load_pieces`**

Trocar:

```c
int mesh_import_load_pieces(const wchar_t *path, float size_scale, MeshPieceSet *out)
{
    memset(out, 0, sizeof *out);

    if (has_ext(path, L".glb") || has_ext(path, L".gltf"))
        return load_gltf_pieces(path, size_scale, out);
```

por:

```c
int mesh_import_load_pieces(const wchar_t *path, float size_scale, MeshPieceSet *out)
{
    memset(out, 0, sizeof *out);

    if (mesh_file_too_big(path, MESH_IMPORT_MAX_BYTES)) {
        log_errorf("mesh_import: arquivo excede o limite de %.0f MB",
                   MESH_IMPORT_MAX_BYTES / (1024.0 * 1024.0));
        return 0;
    }

    if (has_ext(path, L".glb") || has_ext(path, L".gltf"))
        return load_gltf_pieces(path, size_scale, out);
```

- [ ] **Step 5: Checagem extra pra buffers externos de `.gltf`**

Em `gltf_collect_chunks`, trocar:

```c
    cgltf_options options;
    memset(&options, 0, sizeof options);
    cgltf_data *data = NULL;
    if (cgltf_parse_file(&options, u8, &data) != cgltf_result_success) return 0;
    if (cgltf_load_buffers(&options, data, u8) != cgltf_result_success) {
        cgltf_free(data);
        return 0;
    }
```

por:

```c
    cgltf_options options;
    memset(&options, 0, sizeof options);
    cgltf_data *data = NULL;
    if (cgltf_parse_file(&options, u8, &data) != cgltf_result_success) return 0;

    unsigned long long total_buf_bytes = 0;
    for (cgltf_size i = 0; i < data->buffers_count; ++i)
        total_buf_bytes += data->buffers[i].size;
    if (total_buf_bytes > MESH_IMPORT_MAX_BYTES) {
        log_errorf("mesh_import: buffers do .gltf/.glb excedem o limite de %.0f MB",
                   MESH_IMPORT_MAX_BYTES / (1024.0 * 1024.0));
        cgltf_free(data);
        return 0;
    }

    if (cgltf_load_buffers(&options, data, u8) != cgltf_result_success) {
        cgltf_free(data);
        return 0;
    }
```

- [ ] **Step 6: Testes em `build/tests/test_mesh_import.c`**

Adicionar, logo após `write_glb` (novo helper pra arquivo grande de
"lixo" e outro pra escrever um `.gltf` com extensão de 4 letras — os
helpers existentes trocam só os últimos 3 caracteres do `.tmp` que o
`GetTempFileNameW` gera, o que não serve pra `.gltf`):

```c
static void write_big_dummy_file(const wchar_t *ext, size_t total_bytes, wchar_t *out_path)
{
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"tmp", 0, out_path);
    size_t n = wcslen(out_path);
    wcsncpy(out_path + n - 3, ext, 3);

    char *buf = (char *)malloc(total_bytes);
    memset(buf, 'x', total_bytes);

    HANDLE h = CreateFileW(out_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written = 0;
    WriteFile(h, buf, (DWORD)total_bytes, &written, NULL);
    CloseHandle(h);
    free(buf);
}

static void write_gltf_json(const char *json, wchar_t *out_path)
{
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"tmp", 0, out_path);
    /* GetTempFileNameW da extensao .tmp (3 letras) - "gltf" tem 4, entao
       anexa em vez de tentar substituir no lugar como os outros helpers */
    wcscat(out_path, L".gltf");

    HANDLE h = CreateFileW(out_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written = 0;
    WriteFile(h, json, (DWORD)strlen(json), &written, NULL);
    CloseHandle(h);
}
```

E, ao final de `run_mesh_import_tests(void)`:

```c
    /* .obj acima de 10MB -> recusado antes de tentar parsear */
    {
        wchar_t path[MAX_PATH];
        write_big_dummy_file(L"obj", (size_t)11 * 1024 * 1024, path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(!ok);
    }

    /* .glb acima de 10MB -> recusado antes de tentar parsear */
    {
        wchar_t path[MAX_PATH];
        write_big_dummy_file(L"glb", (size_t)11 * 1024 * 1024, path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(!ok);
    }

    /* .gltf pequeno no disco, mas com byteLength declarado acima do limite
       -> recusado sem tentar resolver o buffer (que nem existe de verdade) */
    {
        const char *json =
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"byteLength\":20000000}],"
            "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
            "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
                            "\"min\":[0.0,0.0,0.0],\"max\":[1.0,1.0,0.0]}],"
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"mode\":4}]}],"
            "\"nodes\":[{\"mesh\":0}],"
            "\"scenes\":[{\"nodes\":[0]}],"
            "\"scene\":0}";
        wchar_t path[MAX_PATH];
        write_gltf_json(json, path);

        MeshData md;
        int ok = mesh_import_load(path, 1.0f, &md);
        DeleteFileW(path);

        EXPECT(!ok);
    }

    /* mesh_import_file_too_big: arquivo pequeno normal -> aceito (0) */
    {
        const char *obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
        wchar_t path[MAX_PATH];
        write_text_file(obj, L"obj", path);

        int too_big = mesh_import_file_too_big(path);
        DeleteFileW(path);

        EXPECT(!too_big);
    }

    /* mesh_import_file_too_big: arquivo grande -> recusado (1) */
    {
        wchar_t path[MAX_PATH];
        write_big_dummy_file(L"obj", (size_t)11 * 1024 * 1024, path);

        int too_big = mesh_import_file_too_big(path);
        DeleteFileW(path);

        EXPECT(too_big);
    }
```

- [ ] **Step 7: Build + testes**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```

Esperado: `all tests passed`, incluindo os 5 novos casos.

- [ ] **Step 8: Commit**

```bash
git add src/geometry/mesh_import.h src/geometry/mesh_import.c build/tests/test_mesh_import.c
git commit -m "feat: reject mesh files over 10MB before parsing"
```

---

### Task 2: Feedback imediato no seletor de arquivo (`config_dialog.c`)

**Files:**
- Modify: `src/config_dialog.c`

**Interfaces:**
- Consumes: `mesh_import_file_too_big(const wchar_t *path)` (Task 1).

- [ ] **Step 1: Incluir o header**

Adicionar ao bloco de includes do topo do arquivo:

```c
#include "geometry/mesh_import.h"
```

- [ ] **Step 2: Checar no handler de `IDC_MESHPICK`**

Trocar:

```c
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
```

por:

```c
                    ofn.lpstrFile = file;
                    ofn.nMaxFile = 512;
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                    if (GetOpenFileNameW(&ofn)) {
                        if (mesh_import_file_too_big(file)) {
                            MessageBoxW(h,
                                L"Este arquivo passa de 10MB e nao sera aceito - "
                                L"escolha uma malha menor.",
                                L"Arquivo muito grande", MB_OK | MB_ICONWARNING);
                        } else {
                            wcsncpy(g_work.mesh_path, file, 511);
                            g_work.mesh_path[511] = 0;
                            content_mesh_label(h);
                            preview_dirty(h);
                        }
                    }
                    break;
                }
                case IDC_MESHCLEAR:
```

- [ ] **Step 3: Build de depuração + smoke test**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile debug
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

```powershell
$env:M3DT_SELFTEST = "1"
$env:M3DT_TAB = "0"
$env:M3DT_HOLD_MS = "3000"
$p = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p | Wait-Process -Timeout 8
Write-Output "exitcode: $($p.ExitCode)"
```

Esperado: `exitcode: 0`.

- [ ] **Step 4: Commit**

```bash
git add src/config_dialog.c
git commit -m "feat: warn immediately when picking a mesh file over 10MB"
```

---

### Task 3: Placa de erro 2D em `scene.c`

**Files:**
- Modify: `src/scene.c`

**Interfaces:**
- Consumes: `font_build_contours()`, `contour_mesh_build()`,
  `gl_mesh_upload()`/`gl_mesh_draw()`/`gl_mesh_free()`,
  `material_set_piece_color()` (todos já existentes).
- Produces: novos campos `error_pieces[2]`/`error_colors[2]`/
  `have_error_plaque` em `SceneRenderer` — internos ao arquivo, sem
  mudança de header público.

- [ ] **Step 1: Novos campos em `struct SceneRenderer`**

Trocar:

```c
    wchar_t  mesh_path[512];
    float    mesh_size_scale;
    int      mesh_use_file_materials;
    GlMesh  *mesh_pieces;
    v3      *mesh_piece_colors;
    int      mesh_piece_count;
    int      have_mesh_content;   /* "ja tentamos montar a malha atual pelo menos uma vez" -
                                      cobre tanto o caminho unico quanto o de pecas */
};
```

por:

```c
    wchar_t  mesh_path[512];
    float    mesh_size_scale;
    int      mesh_use_file_materials;
    GlMesh  *mesh_pieces;
    v3      *mesh_piece_colors;
    int      mesh_piece_count;
    int      have_mesh_content;   /* "ja tentamos montar a malha atual pelo menos uma vez" -
                                      cobre tanto o caminho unico quanto o de pecas */

    /* placa de erro (SVG/malha ausente ou invalida) - substitui o
       conteudo 3D normal por uma caixa+texto achatados, nao pertence
       a svg_*/mesh_* porque pode ser disparada por qualquer um dos dois */
    GlMesh   error_pieces[2];      /* [0] caixa, [1] texto */
    v3       error_colors[2];
    int      have_error_plaque;
};
```

- [ ] **Step 2: `free_error_plaque` + `build_flat_quad` + `build_error_plaque`**

Adicionar logo após `free_svg_pieces` e antes de `rebuild_mesh` (a ordem
importa: `rebuild_mesh` vai chamar `free_error_plaque` no seu início, e
`rebuild_svg_mesh`/`rebuild_imported_mesh`, definidas mais abaixo, vão
chamar `build_error_plaque`):

```c
static void free_error_plaque(SceneRenderer *s)
{
    if (s->have_error_plaque) {
        gl_mesh_free(&s->error_pieces[0]);
        gl_mesh_free(&s->error_pieces[1]);
    }
    s->have_error_plaque = 0;
}

/* quad achatado simples (2 triangulos, sem indice compartilhado),
   centrado na origem, normal +Z (voltada pra camera). */
static void build_flat_quad(float halfw, float halfh, float z, MeshData *out)
{
    memset(out, 0, sizeof *out);
    MeshVertex *v = (MeshVertex *)malloc(6 * sizeof(MeshVertex));
    unsigned *idx = (unsigned *)malloc(6 * sizeof(unsigned));
    v3 p[4] = {
        { -halfw, -halfh, z }, { halfw, -halfh, z },
        { halfw,  halfh, z }, { -halfw,  halfh, z }
    };
    int tri[6] = { 0, 1, 2, 0, 2, 3 };
    for (int i = 0; i < 6; ++i) {
        v3 pp = p[tri[i]];
        v[i] = (MeshVertex){ pp.x, pp.y, pp.z, 0.0f, 0.0f, 1.0f, 0.0f };
        idx[i] = (unsigned)i;
    }
    out->verts = v; out->nverts = 6;
    out->idx = idx; out->nidx = 6;
    out->minx = -halfw; out->maxx = halfw;
    out->miny = -halfh; out->maxy = halfh;
    out->minz = z; out->maxz = z;
    out->has_sdf = 0;
}

/* placa achatada (caixa cinza + texto preto) usada como fallback visivel
   quando SVG/malha configurado esta ausente ou invalido - reaproveita o
   mesmo pipeline de texto de sempre (font_build_contours+contour_mesh_build)
   com depth=0 e bevel desligado (mesmo efeito "achatado" que ja aparecia
   nos testes do SVG), fonte fixada no fallback do sistema (family=NULL) em
   vez da fonte configurada pelo usuario, quebras de linha fixas na propria
   'message'. Enquadramento de camera automatico via s->hx/hy/hz, igual a
   qualquer outro conteudo. */
static int build_error_plaque(SceneRenderer *s, const char *message)
{
    free_error_plaque(s);

    float tol = s->quality <= 0 ? 0.010f : (s->quality == 1 ? 0.004f : 0.0018f);

    ContourSet cs;
    if (!font_build_contours(message, NULL, 0, 0, tol, &cs)) return 0;

    MeshParams p;
    memset(&p, 0, sizeof p);
    p.bevel_mode = 2;   /* desligado */
    p.quality = s->quality;

    MeshData text_md;
    int ok = contour_mesh_build(&cs, p, &text_md);
    contourset_free(&cs);
    if (!ok) return 0;

    float text_hw = 0.5f * (text_md.maxx - text_md.minx);
    float text_h  = text_md.maxy - text_md.miny;
    float text_hh = 0.5f * text_h;
    const float MARGIN_FRAC = 0.25f;   /* fracao da altura do texto, em cada lado */
    float margin = MARGIN_FRAC * text_h;
    float box_hw = text_hw + margin;
    float box_hh = text_hh + margin;

    MeshData box_md;
    build_flat_quad(box_hw, box_hh, -0.02f, &box_md);

    s->error_pieces[0] = gl_mesh_upload(box_md.verts, box_md.nverts, box_md.idx, box_md.nidx);
    s->error_colors[0] = (v3){ 0.85f, 0.85f, 0.85f };
    mesh_data_free(&box_md);

    s->error_pieces[1] = gl_mesh_upload(text_md.verts, text_md.nverts, text_md.idx, text_md.nidx);
    s->error_colors[1] = (v3){ 0.0f, 0.0f, 0.0f };
    mesh_data_free(&text_md);

    s->have_error_plaque = 1;
    s->hx = box_hw; s->hy = box_hh; s->hz = 0.02f;
    s->wall_cache_count = 0;
    upload_sdf(s, NULL);
    log_infof("scene: placa de erro '%s'", message);
    return 1;
}
```

- [ ] **Step 3: Limpar a placa no início de `rebuild_mesh`**

Trocar:

```c
static int rebuild_mesh(SceneRenderer *s)
{
    float tol = s->quality <= 0 ? 0.010f : (s->quality == 1 ? 0.004f : 0.0018f);

    ContourSet cs;
```

por:

```c
static int rebuild_mesh(SceneRenderer *s)
{
    free_error_plaque(s);

    float tol = s->quality <= 0 ? 0.010f : (s->quality == 1 ? 0.004f : 0.0018f);

    ContourSet cs;
```

- [ ] **Step 4: `rebuild_svg_mesh` — limpar no início e usar a placa nas 2 falhas**

Trocar:

```c
static int rebuild_svg_mesh(SceneRenderer *s)
{
    free_svg_pieces(s);

    float tol = s->quality <= 0 ? 0.010f : (s->quality == 1 ? 0.004f : 0.0018f);

    SvgShapeSet svgset;
    int loaded = s->svg_path[0] != 0 && svg_shapes_load(s->svg_path, tol, &svgset);
    if (!loaded || svgset.count == 0) {
        if (loaded) svg_shapes_free(&svgset);
        strncpy(s->text, "SVG nao encontrado ou sem forma preenchida", sizeof s->text - 1);
        s->text[sizeof s->text - 1] = 0;
        s->have_svg_mesh = 1;
        return rebuild_mesh(s);
    }
```

por:

```c
static int rebuild_svg_mesh(SceneRenderer *s)
{
    free_svg_pieces(s);
    free_error_plaque(s);

    float tol = s->quality <= 0 ? 0.010f : (s->quality == 1 ? 0.004f : 0.0018f);

    SvgShapeSet svgset;
    int loaded = s->svg_path[0] != 0 && svg_shapes_load(s->svg_path, tol, &svgset);
    if (!loaded || svgset.count == 0) {
        if (loaded) svg_shapes_free(&svgset);
        s->have_svg_mesh = 1;
        return build_error_plaque(s, "SVG nao encontrado\nou sem forma preenchida");
    }
```

E trocar:

```c
    if (s->svg_mesh_count == 0) {
        free_svg_pieces(s);
        strncpy(s->text, "SVG sem geometria valida", sizeof s->text - 1);
        s->text[sizeof s->text - 1] = 0;
        s->have_svg_mesh = 1;
        return rebuild_mesh(s);
    }
```

por:

```c
    if (s->svg_mesh_count == 0) {
        free_svg_pieces(s);
        s->have_svg_mesh = 1;
        return build_error_plaque(s, "SVG sem\ngeometria valida");
    }
```

- [ ] **Step 5: `rebuild_imported_mesh` — limpar no início e usar a placa na falha**

Trocar:

```c
static int rebuild_imported_mesh(SceneRenderer *s)
{
    free_mesh_pieces(s);
    s->have_mesh_content = 1;
```

por:

```c
static int rebuild_imported_mesh(SceneRenderer *s)
{
    free_mesh_pieces(s);
    free_error_plaque(s);
    s->have_mesh_content = 1;
```

E trocar:

```c
    MeshData md;
    int ok = s->mesh_path[0] != 0 && mesh_import_load(s->mesh_path, s->mesh_size_scale, &md);
    if (!ok) {
        strncpy(s->text, "Malha nao encontrada ou invalida", sizeof s->text - 1);
        s->text[sizeof s->text - 1] = 0;
        return rebuild_mesh(s);
    }
```

por:

```c
    MeshData md;
    int ok = s->mesh_path[0] != 0 && mesh_import_load(s->mesh_path, s->mesh_size_scale, &md);
    if (!ok) {
        return build_error_plaque(s, "Malha nao encontrada\nou invalida");
    }
```

- [ ] **Step 6: Widen os dois guards de "sem conteudo nenhum"**

Em `scene_create`, trocar:

```c
    scene_set_config(s, cfg);
    if (!s->have_mesh && s->svg_mesh_count == 0 && s->mesh_piece_count == 0) {
        log_errorf("scene: sem malha inicial");
        material_destroy(&s->mat);
        free(s);
        return NULL;
    }
    return s;
```

por:

```c
    scene_set_config(s, cfg);
    if (!s->have_mesh && s->svg_mesh_count == 0 && s->mesh_piece_count == 0
        && !s->have_error_plaque) {
        log_errorf("scene: sem malha inicial");
        material_destroy(&s->mat);
        free(s);
        return NULL;
    }
    return s;
```

(sem essa mudança, um SVG/malha invalido logo na primeira config faria o
`scene_create` falhar por completo — ele so' saberia que a placa de erro
foi construida com sucesso, nao que "nao ha malha nenhuma".)

Em `scene_render`, trocar:

```c
    if (!s->have_mesh && s->svg_mesh_count == 0 && s->mesh_piece_count == 0) return;
```

por:

```c
    if (!s->have_mesh && s->svg_mesh_count == 0 && s->mesh_piece_count == 0
        && !s->have_error_plaque) return;
```

- [ ] **Step 7: Desenhar a placa com prioridade no dispatch de `scene_render`**

Trocar:

```c
    if (s->content_mode == CONTENT_SVG && s->svg_mesh_count > 0) {
        for (int i = 0; i < s->svg_mesh_count; ++i) {
            v3 piece_color = (s->svg_color_mode == 0) ? s->svg_colors[i] : s->base_color;
            material_set_piece_color(&s->mat, piece_color);
            gl_mesh_draw(&s->svg_meshes[i]);
        }
    } else if (s->content_mode == CONTENT_MESH && s->mesh_piece_count > 0) {
```

por:

```c
    if (s->have_error_plaque) {
        material_set_piece_color(&s->mat, s->error_colors[0]);
        gl_mesh_draw(&s->error_pieces[0]);
        material_set_piece_color(&s->mat, s->error_colors[1]);
        gl_mesh_draw(&s->error_pieces[1]);
    } else if (s->content_mode == CONTENT_SVG && s->svg_mesh_count > 0) {
        for (int i = 0; i < s->svg_mesh_count; ++i) {
            v3 piece_color = (s->svg_color_mode == 0) ? s->svg_colors[i] : s->base_color;
            material_set_piece_color(&s->mat, piece_color);
            gl_mesh_draw(&s->svg_meshes[i]);
        }
    } else if (s->content_mode == CONTENT_MESH && s->mesh_piece_count > 0) {
```

- [ ] **Step 8: Limpar em `scene_destroy`**

Trocar:

```c
    if (s->have_mesh) gl_mesh_free(&s->mesh);
    free_svg_pieces(s);
    free_mesh_pieces(s);
    particles_destroy(s->particles);
```

por:

```c
    if (s->have_mesh) gl_mesh_free(&s->mesh);
    free_svg_pieces(s);
    free_mesh_pieces(s);
    free_error_plaque(s);
    particles_destroy(s->particles);
```

- [ ] **Step 9: Build de depuração**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile debug
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

Esperado: build limpo, sem warnings.

- [ ] **Step 10: Verificação visual real (3 mensagens de erro)**

**Aviso ao usuário**: tentar `PushNotification` antes das capturas reais
deste step; se vier "not sent", mandar mensagem de chat e **esperar
confirmação explícita** (`AskUserQuestion`) antes de prosseguir. Sempre
re-setar todas as variáveis de ambiente em cada chamada do PowerShell.

Malha inválida:

```powershell
$key = "HKCU:\Software\Modern3DText"
New-Item -Path $key -Force | Out-Null
Set-ItemProperty -Path $key -Name "content_mode" -Value "3"
Set-ItemProperty -Path $key -Name "mesh_path" -Value "C:\caminho\que\nao\existe.obj"
$env:M3DT_SELFTEST = "1"
$env:M3DT_HOLD_MS = "4000"
$env:M3DT_SHOT = "$env:TEMP\m3dt_plaque_mesh.png"
$p = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p | Wait-Process -Timeout 10
Write-Output "exitcode: $($p.ExitCode)"
```

SVG ausente:

```powershell
Set-ItemProperty -Path $key -Name "content_mode" -Value "2"
Set-ItemProperty -Path $key -Name "svg_path" -Value "C:\caminho\que\nao\existe.svg"
$env:M3DT_SELFTEST = "1"
$env:M3DT_HOLD_MS = "4000"
$env:M3DT_SHOT = "$env:TEMP\m3dt_plaque_svg.png"
$p2 = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p2 | Wait-Process -Timeout 10
Write-Output "exitcode: $($p2.ExitCode)"
```

Usar `Read` nas duas imagens geradas — esperado: caixa cinza-claro sólida
com texto preto legível em 2 linhas, fundo (gradiente/nebulosa) ainda
visível ao redor da placa. Depois, confirmar que trocar para um SVG/malha
válido faz a placa desaparecer (reaproveitar um fixture válido dos testes
anteriores, ex.: o `.gltf` multi-material da Fase 7c) e restaurar o
registro:

```powershell
Remove-Item -Path "HKCU:\Software\Modern3DText" -Recurse -Force -ErrorAction SilentlyContinue
```

- [ ] **Step 11: Suite de testes completa + build release**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile release
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

- [ ] **Step 12: Commit**

```bash
git add src/scene.c
git commit -m "feat: replace unreadable 3D error text with a flat error plaque"
```

---

## Self-Review (executado antes de apresentar o plano)

1. **Cobertura do spec**: §2 (limite de tamanho, 3 camadas) → Task 1 +
   Task 2; §3 (placa de erro: gatilhos, texto, caixa, render, limpeza) →
   Task 3, cada bullet do spec mapeado a um step (gatilhos → steps 3-5,
   render → steps 6-7, limpeza → steps 3-5 e 8); §4/§5 (sem config novo,
   único efeito de UI é o `MessageBoxW`) → confirmado, nenhuma task extra
   necessária.
2. **Placeholders**: nenhum "TBD" — todo trecho cola direto no arquivo
   indicado, com `old_string`/`new_string` exatos verificados contra o
   conteúdo REAL atual de cada arquivo (relido nesta sessão antes de
   escrever o plano).
3. **Consistência de tipos**: `mesh_import_file_too_big` (Task 1) tem a
   MESMA assinatura usada na Task 2; `build_error_plaque`/
   `free_error_plaque`/`build_flat_quad` (Task 3) usam os tipos já
   existentes (`ContourSet`, `MeshParams`, `MeshData`, `GlMesh`, `v3`) sem
   introduzir nenhum tipo novo.
4. **Risco verificado à mão**: confirmei lendo `contour_mesh.c` que
   `depth=0` + `bevel_mode=2` é um caminho seguro e já exercitado (gera
   tampas frente/verso coincidentes em z=0 e paredes de altura zero,
   ambas inofensivas com `GL_CULL_FACE` desabilitado globalmente pelo
   projeto) — não é um caso novo/arriscado, é o mesmo efeito que o
   usuário já viu nos testes do SVG. Também confirmei em `cgltf.h` que
   `cgltf_buffer.size` (tipo `cgltf_size` = `size_t`) é o campo certo pra
   somar os tamanhos declarados, e que `font_build_contours` já usa
   `"Segoe UI"` como fallback quando `family` é NULL — não preciso
   hardcodar a string eu mesmo.
5. **Bug potencial evitado**: os dois guards de "sem conteúdo nenhum" em
   `scene_create`/`scene_render` (`if (!have_mesh && svg_mesh_count==0 &&
   mesh_piece_count==0)`) precisam incluir `&& !have_error_plaque` — sem
   isso, `scene_create` falharia completamente (retornando NULL) sempre
   que a config inicial apontasse pra um SVG/malha inválido, o que é
   exatamente o caso que esta fase deveria tornar legível, não travar.
   Coberto explicitamente no Step 6 da Task 3.
