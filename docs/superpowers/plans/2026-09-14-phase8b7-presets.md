# Fase 8b-7 — Sistema de Presets — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rodapé fixo no dialog de config com um dropdown de presets
(4 embutidos + salvos pelo usuário) e Salvar/Apagar/Importar/Exportar,
afetando só os campos "visuais" da `Config` (nunca Texto/Fonte/
Conteúdo/Movimento).

**Architecture:** `src/presets.c/h` novo concentra toda a lógica de
presets (os 4 embutidos como funções `Config`-builder, CRUD de
presets salvos como subchaves do registro reaproveitando
`config_load_from`/`config_save_to`, e import/export como arquivo
`.ini` via uma subchave temporária pro reaproveitar o mesmo
saneamento). O dialog principal ganha um rodapé novo com o combo +
4 botões, e um mecanismo de "recarregar as 9 abas" implementado como
**destruir e recriar os 9 sub-diálogos** — reaproveita 100% do código
de `WM_INITDIALOG` de cada aba (já testado e correto) em vez de
duplicar a lógica de carregamento em funções novas.

**Tech Stack:** C (C99), Win32 (registro, dialog resources, combo/
botões, `MessageBoxW`), `build/Makefile`.

## Global Constraints

- Todo campo/preset segue o padrão já usado no projeto: nomes de
  chave do registro idênticos aos já usados por
  `config_load_from`/`config_save_to` (crítico pro import/export
  conseguir reaproveitar o saneamento existente via subchave
  temporária).
- `mingw32-make -f build/Makefile clean` obrigatório após editar
  qualquer `.h`.
- Copiar `dist/Modern3DText.scr` -> `dist/Modern3DText.exe` após todo
  build real.
- Captura de tela real exige aviso prévio (PushNotification, senão
  chat + `AskUserQuestion` aguardando confirmação).
- Registro `HKCU\Software\Modern3DText`: checar/backupar antes de
  limpar pra teste de instalação limpa.
- **Campo `base_color` é armazenado como uma única chave hex
  `#RRGGBB`** (não `base_r`/`base_g`/`base_b` separados) — ver
  `config.c:255-260` (load) e `config.c:455-460` (save). Qualquer
  código que escreva/leia esse campo via registro (import/export)
  precisa usar esse formato exato, senão a cor não sobrevive ao
  round-trip.

---

### Task 1: `src/presets.h`/`.c` — escopo + os 4 presets embutidos

**Files:**
- Create: `src/presets.h`
- Create: `src/presets.c`
- Create: `build/tests/test_presets.c`
- Modify: `build/Makefile` (adicionar `presets.c` a `SRC_C` e
  `test_presets.c` à lista de testes)

**Interfaces:**
- Produces: `preset_scope_copy(Config *dst, const Config *src)`,
  `g_builtin_presets[4]` (array de `{StrId name; void(*build)(Config*);}`),
  usados pelas Tasks 2, 6 e 7.

- [ ] **Step 1: Criar `src/presets.h`**

```c
#ifndef M3DT_PRESETS_H
#define M3DT_PRESETS_H
#include "config.h"
#include "i18n.h"

#define PRESET_NAME_MAX 64
#define BUILTIN_PRESET_COUNT 4

typedef struct {
    StrId name;
    void (*build)(Config *out);
} BuiltinPreset;

extern const BuiltinPreset g_builtin_presets[BUILTIN_PRESET_COUNT];

/* copia so os campos "visuais" (material/geometria-bevel/fundo/
   efeitos/particulas/cor do texto/qualidade) de src pra dst - usada
   tanto pra aplicar um preset em g_work quanto pra extrair de g_work
   na hora de salvar. Ao copiar particles_kind/density/speed/size/
   opacity, tambem sincroniza o par de memoria por tipo correspondente
   (particles_<kind>_density/size/opacity) com os mesmos valores. */
void preset_scope_copy(Config *dst, const Config *src);

/* presets salvos pelo usuario - subchaves de <base>\<nome>. As
   variantes sem sufixo _from usam a base real do produto; as com
   _from aceitam uma base alternativa (testes). */
void preset_user_save(const wchar_t *name, const Config *from);
int  preset_user_load(const wchar_t *name, Config *out);
void preset_user_delete(const wchar_t *name);
int  preset_user_list(wchar_t names[][PRESET_NAME_MAX], int max);

void preset_user_save_to(const wchar_t *base, const wchar_t *name, const Config *from);
int  preset_user_load_from(const wchar_t *base, const wchar_t *name, Config *out);
void preset_user_delete_from(const wchar_t *base, const wchar_t *name);
int  preset_user_list_from(const wchar_t *base, wchar_t names[][PRESET_NAME_MAX], int max);

/* importa/exporta um preset como arquivo .ini (chave=valor, cabecalho
   [Preset]) - reaproveita o saneamento de config_load_from via uma
   subchave temporaria. Retornam 1 em sucesso, 0 em falha. */
int preset_export_file(const wchar_t *path, const Config *from);
int preset_import_file(const wchar_t *path, Config *out);

#endif
```

- [ ] **Step 2: Criar `src/presets.c` com o escopo + os 4 builders**

```c
#include "presets.h"
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

void preset_scope_copy(Config *dst, const Config *src)
{
    dst->material_mode = src->material_mode;
    dst->metalness = src->metalness;
    dst->roughness = src->roughness;
    dst->env_mode = src->env_mode;
    dst->bevel_mode = src->bevel_mode;
    dst->background_type = src->background_type;
    dst->bg_color1_r = src->bg_color1_r; dst->bg_color1_g = src->bg_color1_g; dst->bg_color1_b = src->bg_color1_b;
    dst->bg_color2_r = src->bg_color2_r; dst->bg_color2_g = src->bg_color2_g; dst->bg_color2_b = src->bg_color2_b;
    dst->bg_grad_angle = src->bg_grad_angle;
    dst->bg_neb_color1_r = src->bg_neb_color1_r; dst->bg_neb_color1_g = src->bg_neb_color1_g; dst->bg_neb_color1_b = src->bg_neb_color1_b;
    dst->bg_neb_color2_r = src->bg_neb_color2_r; dst->bg_neb_color2_g = src->bg_neb_color2_g; dst->bg_neb_color2_b = src->bg_neb_color2_b;
    dst->bloom_on = src->bloom_on;
    dst->bloom_threshold = src->bloom_threshold;
    dst->bloom_intensity = src->bloom_intensity;
    dst->bloom_radius = src->bloom_radius;
    dst->streaks_mode = src->streaks_mode;
    dst->streaks_intensity = src->streaks_intensity;
    dst->streaks_length = src->streaks_length;
    dst->chroma_on = src->chroma_on;
    dst->chroma_strength = src->chroma_strength;
    dst->vignette_on = src->vignette_on;
    dst->vignette_amount = src->vignette_amount;
    dst->fxaa_on = src->fxaa_on;
    dst->particles_on = src->particles_on;
    dst->particles_kind = src->particles_kind;
    dst->particles_density = src->particles_density;
    dst->particles_speed = src->particles_speed;
    dst->particles_size_scale = src->particles_size_scale;
    dst->particles_opacity = src->particles_opacity;
    switch (src->particles_kind) {
        case 0:
            dst->particles_dust_density = src->particles_density;
            dst->particles_dust_size = src->particles_size_scale;
            dst->particles_dust_opacity = src->particles_opacity;
            break;
        case 1:
            dst->particles_bokeh_density = src->particles_density;
            dst->particles_bokeh_size = src->particles_size_scale;
            dst->particles_bokeh_opacity = src->particles_opacity;
            break;
        case 2:
            dst->particles_sparks_density = src->particles_density;
            dst->particles_sparks_size = src->particles_size_scale;
            dst->particles_sparks_opacity = src->particles_opacity;
            break;
        case 3:
            dst->particles_stars_density = src->particles_density;
            dst->particles_stars_size = src->particles_size_scale;
            dst->particles_stars_opacity = src->particles_opacity;
            break;
    }
    dst->base_r = src->base_r; dst->base_g = src->base_g; dst->base_b = src->base_b;
    dst->quality = src->quality;
}

static void preset_classico(Config *out)
{
    config_defaults(out);
    out->material_mode = 0;
    out->metalness = 0.9f; out->roughness = 0.5f; out->env_mode = 0;
    out->bevel_mode = 0;
    out->base_r = 0.72157f; out->base_g = 0.74118f; out->base_b = 0.78039f;
    out->background_type = 1;
    out->bg_color1_r = 0.06275f; out->bg_color1_g = 0.03922f; out->bg_color1_b = 0.03922f;
    out->bg_color2_r = 0.04706f; out->bg_color2_g = 0.07451f; out->bg_color2_b = 0.13725f;
    out->bg_grad_angle = 101.0f;
    out->bloom_on = 1; out->bloom_threshold = 1.0f; out->bloom_intensity = 0.3f; out->bloom_radius = 0.18f;
    out->streaks_mode = 0; out->streaks_intensity = 0.76f; out->streaks_length = 0.14f;
    out->chroma_on = 0; out->chroma_strength = 0.52f;
    out->vignette_on = 0; out->vignette_amount = 0.35f;
    out->fxaa_on = 1;
    out->particles_on = 0; out->particles_kind = 0;
    out->particles_density = 0.43f; out->particles_speed = 0.76f; out->particles_size_scale = 0.80f; out->particles_opacity = 0.50f;
    out->quality = 2;
}

static void preset_cinema(Config *out)
{
    config_defaults(out);
    out->material_mode = 1;
    out->metalness = 0.9f; out->roughness = 0.5f; out->env_mode = 0;
    out->bevel_mode = 1;
    out->base_r = 0.72157f; out->base_g = 0.74118f; out->base_b = 0.78039f;
    out->background_type = 3;
    out->bg_color1_r = 0.06275f; out->bg_color1_g = 0.03922f; out->bg_color1_b = 0.03922f;
    out->bg_color2_r = 0.04706f; out->bg_color2_g = 0.07451f; out->bg_color2_b = 0.13725f;
    out->bg_grad_angle = 101.0f;
    out->bg_neb_color1_r = 0.02745f; out->bg_neb_color1_g = 0.01961f; out->bg_neb_color1_b = 0.07843f;
    out->bg_neb_color2_r = 0.24706f; out->bg_neb_color2_g = 0.09804f; out->bg_neb_color2_b = 0.34902f;
    out->bloom_on = 1; out->bloom_threshold = 0.64f; out->bloom_intensity = 0.65f; out->bloom_radius = 0.27f;
    out->streaks_mode = 2; out->streaks_intensity = 0.76f; out->streaks_length = 0.14f;
    out->chroma_on = 1; out->chroma_strength = 0.30f;
    out->vignette_on = 1; out->vignette_amount = 0.35f;
    out->fxaa_on = 1;
    out->particles_on = 1; out->particles_kind = 1;
    out->particles_density = 0.43f; out->particles_speed = 0.76f; out->particles_size_scale = 0.81f; out->particles_opacity = 0.42f;
    out->quality = 2;
}

static void preset_neon(Config *out)
{
    config_defaults(out);
    out->material_mode = 0;
    out->metalness = 0.9f; out->roughness = 0.5f; out->env_mode = 0;
    out->bevel_mode = 1;
    out->base_r = 1.0f; out->base_g = 0.1f; out->base_b = 0.6f;
    out->background_type = 0;
    out->bg_color1_r = 0.03f; out->bg_color1_g = 0.01f; out->bg_color1_b = 0.05f;
    out->bg_color2_r = 0.04706f; out->bg_color2_g = 0.07451f; out->bg_color2_b = 0.13725f;
    out->bg_grad_angle = 101.0f;
    out->bg_neb_color1_r = 0.02745f; out->bg_neb_color1_g = 0.01961f; out->bg_neb_color1_b = 0.07843f;
    out->bg_neb_color2_r = 0.24706f; out->bg_neb_color2_g = 0.09804f; out->bg_neb_color2_b = 0.34902f;
    out->bloom_on = 1; out->bloom_threshold = 0.4f; out->bloom_intensity = 1.3f; out->bloom_radius = 0.4f;
    out->streaks_mode = 1; out->streaks_intensity = 1.2f; out->streaks_length = 0.2f;
    out->chroma_on = 1; out->chroma_strength = 0.6f;
    out->vignette_on = 1; out->vignette_amount = 0.45f;
    out->fxaa_on = 1;
    out->particles_on = 1; out->particles_kind = 2;
    out->particles_density = 0.42f; out->particles_speed = 0.76f; out->particles_size_scale = 0.72f; out->particles_opacity = 0.42f;
    out->quality = 2;
}

static void preset_suave(Config *out)
{
    config_defaults(out);
    out->material_mode = 3;
    out->metalness = 0.9f; out->roughness = 0.5f; out->env_mode = 0;
    out->bevel_mode = 1;
    out->base_r = 0.20f; out->base_g = 0.21f; out->base_b = 0.24f;
    out->background_type = 1;
    out->bg_color1_r = 0.85f; out->bg_color1_g = 0.87f; out->bg_color1_b = 0.92f;
    out->bg_color2_r = 0.78f; out->bg_color2_g = 0.82f; out->bg_color2_b = 0.90f;
    out->bg_grad_angle = 90.0f;
    out->bg_neb_color1_r = 0.02745f; out->bg_neb_color1_g = 0.01961f; out->bg_neb_color1_b = 0.07843f;
    out->bg_neb_color2_r = 0.24706f; out->bg_neb_color2_g = 0.09804f; out->bg_neb_color2_b = 0.34902f;
    out->bloom_on = 1; out->bloom_threshold = 1.6f; out->bloom_intensity = 0.12f; out->bloom_radius = 0.12f;
    out->streaks_mode = 0; out->streaks_intensity = 0.76f; out->streaks_length = 0.14f;
    out->chroma_on = 0; out->chroma_strength = 0.52f;
    out->vignette_on = 0; out->vignette_amount = 0.35f;
    out->fxaa_on = 1;
    out->particles_on = 1; out->particles_kind = 0;
    out->particles_density = 0.43f; out->particles_speed = 0.76f; out->particles_size_scale = 0.80f; out->particles_opacity = 0.50f;
    out->quality = 1;
}

const BuiltinPreset g_builtin_presets[BUILTIN_PRESET_COUNT] = {
    { STR_PRESET_NAME_CLASSICO, preset_classico },
    { STR_PRESET_NAME_CINEMA,   preset_cinema },
    { STR_PRESET_NAME_NEON,     preset_neon },
    { STR_PRESET_NAME_SUAVE,    preset_suave },
};
```

As funções de CRUD/import-export das Tasks 2 e 3 já ficam declaradas
em `presets.h` (header coeso desde já), mas só são implementadas nos
próprios arquivos daquelas tasks. Isso não impede o build/testes desta
Task 1: `presets.h` só declara protótipos, e nada em `presets.c`/
`test_presets.c` (nesta task) chama essas funções ainda — um protótipo
sem chamada não gera erro de link, só as Tasks 2/3 precisam realmente
defini-las antes de serem chamadas.

- [ ] **Step 3: Adicionar `presets.c` ao Makefile**

Em `build/Makefile`, na lista `SRC_C` (linha 35-40), adicione
`src/presets.c` (ex.: logo após `src/config.c`):

```makefile
SRC_C := src/main.c src/cmdline.c src/util/log.c src/util/mathx.c src/util/clockfmt.c src/gl_core.c \
         src/gl_window.c src/host_win32.c src/config_dialog.c src/config.c src/presets.c \
         src/material.c src/scene.c src/env.c src/post.c src/particles.c \
         src/render_tiers.c src/render_tiers_gl.c src/i18n.c \
         src/geometry/font_outline.c src/geometry/stb_impl.c src/geometry/contour_mesh.c \
         src/geometry/sdf.c src/geometry/svg_shapes.c src/geometry/mesh_import.c
```

Na regra `test` (procure a linha que lista `build/tests/test_config.c
... src/config.c ...`), adicione `build/tests/test_presets.c` à lista
de arquivos de teste e `src/presets.c` à lista de fontes compiladas
junto.

- [ ] **Step 4: `build/tests/test_presets.c` - conferir os 4 builders**

```c
#include "test.h"
#include "presets.h"
#include <math.h>

static int nearf(float a, float b) { return fabsf(a - b) < 1e-3f; }

void run_presets_tests(void)
{
    Config c;

    g_builtin_presets[0].build(&c);   /* Classico */
    EXPECT(c.material_mode == 0);
    EXPECT(c.bevel_mode == 0);
    EXPECT(nearf(c.bg_grad_angle, 101.0f));
    EXPECT(c.bloom_on == 1 && nearf(c.bloom_threshold, 1.0f));
    EXPECT(c.streaks_mode == 0);
    EXPECT(c.chroma_on == 0);
    EXPECT(c.vignette_on == 0);
    EXPECT(c.particles_on == 0);
    EXPECT(c.quality == 2);

    g_builtin_presets[1].build(&c);   /* Cinema */
    EXPECT(c.material_mode == 1);
    EXPECT(nearf(c.metalness, 0.9f) && nearf(c.roughness, 0.5f));
    EXPECT(c.env_mode == 0);
    EXPECT(c.background_type == 3);
    EXPECT(c.streaks_mode == 2);
    EXPECT(nearf(c.chroma_strength, 0.30f));
    EXPECT(c.particles_on == 1 && c.particles_kind == 1);
    EXPECT(nearf(c.particles_size_scale, 0.81f));

    g_builtin_presets[2].build(&c);   /* Neon */
    EXPECT(c.material_mode == 0);
    EXPECT(c.background_type == 0);
    EXPECT(nearf(c.base_r, 1.0f) && nearf(c.base_g, 0.1f) && nearf(c.base_b, 0.6f));
    EXPECT(c.streaks_mode == 1);
    EXPECT(nearf(c.bloom_intensity, 1.3f));
    EXPECT(c.particles_kind == 2);

    g_builtin_presets[3].build(&c);   /* Suave */
    EXPECT(c.material_mode == 3);
    EXPECT(c.background_type == 1);
    EXPECT(nearf(c.bg_color1_r, 0.85f));
    EXPECT(c.bloom_on == 1 && nearf(c.bloom_intensity, 0.12f));
    EXPECT(c.particles_kind == 0);
    EXPECT(c.quality == 1);

    /* preset_scope_copy: so os campos do escopo mudam, resto fica intocado */
    Config dst, src;
    config_defaults(&dst);
    config_defaults(&src);
    strcpy(src.text, "Nao deve vazar");
    src.max_angle_y = 999.0f;
    src.material_mode = 2;
    src.particles_kind = 3;
    src.particles_density = 0.9f; src.particles_speed = 0.5f;
    src.particles_size_scale = 0.7f; src.particles_opacity = 0.78f;

    preset_scope_copy(&dst, &src);
    EXPECT(strcmp(dst.text, "3D Text+") == 0);           /* nao mexeu */
    EXPECT(!nearf(dst.max_angle_y, 999.0f));               /* nao mexeu */
    EXPECT(dst.material_mode == 2);                        /* mexeu */
    EXPECT(dst.particles_kind == 3);
    EXPECT(nearf(dst.particles_stars_density, 0.9f));       /* memoria do tipo sincronizada */
    EXPECT(nearf(dst.particles_stars_size, 0.7f));
    EXPECT(nearf(dst.particles_stars_opacity, 0.78f));
}
```

- [ ] **Step 5: Registrar a chamada do novo teste**

Em `build/tests/test_main.c`, adicione a declaração
`void run_presets_tests(void);` e a chamada `run_presets_tests();`
junto das outras (mesmo padrão de `run_config_tests()` etc. já
presentes nesse arquivo).

- [ ] **Step 6: Build limpo e testes**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```

Expected: build sem warnings, todos os testes passando (incluindo os
novos de `test_presets.c`).

- [ ] **Step 7: Commit**

```bash
git add src/presets.h src/presets.c build/tests/test_presets.c \
        build/tests/test_main.c build/Makefile
git commit -m "$(cat <<'EOF'
feat: add presets.c with visual-scope copy and 4 builtin presets

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: CRUD de presets salvos pelo usuário (registro)

**Files:**
- Modify: `src/presets.c` (implementações que faltavam)
- Modify: `build/tests/test_presets.c`

**Interfaces:**
- Consumes: `config_load_from`/`config_save_to` (já existentes,
  `config.h`), `preset_scope_copy` (Task 1).
- Produces: `preset_user_save/_to`, `preset_user_load/_from`,
  `preset_user_delete/_from`, `preset_user_list/_from` — consumidos
  pela Task 6.

- [ ] **Step 1: Implementar em `src/presets.c`**

Adicionar ao final de `src/presets.c`:

```c
static const wchar_t *PRESETS_BASE = L"Software\\Modern3DText\\Presets";

static int wcscmp_qsort(const void *a, const void *b)
{
    return wcscmp((const wchar_t *)a, (const wchar_t *)b);
}

void preset_user_save_to(const wchar_t *base, const wchar_t *name, const Config *from)
{
    Config tmp;
    config_defaults(&tmp);
    preset_scope_copy(&tmp, from);

    wchar_t path[600];
    swprintf(path, 600, L"%ls\\%ls", base, name);
    config_save_to(&tmp, path);
}

int preset_user_load_from(const wchar_t *base, const wchar_t *name, Config *out)
{
    wchar_t path[600];
    swprintf(path, 600, L"%ls\\%ls", base, name);

    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return 0;
    RegCloseKey(k);

    Config tmp;
    config_load_from(&tmp, path);
    preset_scope_copy(out, &tmp);
    return 1;
}

void preset_user_delete_from(const wchar_t *base, const wchar_t *name)
{
    wchar_t path[600];
    swprintf(path, 600, L"%ls\\%ls", base, name);
    RegDeleteKeyW(HKEY_CURRENT_USER, path);
}

int preset_user_list_from(const wchar_t *base, wchar_t names[][PRESET_NAME_MAX], int max)
{
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, base, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return 0;

    int n = 0;
    for (DWORD i = 0; n < max; ++i) {
        wchar_t name[PRESET_NAME_MAX];
        DWORD cch = PRESET_NAME_MAX;
        if (RegEnumKeyExW(k, i, name, &cch, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
            break;
        wcsncpy(names[n], name, PRESET_NAME_MAX - 1);
        names[n][PRESET_NAME_MAX - 1] = 0;
        ++n;
    }
    RegCloseKey(k);
    if (n > 1)
        qsort(names, (size_t)n, PRESET_NAME_MAX * sizeof(wchar_t), wcscmp_qsort);
    return n;
}

void preset_user_save(const wchar_t *name, const Config *from) { preset_user_save_to(PRESETS_BASE, name, from); }
int  preset_user_load(const wchar_t *name, Config *out) { return preset_user_load_from(PRESETS_BASE, name, out); }
void preset_user_delete(const wchar_t *name) { preset_user_delete_from(PRESETS_BASE, name); }
int  preset_user_list(wchar_t names[][PRESET_NAME_MAX], int max) { return preset_user_list_from(PRESETS_BASE, names, max); }
```

Adicionar `#include <stdlib.h>` no topo de `presets.c` (pra `qsort`).

- [ ] **Step 2: Testes de round-trip em `test_presets.c`**

Adicionar ao final de `run_presets_tests()`:

```c
    /* CRUD de presets salvos - base de teste separada */
    const wchar_t *TBASE = L"Software\\Modern3DText_test_presets";
    wchar_t names[8][PRESET_NAME_MAX];
    int n;

    /* limpeza defensiva antes (caso uma execucao anterior tenha falhado no meio) */
    n = preset_user_list_from(TBASE, names, 8);
    for (int i = 0; i < n; ++i) preset_user_delete_from(TBASE, names[i]);

    Config a;
    config_defaults(&a);
    a.material_mode = 3;
    a.background_type = 0;
    a.bg_color1_r = 0.11f; a.bg_color1_g = 0.22f; a.bg_color1_b = 0.33f;
    a.particles_on = 1; a.particles_kind = 2;
    a.particles_density = 0.55f; a.particles_speed = 0.66f;
    a.particles_size_scale = 0.77f; a.particles_opacity = 0.88f;
    a.base_r = 0.9f; a.base_g = 0.5f; a.base_b = 0.1f;
    a.quality = 1;

    preset_user_save_to(TBASE, L"Zulu", &a);
    preset_user_save_to(TBASE, L"Alfa", &a);

    n = preset_user_list_from(TBASE, names, 8);
    EXPECT(n == 2);
    EXPECT(wcscmp(names[0], L"Alfa") == 0);   /* ordem alfabetica */
    EXPECT(wcscmp(names[1], L"Zulu") == 0);

    Config b;
    EXPECT(preset_user_load_from(TBASE, L"Alfa", &b) == 1);
    EXPECT(b.material_mode == 3);
    EXPECT(nearf(b.bg_color1_r, 0.11f) && nearf(b.bg_color1_g, 0.22f) && nearf(b.bg_color1_b, 0.33f));
    EXPECT(b.particles_kind == 2);
    EXPECT(nearf(b.particles_sparks_density, 0.55f));
    EXPECT(nearf(b.base_r, 0.9f) && nearf(b.base_g, 0.5f) && nearf(b.base_b, 0.1f));
    EXPECT(b.quality == 1);

    EXPECT(preset_user_load_from(TBASE, L"NaoExiste", &b) == 0);

    preset_user_delete_from(TBASE, L"Alfa");
    n = preset_user_list_from(TBASE, names, 8);
    EXPECT(n == 1);
    EXPECT(wcscmp(names[0], L"Zulu") == 0);

    preset_user_delete_from(TBASE, L"Zulu");
    n = preset_user_list_from(TBASE, names, 8);
    EXPECT(n == 0);
```

- [ ] **Step 3: Build limpo e testes**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```

Expected: build sem warnings, todos os testes passando.

- [ ] **Step 4: Commit**

```bash
git add src/presets.c build/tests/test_presets.c
git commit -m "$(cat <<'EOF'
feat: add saved-preset registry CRUD (list/save/load/delete)

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Importar/exportar preset como `.ini`

**Files:**
- Modify: `src/presets.c`
- Modify: `build/tests/test_presets.c`

**Interfaces:**
- Consumes: `config_load_from` (reaproveita o saneamento existente
  via subchave temporária), `preset_scope_copy` (Task 1).
- Produces: `preset_export_file`, `preset_import_file`, consumidos
  pela Task 6.

- [ ] **Step 1: Implementar em `src/presets.c`**

Adicionar ao final de `src/presets.c`:

```c
int preset_export_file(const wchar_t *path, const Config *from)
{
    FILE *f = _wfopen(path, L"w, ccs=UTF-8");
    if (!f) return 0;

    unsigned r = (unsigned)(from->base_r * 255.0f + 0.5f);
    unsigned g = (unsigned)(from->base_g * 255.0f + 0.5f);
    unsigned bl = (unsigned)(from->base_b * 255.0f + 0.5f);

    fwprintf(f, L"[Preset]\r\n");
    fwprintf(f, L"material_mode=%d\r\n", from->material_mode);
    fwprintf(f, L"metalness=%.5f\r\n", (double)from->metalness);
    fwprintf(f, L"roughness=%.5f\r\n", (double)from->roughness);
    fwprintf(f, L"env_mode=%d\r\n", from->env_mode);
    fwprintf(f, L"bevel_mode=%d\r\n", from->bevel_mode);
    fwprintf(f, L"background_type=%d\r\n", from->background_type);
    fwprintf(f, L"bg_color1_r=%.5f\r\n", (double)from->bg_color1_r);
    fwprintf(f, L"bg_color1_g=%.5f\r\n", (double)from->bg_color1_g);
    fwprintf(f, L"bg_color1_b=%.5f\r\n", (double)from->bg_color1_b);
    fwprintf(f, L"bg_color2_r=%.5f\r\n", (double)from->bg_color2_r);
    fwprintf(f, L"bg_color2_g=%.5f\r\n", (double)from->bg_color2_g);
    fwprintf(f, L"bg_color2_b=%.5f\r\n", (double)from->bg_color2_b);
    fwprintf(f, L"bg_grad_angle=%.5f\r\n", (double)from->bg_grad_angle);
    fwprintf(f, L"bg_neb_color1_r=%.5f\r\n", (double)from->bg_neb_color1_r);
    fwprintf(f, L"bg_neb_color1_g=%.5f\r\n", (double)from->bg_neb_color1_g);
    fwprintf(f, L"bg_neb_color1_b=%.5f\r\n", (double)from->bg_neb_color1_b);
    fwprintf(f, L"bg_neb_color2_r=%.5f\r\n", (double)from->bg_neb_color2_r);
    fwprintf(f, L"bg_neb_color2_g=%.5f\r\n", (double)from->bg_neb_color2_g);
    fwprintf(f, L"bg_neb_color2_b=%.5f\r\n", (double)from->bg_neb_color2_b);
    fwprintf(f, L"bloom_on=%d\r\n", from->bloom_on);
    fwprintf(f, L"bloom_threshold=%.5f\r\n", (double)from->bloom_threshold);
    fwprintf(f, L"bloom_intensity=%.5f\r\n", (double)from->bloom_intensity);
    fwprintf(f, L"bloom_radius=%.5f\r\n", (double)from->bloom_radius);
    fwprintf(f, L"streaks_mode=%d\r\n", from->streaks_mode);
    fwprintf(f, L"streaks_intensity=%.5f\r\n", (double)from->streaks_intensity);
    fwprintf(f, L"streaks_length=%.5f\r\n", (double)from->streaks_length);
    fwprintf(f, L"chroma_on=%d\r\n", from->chroma_on);
    fwprintf(f, L"chroma_strength=%.5f\r\n", (double)from->chroma_strength);
    fwprintf(f, L"vignette_on=%d\r\n", from->vignette_on);
    fwprintf(f, L"vignette_amount=%.5f\r\n", (double)from->vignette_amount);
    fwprintf(f, L"fxaa_on=%d\r\n", from->fxaa_on);
    fwprintf(f, L"particles_on=%d\r\n", from->particles_on);
    fwprintf(f, L"particles_kind=%d\r\n", from->particles_kind);
    fwprintf(f, L"particles_density=%.5f\r\n", (double)from->particles_density);
    fwprintf(f, L"particles_speed=%.5f\r\n", (double)from->particles_speed);
    fwprintf(f, L"particles_size_scale=%.5f\r\n", (double)from->particles_size_scale);
    fwprintf(f, L"particles_opacity=%.5f\r\n", (double)from->particles_opacity);
    fwprintf(f, L"base_color=#%02X%02X%02X\r\n", r & 0xFF, g & 0xFF, bl & 0xFF);
    fwprintf(f, L"quality=%d\r\n", from->quality);

    fclose(f);
    return 1;
}

int preset_import_file(const wchar_t *path, Config *out)
{
    FILE *f = _wfopen(path, L"r, ccs=UTF-8");
    if (!f) return 0;

    const wchar_t *tmpkey = L"Software\\Modern3DText\\Presets\\_import_tmp";
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, tmpkey, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL) != ERROR_SUCCESS) {
        fclose(f);
        return 0;
    }

    wchar_t line[256];
    while (fgetws(line, 256, f)) {
        wchar_t *eq = wcschr(line, L'=');
        if (!eq) continue;
        *eq = 0;
        wchar_t *val = eq + 1;
        size_t vlen = wcslen(val);
        while (vlen > 0 && (val[vlen - 1] == L'\n' || val[vlen - 1] == L'\r')) val[--vlen] = 0;
        RegSetValueExW(k, line, 0, REG_SZ, (const BYTE *)val, (DWORD)((vlen + 1) * sizeof(wchar_t)));
    }
    fclose(f);
    RegCloseKey(k);

    Config tmp;
    config_load_from(&tmp, tmpkey);
    RegDeleteKeyW(HKEY_CURRENT_USER, tmpkey);

    preset_scope_copy(out, &tmp);
    return 1;
}
```

- [ ] **Step 2: Teste de round-trip em `test_presets.c`**

Adicionar ao final de `run_presets_tests()`:

```c
    /* import/export via arquivo temporario */
    wchar_t tmpdir[MAX_PATH], tmpfile[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpdir);
    swprintf(tmpfile, MAX_PATH, L"%lsm3dt_preset_test.ini", tmpdir);

    Config ea;
    config_defaults(&ea);
    ea.material_mode = 1;
    ea.background_type = 3;
    ea.bg_neb_color1_r = 0.5f; ea.bg_neb_color1_g = 0.25f; ea.bg_neb_color1_b = 0.75f;
    ea.particles_on = 1; ea.particles_kind = 3;
    ea.particles_density = 0.6f; ea.particles_speed = 0.7f;
    ea.particles_size_scale = 0.8f; ea.particles_opacity = 0.9f;
    ea.base_r = 0.2f; ea.base_g = 0.4f; ea.base_b = 0.6f;
    ea.quality = 0;

    EXPECT(preset_export_file(tmpfile, &ea) == 1);

    Config eb;
    EXPECT(preset_import_file(tmpfile, &eb) == 1);
    EXPECT(eb.material_mode == 1);
    EXPECT(eb.background_type == 3);
    EXPECT(nearf(eb.bg_neb_color1_r, 0.5f) && nearf(eb.bg_neb_color1_g, 0.25f) && nearf(eb.bg_neb_color1_b, 0.75f));
    EXPECT(eb.particles_kind == 3);
    EXPECT(nearf(eb.particles_stars_density, 0.6f));
    EXPECT(nearf(eb.base_r, 0.2f) && nearf(eb.base_g, 0.4f) && nearf(eb.base_b, 0.6f));
    EXPECT(eb.quality == 0);

    DeleteFileW(tmpfile);
    EXPECT(preset_import_file(tmpfile, &eb) == 0);   /* arquivo nao existe mais */
```

- [ ] **Step 3: Build limpo e testes**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```

Expected: build sem warnings, todos os testes passando.

- [ ] **Step 4: Commit**

```bash
git add src/presets.c build/tests/test_presets.c
git commit -m "$(cat <<'EOF'
feat: add preset import/export as .ini files

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Recarregar as 9 abas (destruir + recriar)

**Files:**
- Modify: `src/config_dialog.c`

**Interfaces:**
- Produces: `static void reload_all_tabs(HWND dlg)`, consumida pela
  Task 6.

- [ ] **Step 1: Adicionar `reload_all_tabs` logo após `select_tab`**

Em `src/config_dialog.c`, logo depois da função `select_tab` (antes
de `apply_language_change`):

```c
/* destroi e recria os 9 sub-dialogos das abas - reaproveita 100% da
   logica de carregamento ja existente em cada *_proc's WM_INITDIALOG
   (sliders/radios/combos/labels a partir de g_work), sem precisar
   duplicar essa logica numa mensagem nova. Usado depois de aplicar
   um preset, quando varios campos de g_work mudam de uma vez. */
static void reload_all_tabs(HWND dlg)
{
    HWND tabs = GetDlgItem(dlg, IDC_TABS);

    DestroyWindow(g_content);
    DestroyWindow(g_motion);
    DestroyWindow(g_material);
    DestroyWindow(g_geometry);
    DestroyWindow(g_effects);
    DestroyWindow(g_perf);
    DestroyWindow(g_post);
    DestroyWindow(g_bg);
    DestroyWindow(g_particles);

    g_content = CreateDialogW(GetModuleHandleW(NULL),
                              MAKEINTRESOURCEW(IDD_TAB_CONTENT), dlg, content_proc);
    g_motion = CreateDialogW(GetModuleHandleW(NULL),
                             MAKEINTRESOURCEW(IDD_TAB_MOTION), dlg, motion_proc);
    g_material = CreateDialogW(GetModuleHandleW(NULL),
                              MAKEINTRESOURCEW(IDD_TAB_MATERIAL), dlg, material_proc);
    g_geometry = CreateDialogW(GetModuleHandleW(NULL),
                               MAKEINTRESOURCEW(IDD_TAB_GEOMETRY), dlg, geometry_proc);
    g_effects = CreateDialogW(GetModuleHandleW(NULL),
                              MAKEINTRESOURCEW(IDD_TAB_EFFECTS), dlg, effects_proc);
    g_perf = CreateDialogW(GetModuleHandleW(NULL),
                           MAKEINTRESOURCEW(IDD_TAB_PERF), dlg, perf_proc);
    g_post = CreateDialogW(GetModuleHandleW(NULL),
                           MAKEINTRESOURCEW(IDD_TAB_POST), dlg, post_proc);
    g_bg = CreateDialogW(GetModuleHandleW(NULL),
                         MAKEINTRESOURCEW(IDD_TAB_BG), dlg, bg_proc);
    g_particles = CreateDialogW(GetModuleHandleW(NULL),
                                MAKEINTRESOURCEW(IDD_TAB_PARTICLES), dlg, particles_proc);

    place_tab_child(dlg, tabs, g_content);
    place_tab_child(dlg, tabs, g_motion);
    place_tab_child(dlg, tabs, g_material);
    place_tab_child(dlg, tabs, g_geometry);
    place_tab_child(dlg, tabs, g_effects);
    place_tab_child(dlg, tabs, g_perf);
    place_tab_child(dlg, tabs, g_post);
    place_tab_child(dlg, tabs, g_bg);
    place_tab_child(dlg, tabs, g_particles);

    select_tab(g_cur_tab);   /* mantem a aba atual selecionada, marca o preview sujo */
}
```

Nota: `place_tab_child`, `g_content`..`g_particles`, `g_cur_tab` e os
9 `*_proc` já existem - esta função só reusa o que
`dlg_proc`'s `WM_INITDIALOG` já faz na criação inicial.

- [ ] **Step 2: Build limpo (função ainda não é chamada por ninguém)**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile release 2>&1 | grep -iE "warn|error"
```

Expected: sem erros. Um warning `-Wunused-function` é esperado aqui
(a função só passa a ser usada na Task 6) - se aparecer, ignore por
ora, vai sumir assim que a Task 6 adicionar a chamada.

- [ ] **Step 3: Commit**

```bash
git add src/config_dialog.c
git commit -m "$(cat <<'EOF'
feat: add reload_all_tabs (destroy+recreate the 9 tab dialogs)

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Dialog "Nome do preset" + strings i18n

**Files:**
- Modify: `src/resource.h`
- Modify: `res/screensaver.rc`
- Modify: `src/i18n.h`
- Modify: `src/i18n.c`
- Modify: `res/lang/pt.txt`, `res/lang/en.txt`
- Modify: `src/config_dialog.c`

**Interfaces:**
- Produces: `static int prompt_preset_name(HWND owner, wchar_t *out, int outCap)`,
  consumida pela Task 6.

- [ ] **Step 1: Novos IDs em `resource.h`**

Ao final de `src/resource.h`, antes do `#endif`:

```c
#define IDD_PRESET_NAME    119

#define IDC_PRESET_LABEL      2000
#define IDC_PRESET_COMBO      2001
#define IDC_PRESET_SAVE       2002
#define IDC_PRESET_DELETE     2003
#define IDC_PRESET_IMPORT     2004
#define IDC_PRESET_EXPORT     2005
#define IDC_PRESET_NAME_LABEL 2006
#define IDC_PRESET_NAME_EDIT  2007
```

- [ ] **Step 2: Novo template de dialog no `.rc`**

Em `res/screensaver.rc`, logo após o `END` do `IDD_CONFIG` (linha 16)
e antes de `IDD_TAB_CONTENT`:

```
IDD_PRESET_NAME DIALOGEX 0, 0, 200, 62
STYLE DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Nome do preset"
FONT 9, "Segoe UI", 400, 0, 0x1
BEGIN
    LTEXT         "Nome:", IDC_PRESET_NAME_LABEL, 8, 10, 184, 9
    EDITTEXT      IDC_PRESET_NAME_EDIT, 8, 22, 184, 14, ES_AUTOHSCROLL | WS_TABSTOP
    DEFPUSHBUTTON "OK",       IDOK,     70, 42, 56, 14
    PUSHBUTTON    "Cancelar", IDCANCEL, 132, 42, 56, 14
END
```

- [ ] **Step 3: Novas `StrId` em `i18n.h`**

Em `src/i18n.h`, ao final do enum `StrId` (antes do fechamento),
adicionar:

```c
    STR_PRESET_LABEL, STR_PRESET_SAVE_BTN, STR_PRESET_DELETE_BTN,
    STR_PRESET_IMPORT_BTN, STR_PRESET_EXPORT_BTN,
    STR_PRESET_NAME_TITLE, STR_PRESET_NAME_LABEL,
    STR_PRESET_APPLY_CONFIRM_TITLE, STR_PRESET_APPLY_CONFIRM,
    STR_PRESET_DELETE_CONFIRM_TITLE, STR_PRESET_DELETE_CONFIRM,
    STR_PRESET_OVERWRITE_CONFIRM_TITLE, STR_PRESET_OVERWRITE_CONFIRM,
    STR_PRESET_CANT_OVERWRITE_BUILTIN_TITLE, STR_PRESET_CANT_OVERWRITE_BUILTIN,
    STR_PRESET_NAME_CLASSICO, STR_PRESET_NAME_CINEMA,
    STR_PRESET_NAME_NEON, STR_PRESET_NAME_SUAVE,
    STR_FILTER_INI,
    STR_PRESET_IMPORT_FAILED_TITLE, STR_PRESET_IMPORT_FAILED,
    STR_PRESET_EXPORT_FAILED_TITLE, STR_PRESET_EXPORT_FAILED,
```

- [ ] **Step 4: Chaves correspondentes em `i18n.c`**

Em `src/i18n.c`, na MESMA posição relativa (final do array de
chaves), adicionar, na mesma ordem do Step 3:

```c
    "preset.label", "preset.save_btn", "preset.delete_btn",
    "preset.import_btn", "preset.export_btn",
    "preset.name_title", "preset.name_label",
    "preset.apply_confirm_title", "preset.apply_confirm",
    "preset.delete_confirm_title", "preset.delete_confirm",
    "preset.overwrite_confirm_title", "preset.overwrite_confirm",
    "preset.cant_overwrite_builtin_title", "preset.cant_overwrite_builtin",
    "preset.name.classico", "preset.name.cinema",
    "preset.name.neon", "preset.name.suave",
    "filter.ini",
    "preset.import_failed_title", "preset.import_failed",
    "preset.export_failed_title", "preset.export_failed",
```

- [ ] **Step 5: Strings em `res/lang/pt.txt`**

Ao final do arquivo, adicionar:

```
# presets
preset.label=Preset:
preset.save_btn=Salvar
preset.delete_btn=Apagar
preset.import_btn=Importar...
preset.export_btn=Exportar...
preset.name_title=Nome do preset
preset.name_label=Nome:
preset.apply_confirm_title=Aplicar preset
preset.apply_confirm=Aplicar o preset '%ls'? As alterações não salvas serão perdidas.
preset.delete_confirm_title=Apagar preset
preset.delete_confirm=Apagar o preset '%ls'? Essa ação não pode ser desfeita.
preset.overwrite_confirm_title=Sobrescrever preset
preset.overwrite_confirm=Já existe um preset chamado '%ls'. Sobrescrever?
preset.cant_overwrite_builtin_title=Nome reservado
preset.cant_overwrite_builtin='%ls' é um preset fixo e não pode ser sobrescrito. Escolha outro nome.
preset.name.classico=Clássico
preset.name.cinema=Cinema
preset.name.neon=Néon
preset.name.suave=Suave
filter.ini=Presets
preset.import_failed_title=Falha ao importar
preset.import_failed=Não foi possível ler o arquivo selecionado.
preset.export_failed_title=Falha ao exportar
preset.export_failed=Não foi possível gravar o arquivo selecionado.
```

- [ ] **Step 6: Strings em `res/lang/en.txt`**

Ao final do arquivo, adicionar:

```
# presets
preset.label=Preset:
preset.save_btn=Save
preset.delete_btn=Delete
preset.import_btn=Import...
preset.export_btn=Export...
preset.name_title=Preset name
preset.name_label=Name:
preset.apply_confirm_title=Apply preset
preset.apply_confirm=Apply preset '%ls'? Unsaved changes will be lost.
preset.delete_confirm_title=Delete preset
preset.delete_confirm=Delete preset '%ls'? This cannot be undone.
preset.overwrite_confirm_title=Overwrite preset
preset.overwrite_confirm=A preset named '%ls' already exists. Overwrite?
preset.cant_overwrite_builtin_title=Reserved name
preset.cant_overwrite_builtin='%ls' is a built-in preset and cannot be overwritten. Choose another name.
preset.name.classico=Classic
preset.name.cinema=Cinema
preset.name.neon=Neon
preset.name.suave=Smooth
filter.ini=Presets
preset.import_failed_title=Import failed
preset.import_failed=Could not read the selected file.
preset.export_failed_title=Export failed
preset.export_failed=Could not write the selected file.
```

- [ ] **Step 7: `prompt_preset_name` + `preset_name_proc` em `config_dialog.c`**

Em `src/config_dialog.c`, logo antes de `reload_all_tabs` (adicionada
na Task 4):

```c
static wchar_t *g_preset_name_out;
static int      g_preset_name_cap;

static INT_PTR CALLBACK preset_name_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG:
            SetWindowTextW(h, i18n_str(STR_PRESET_NAME_TITLE));
            SetDlgItemTextW(h, IDC_PRESET_NAME_LABEL, i18n_str(STR_PRESET_NAME_LABEL));
            SetDlgItemTextW(h, IDCANCEL, i18n_str(STR_BTN_CANCEL));
            /* IDOK fica "OK" fixo, sem traducao - mesma convencao ja usada
               no dialogo principal (apply_language_change nunca retraduz
               IDOK, so IDCANCEL/IDC_APPLY). */
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDOK: {
                    wchar_t buf[PRESET_NAME_MAX];
                    GetDlgItemTextW(h, IDC_PRESET_NAME_EDIT, buf, PRESET_NAME_MAX);
                    if (buf[0] == 0 || wcschr(buf, L'\\')) {
                        MessageBeep(MB_ICONWARNING);
                        return TRUE;   /* nome vazio ou com \\ - nao fecha */
                    }
                    wcsncpy(g_preset_name_out, buf, (size_t)g_preset_name_cap - 1);
                    g_preset_name_out[g_preset_name_cap - 1] = 0;
                    EndDialog(h, IDOK);
                    return TRUE;
                }
                case IDCANCEL:
                    EndDialog(h, IDCANCEL);
                    return TRUE;
            }
            return TRUE;
    }
    return FALSE;
}

/* pede um nome de preset ao usuario; devolve 1 e preenche out[0..outCap)
   se confirmado, 0 se cancelado. Rejeita nome vazio ou com '\\' (quebraria
   o caminho da subchave do registro). */
static int prompt_preset_name(HWND owner, wchar_t *out, int outCap)
{
    g_preset_name_out = out;
    g_preset_name_cap = outCap;
    out[0] = 0;
    return DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_PRESET_NAME),
                            owner, preset_name_proc, 0) == IDOK;
}
```

Adicionar `#include "presets.h"` no topo de `config_dialog.c` (junto
dos outros `#include`s locais).

- [ ] **Step 8: Build limpo**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile release 2>&1 | grep -iE "warn|error"
```

Expected: sem erros. `prompt_preset_name` ainda não é chamada por
ninguém (Task 6 faz isso) - um warning de função não usada aqui é
esperado e vai sumir na Task 6.

- [ ] **Step 9: Commit**

```bash
git add src/resource.h res/screensaver.rc src/i18n.h src/i18n.c \
        res/lang/pt.txt res/lang/en.txt src/config_dialog.c
git commit -m "$(cat <<'EOF'
feat: add preset-name prompt dialog and preset i18n strings

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: Rodapé de presets no dialog principal

**Files:**
- Modify: `res/screensaver.rc` (cresce `IDD_CONFIG` + nova linha)
- Modify: `src/config_dialog.c` (`dlg_proc`)

**Interfaces:**
- Consumes: `g_builtin_presets`, `preset_scope_copy`,
  `preset_user_save/load/delete/list`, `preset_export_file`,
  `preset_import_file` (Tasks 1-3), `reload_all_tabs` (Task 4),
  `prompt_preset_name` (Task 5).

- [ ] **Step 1: Crescer `IDD_CONFIG` e adicionar a linha de presets**

Em `res/screensaver.rc`, troque:

```
IDD_CONFIG DIALOGEX 0, 0, 448, 250
STYLE DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Modern 3D Text"
FONT 9, "Segoe UI", 400, 0, 0x1
BEGIN
    CONTROL         "", IDC_TABS, "SysTabControl32", WS_TABSTOP | TCS_MULTILINE, 6, 6, 250, 238
    CONTROL         "", IDC_PREVIEW, "Static", SS_BLACKFRAME, 264, 18, 178, 192
    DEFPUSHBUTTON   "OK",       IDOK,      264, 220, 56, 14
    PUSHBUTTON      "Cancelar", IDCANCEL,  326, 220, 56, 14
    PUSHBUTTON      "Aplicar",  IDC_APPLY, 386, 220, 56, 14
END
```

por:

```
IDD_CONFIG DIALOGEX 0, 0, 448, 268
STYLE DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Modern 3D Text"
FONT 9, "Segoe UI", 400, 0, 0x1
BEGIN
    CONTROL         "", IDC_TABS, "SysTabControl32", WS_TABSTOP | TCS_MULTILINE, 6, 6, 250, 238
    CONTROL         "", IDC_PREVIEW, "Static", SS_BLACKFRAME, 264, 18, 178, 192
    DEFPUSHBUTTON   "OK",       IDOK,      264, 220, 56, 14
    PUSHBUTTON      "Cancelar", IDCANCEL,  326, 220, 56, 14
    PUSHBUTTON      "Aplicar",  IDC_APPLY, 386, 220, 56, 14

    LTEXT           "Preset:", IDC_PRESET_LABEL, 6, 243, 40, 9
    COMBOBOX        IDC_PRESET_COMBO, 48, 240, 140, 100, CBS_DROPDOWNLIST | WS_TABSTOP
    PUSHBUTTON      "Salvar",     IDC_PRESET_SAVE,   192, 239, 48, 14
    PUSHBUTTON      "Apagar",     IDC_PRESET_DELETE, 242, 239, 48, 14
    PUSHBUTTON      "Importar...", IDC_PRESET_IMPORT, 292, 239, 68, 14
    PUSHBUTTON      "Exportar...", IDC_PRESET_EXPORT, 362, 239, 68, 14
END
```

- [ ] **Step 2: Estado do módulo + helper de repopular o combo**

Em `src/config_dialog.c`, logo após `prompt_preset_name` (Task 5):

```c
static int g_preset_sel = -1;   /* -1 = nenhum preset selecionado ainda */

/* repopula o combo: os 4 embutidos primeiro (traduzidos), depois os
   salvos em ordem alfabetica. Preserva a selecao visual em g_preset_sel
   se ainda for valida, senao limpa. */
static void preset_refresh_combo(HWND dlg)
{
    HWND cb = GetDlgItem(dlg, IDC_PRESET_COMBO);
    SendMessageW(cb, CB_RESETCONTENT, 0, 0);

    for (int i = 0; i < BUILTIN_PRESET_COUNT; ++i)
        SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)i18n_str(g_builtin_presets[i].name));

    wchar_t names[64][PRESET_NAME_MAX];
    int n = preset_user_list(names, 64);
    for (int i = 0; i < n; ++i)
        SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)names[i]);

    int total = BUILTIN_PRESET_COUNT + n;
    if (g_preset_sel >= total) g_preset_sel = -1;
    SendMessageW(cb, CB_SETCURSEL, g_preset_sel, 0);
    EnableWindow(GetDlgItem(dlg, IDC_PRESET_DELETE), g_preset_sel >= BUILTIN_PRESET_COUNT);
}

static void preset_apply_i18n(HWND dlg)
{
    SetDlgItemTextW(dlg, IDC_PRESET_LABEL, i18n_str(STR_PRESET_LABEL));
    SetDlgItemTextW(dlg, IDC_PRESET_SAVE, i18n_str(STR_PRESET_SAVE_BTN));
    SetDlgItemTextW(dlg, IDC_PRESET_DELETE, i18n_str(STR_PRESET_DELETE_BTN));
    SetDlgItemTextW(dlg, IDC_PRESET_IMPORT, i18n_str(STR_PRESET_IMPORT_BTN));
    SetDlgItemTextW(dlg, IDC_PRESET_EXPORT, i18n_str(STR_PRESET_EXPORT_BTN));
    preset_refresh_combo(dlg);
}
```

- [ ] **Step 3: Chamar no `WM_INITDIALOG` do `dlg_proc`**

Em `src/config_dialog.c`, dentro de `dlg_proc`'s `WM_INITDIALOG`,
logo após `EnableWindow(GetDlgItem(h, IDC_APPLY), FALSE);`:

```c
            g_preset_sel = -1;
            preset_apply_i18n(h);
```

E em `apply_language_change()` (que já re-traduz todo o resto da UI
ao trocar de idioma), dentro do bloco `if (g_dlg) { ... }` já
existente, logo depois do `for (int i = 0; i < 9; ++i) { ... }` que
retraduz as abas (ainda dentro das chaves do `if`), adicionar:

```c
        preset_apply_i18n(g_dlg);
```

(fica como a última linha antes do `}` que fecha o `if (g_dlg)`.)

- [ ] **Step 4: Handlers de `WM_COMMAND` no `dlg_proc`**

Em `src/config_dialog.c`, dentro de `dlg_proc`'s
`case WM_COMMAND: switch (LOWORD(w)) { ... }`, adicionar (junto de
`IDOK`/`IDC_APPLY`/`IDCANCEL` já existentes):

```c
                case IDC_PRESET_COMBO: {
                    if (HIWORD(w) != CBN_SELCHANGE) break;
                    HWND cb = GetDlgItem(h, IDC_PRESET_COMBO);
                    int sel = (int)SendMessageW(cb, CB_GETCURSEL, 0, 0);
                    if (sel < 0 || sel == g_preset_sel) break;

                    wchar_t display[PRESET_NAME_MAX];
                    SendMessageW(cb, CB_GETLBTEXT, sel, (LPARAM)display);

                    wchar_t msg[256];
                    swprintf(msg, 256, i18n_str(STR_PRESET_APPLY_CONFIRM), display);
                    if (MessageBoxW(h, msg, i18n_str(STR_PRESET_APPLY_CONFIRM_TITLE),
                                    MB_YESNO | MB_ICONQUESTION) != IDYES) {
                        SendMessageW(cb, CB_SETCURSEL, g_preset_sel, 0);
                        break;
                    }

                    Config tmp;
                    if (sel < BUILTIN_PRESET_COUNT) {
                        g_builtin_presets[sel].build(&tmp);
                    } else {
                        wchar_t names[64][PRESET_NAME_MAX];
                        int n = preset_user_list(names, 64);
                        int idx = sel - BUILTIN_PRESET_COUNT;
                        if (idx >= n || !preset_user_load(names[idx], &tmp)) break;
                    }
                    preset_scope_copy(&g_work, &tmp);
                    g_preset_sel = sel;
                    EnableWindow(GetDlgItem(h, IDC_PRESET_DELETE), sel >= BUILTIN_PRESET_COUNT);
                    reload_all_tabs(h);
                    EnableWindow(GetDlgItem(h, IDC_APPLY), TRUE);
                    break;
                }
                case IDC_PRESET_SAVE: {
                    wchar_t name[PRESET_NAME_MAX];
                    if (!prompt_preset_name(h, name, PRESET_NAME_MAX)) break;

                    int is_builtin_name = 0;
                    for (int i = 0; i < BUILTIN_PRESET_COUNT; ++i)
                        if (wcscmp(name, i18n_str(g_builtin_presets[i].name)) == 0) is_builtin_name = 1;
                    if (is_builtin_name) {
                        wchar_t msg[256];
                        swprintf(msg, 256, i18n_str(STR_PRESET_CANT_OVERWRITE_BUILTIN), name);
                        MessageBoxW(h, msg, i18n_str(STR_PRESET_CANT_OVERWRITE_BUILTIN_TITLE), MB_OK | MB_ICONWARNING);
                        break;
                    }

                    Config existing;
                    if (preset_user_load(name, &existing)) {
                        wchar_t msg[256];
                        swprintf(msg, 256, i18n_str(STR_PRESET_OVERWRITE_CONFIRM), name);
                        if (MessageBoxW(h, msg, i18n_str(STR_PRESET_OVERWRITE_CONFIRM_TITLE),
                                        MB_YESNO | MB_ICONQUESTION) != IDYES)
                            break;
                    }

                    preset_user_save(name, &g_work);
                    preset_refresh_combo(h);

                    wchar_t names[64][PRESET_NAME_MAX];
                    int n = preset_user_list(names, 64);
                    for (int i = 0; i < n; ++i)
                        if (wcscmp(names[i], name) == 0) { g_preset_sel = BUILTIN_PRESET_COUNT + i; break; }
                    SendMessageW(GetDlgItem(h, IDC_PRESET_COMBO), CB_SETCURSEL, g_preset_sel, 0);
                    EnableWindow(GetDlgItem(h, IDC_PRESET_DELETE), TRUE);
                    break;
                }
                case IDC_PRESET_DELETE: {
                    if (g_preset_sel < BUILTIN_PRESET_COUNT) break;
                    wchar_t names[64][PRESET_NAME_MAX];
                    int n = preset_user_list(names, 64);
                    int idx = g_preset_sel - BUILTIN_PRESET_COUNT;
                    if (idx >= n) break;

                    wchar_t msg[256];
                    swprintf(msg, 256, i18n_str(STR_PRESET_DELETE_CONFIRM), names[idx]);
                    if (MessageBoxW(h, msg, i18n_str(STR_PRESET_DELETE_CONFIRM_TITLE),
                                    MB_YESNO | MB_ICONQUESTION) != IDYES)
                        break;

                    preset_user_delete(names[idx]);
                    g_preset_sel = -1;
                    preset_refresh_combo(h);
                    break;
                }
                case IDC_PRESET_IMPORT: {
                    wchar_t file[512] = L"";
                    OPENFILENAMEW ofn;
                    memset(&ofn, 0, sizeof ofn);
                    ofn.lStructSize = sizeof ofn;
                    ofn.hwndOwner = h;
                    wchar_t filter[128]; int fp = 0;
                    filter_append(filter, &fp, 128, i18n_str(STR_FILTER_INI));
                    filter_append(filter, &fp, 128, L"*.ini");
                    filter[fp] = 0;
                    ofn.lpstrFilter = filter;
                    ofn.lpstrFile = file;
                    ofn.nMaxFile = 512;
                    ofn.lpstrDefExt = L"ini";
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                    if (!GetOpenFileNameW(&ofn)) break;

                    Config imported;
                    if (!preset_import_file(file, &imported)) {
                        MessageBoxW(h, i18n_str(STR_PRESET_IMPORT_FAILED),
                                    i18n_str(STR_PRESET_IMPORT_FAILED_TITLE), MB_OK | MB_ICONERROR);
                        break;
                    }

                    wchar_t name[PRESET_NAME_MAX];
                    if (!prompt_preset_name(h, name, PRESET_NAME_MAX)) break;

                    int is_builtin_name = 0;
                    for (int i = 0; i < BUILTIN_PRESET_COUNT; ++i)
                        if (wcscmp(name, i18n_str(g_builtin_presets[i].name)) == 0) is_builtin_name = 1;
                    if (is_builtin_name) {
                        wchar_t msg[256];
                        swprintf(msg, 256, i18n_str(STR_PRESET_CANT_OVERWRITE_BUILTIN), name);
                        MessageBoxW(h, msg, i18n_str(STR_PRESET_CANT_OVERWRITE_BUILTIN_TITLE), MB_OK | MB_ICONWARNING);
                        break;
                    }
                    Config existing;
                    if (preset_user_load(name, &existing)) {
                        wchar_t msg[256];
                        swprintf(msg, 256, i18n_str(STR_PRESET_OVERWRITE_CONFIRM), name);
                        if (MessageBoxW(h, msg, i18n_str(STR_PRESET_OVERWRITE_CONFIRM_TITLE),
                                        MB_YESNO | MB_ICONQUESTION) != IDYES)
                            break;
                    }

                    preset_user_save(name, &imported);
                    preset_refresh_combo(h);
                    break;
                }
                case IDC_PRESET_EXPORT: {
                    if (g_preset_sel < 0) break;
                    wchar_t file[512] = L"";
                    OPENFILENAMEW ofn;
                    memset(&ofn, 0, sizeof ofn);
                    ofn.lStructSize = sizeof ofn;
                    ofn.hwndOwner = h;
                    wchar_t filter[128]; int fp = 0;
                    filter_append(filter, &fp, 128, i18n_str(STR_FILTER_INI));
                    filter_append(filter, &fp, 128, L"*.ini");
                    filter[fp] = 0;
                    ofn.lpstrFilter = filter;
                    ofn.lpstrFile = file;
                    ofn.nMaxFile = 512;
                    ofn.lpstrDefExt = L"ini";
                    ofn.Flags = OFN_OVERWRITEPROMPT;
                    if (!GetSaveFileNameW(&ofn)) break;

                    Config tmp;
                    if (g_preset_sel < BUILTIN_PRESET_COUNT) {
                        g_builtin_presets[g_preset_sel].build(&tmp);
                    } else {
                        wchar_t names[64][PRESET_NAME_MAX];
                        int n = preset_user_list(names, 64);
                        int idx = g_preset_sel - BUILTIN_PRESET_COUNT;
                        if (idx >= n || !preset_user_load(names[idx], &tmp)) break;
                    }
                    if (!preset_export_file(file, &tmp))
                        MessageBoxW(h, i18n_str(STR_PRESET_EXPORT_FAILED),
                                    i18n_str(STR_PRESET_EXPORT_FAILED_TITLE), MB_OK | MB_ICONERROR);
                    break;
                }
```

- [ ] **Step 5: Build limpo**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
mingw32-make -f build/Makefile release 2>&1 | grep -iE "warn|error"
```

Expected: sem warnings novos, todos os testes passando.

```bash
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

- [ ] **Step 6: Commit**

```bash
git add res/screensaver.rc src/config_dialog.c
git commit -m "$(cat <<'EOF'
feat: preset footer UI (select/save/delete/import/export)

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: Verificação visual real + commit final

**Files:** nenhum (só verificação)

- [ ] **Step 1: Aviso prévio e backup do registro**

Checar/backupar `HKCU\Software\Modern3DText` se existir. Aviso prévio
(PushNotification, senão chat + `AskUserQuestion` aguardando
confirmação) antes de qualquer captura.

- [ ] **Step 2: Aplicar cada um dos 4 presets embutidos**

Com registro limpo, abrir o dialog (`/c`), selecionar cada um dos 4
presets no combo do rodapé via simulação de UI real (`CB_SETCURSEL` +
`WM_COMMAND`/`CBN_SELCHANGE`, confirmando o `MessageBoxW` - via
`FindWindowExW` pra achar a janela do MessageBox e
`SendMessage(BM_CLICK)` no botão "Sim", técnica já usada nesta
sessão) e capturar a tela a cada troca, conferindo: (a) Material/
Fundo/Partículas mudam visivelmente conforme a tabela da Fase 8b-7,
(b) o Texto/Fonte digitados (testar com um texto customizado antes de
aplicar) permanecem intocados, (c) o botão Aplicar fica habilitado
após cada troca.

- [ ] **Step 3: Salvar, apagar, importar, exportar**

Ajustar manualmente Material/Fundo pra algo customizado; Salvar com
um nome novo; confirmar que aparece no combo depois de reabrir o
dialog. Selecionar esse preset salvo e Apagar (confirmando); conferir
que sumiu do combo. Selecionar um preset (embutido ou salvo),
Exportar pra um `.ini` num diretório de teste; abrir o arquivo gerado
e conferir visualmente o formato `[Preset]` + `chave=valor`.
Importar esse mesmo arquivo de volta com outro nome; conferir que
aparece no combo e, ao selecioná-lo, reproduz os mesmos valores.

- [ ] **Step 4: Limpeza**

Restaurar o backup do registro se um foi feito; senão, limpar a chave
de teste criada durante a verificação. Parar qualquer processo de
teste ainda rodando.

- [ ] **Step 5: Enviar o `.exe`**

Enviar `dist/Modern3DText.exe` ao usuário via `SendUserFile`.
