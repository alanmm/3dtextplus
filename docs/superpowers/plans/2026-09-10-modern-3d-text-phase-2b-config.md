# Modern 3D Text — Fase 2b: Configuração — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Tirar os parâmetros fixos do `scene.c` e colocá-los sob controle do usuário: uma struct `Config` persistida em `HKCU\Software\Modern3DText`, e um diálogo de configuração Win32 com abas **Conteúdo** e **Movimento** (campo de texto, seletor de fonte, cor, e sliders de profundidade / ângulo / inclinação / período), com **mini-preview 3D ao vivo** ao lado. O `/s` e o `/p` passam a renderizar o que estiver salvo.

**Architecture:** `config.c` define a `Config` e faz load/save em REG_SZ no registro do usuário (valores legíveis; ausentes ou fora de faixa caem para o padrão). `scene.c` deixa de ter `#define SC_*` e recebe um `const Config*` — `scene_set_config` reconstrói a malha só quando texto/fonte/profundidade mudam, senão só atualiza os parâmetros baratos de câmera/pêndulo/cor. A plumbing de janela+contexto GL sai de `host_win32.c` para um módulo `gl_window.c` reutilizável, e o `config_dialog.c` usa esse módulo para hospedar o mini-preview: um `SysTabControl32` com uma sub-janela de diálogo por aba à esquerda, e uma janela-filha GL à direita que roda a cena real em baixa-res, atualizada (com debounce) a cada mudança de controle.

**Tech Stack:** C11 · w64devkit · Win32 (registry, `SysTabControl32`, `msctls_trackbar32`, `ChooseColorW`, `EnumFontFamiliesExW`) · OpenGL 3.3 (reuso de `scene`/`material`/`gl_core` da Fase 2a).

## Global Constraints

Do spec e do estado pós-Fase 2a. Todo task herda esta seção.

- **Linguagem:** C11. Flags `-std=c11 -municode -Wall -Wextra` (+ `-O2 -DNDEBUG` no release). **Sem `-ffast-math`.** Build **sem warnings** a `-O2 -Wall -Wextra`.
- **Toolchain:** só w64devkit (`C:\Users\alanm\w64devkit`, `bin` no PATH).
- **Registro:** única chave de escrita é `HKCU\Software\Modern3DText` (testes usam `HKCU\Software\Modern3DText_test` e limpam depois). Nenhuma escrita fora disso e de `%LOCALAPPDATA%\Modern3DText\`.
- **Valores no registro:** todos `REG_SZ` legíveis (ex.: `depth="0.30"`, `base_color="#B7BDC7"`). Load tolera valor ausente, vazio ou fora de faixa → padrão; chave inexistente → todos os padrões + grava o preset.
- **Sem dependência de runtime** além de DLLs do Windows. Libs de link inalteradas.
- **`.scr` ≤ 3 MB** (o build falha acima disso).
- **Commits frequentes.** TDD em `config` (round-trip real no registro sob `_test`). Diálogo/GL: verificação headless (`M3DT_SELFTEST` fecha sozinho) + captura PNG onde fizer sentido.
- **i18n:** ainda **não** nesta fase — textos do diálogo em português direto no `.rc` (a tabela PT/EN é da Fase 8). Manter as strings curtas e num só lugar para facilitar depois.
- **Atribuição:** todo commit termina com `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>`.

---

## File Structure

| Arquivo | Responsabilidade |
|---|---|
| `src/config.h` / `.c` | struct `Config`; `config_defaults`, `config_load`/`config_save` (chave real), `config_load_from`/`config_save_to` (chave explícita, p/ testes); parse/format de float e cor |
| `src/gl_window.h` / `.c` | **extraído** de `host_win32.c`: `GlWindow`, DPI-aware, WGL bootstrap, `gl_window_create(cfg)` / `_destroy` / `_frame`, `gl_window_set_config` |
| `src/host_win32.c` | (modif.) usa `gl_window`; carrega `Config` no startup e passa para as janelas |
| `src/scene.h` / `.c` | (modif.) `scene_create(const Config*)`, `scene_set_config(SceneRenderer*, const Config*)` — reconstrói a malha só se necessário; sem `#define SC_*` |
| `src/config_dialog.c` | (reescrito) carrega `Config`, monta tab control + sub-diálogos por aba + mini-preview GL, OK/Aplicar/Cancelar |
| `res/screensaver.rc` | diálogo principal redesenhado + `IDD_TAB_CONTENT` + `IDD_TAB_MOTION` |
| `src/resource.h` | novos IDs |
| `build/tests/test_config.c` | round-trip, defaults, clamps |
| `build/Makefile` | `src/config.c` e `src/gl_window.c` em `SRC_C`; `test_config.c` nos testes; `-ladvapi32` já presente |

---

## Task 1: `Config` struct + registro (TDD)

**Files:**
- Create: `src/config.h`, `src/config.c`
- Create: `build/tests/test_config.c`
- Modify: `build/tests/test_main.c`, `build/Makefile`

**Interfaces:**
- Consumes: `util/log.h`.
- Produces:
  ```c
  typedef enum { CONTENT_TEXT = 0 } ContentMode;

  typedef struct {
      int          version;              /* schema; atual = 1 */
      ContentMode  content_mode;
      char         text[512];            /* UTF-8 */
      wchar_t      font_family[64];
      int          font_bold;            /* 0/1 */
      int          font_italic;          /* 0/1 */
      float        depth;                /* 0.02 .. 2.0 */
      float        max_angle_y;          /* 5 .. 170 (graus) */
      float        tilt_x;               /* 0 .. 30 (graus) */
      float        period;               /* 2 .. 30 (s) */
      float        base_r, base_g, base_b;  /* 0..1 */
  } Config;

  void config_defaults(Config *c);
  void config_load(Config *c);                              /* HKCU\Software\Modern3DText */
  void config_save(const Config *c);
  void config_load_from(Config *c, const wchar_t *subkey);  /* p/ testes: "Software\\X" */
  void config_save_to(const Config *c, const wchar_t *subkey);
  ```
  Padrões: `version=1`, `CONTENT_TEXT`, `text="Modern 3D Text"`, `font_family=L"Segoe UI"`, `bold=1`, `italic=0`, `depth=0.30`, `max_angle_y=42`, `tilt_x=8`, `period=9`, `base=(0.72,0.74,0.78)`.

- [ ] **Step 1: Escrever `build/tests/test_config.c` (falha primeiro)**

```c
#include "test.h"
#include "config.h"
#include <windows.h>
#include <string.h>
#include <math.h>

#define TESTKEY L"Software\\Modern3DText_test"

static int nearf(float a, float b) { return fabsf(a - b) < 1e-3f; }

void run_config_tests(void)
{
    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);

    /* defaults sensatos */
    Config d;
    config_defaults(&d);
    EXPECT(strcmp(d.text, "Modern 3D Text") == 0);
    EXPECT(wcscmp(d.font_family, L"Segoe UI") == 0);
    EXPECT(nearf(d.depth, 0.30f));
    EXPECT(d.version == 1);

    /* round-trip */
    Config a;
    config_defaults(&a);
    strcpy(a.text, "Ola Mundo");
    wcscpy(a.font_family, L"Arial");
    a.font_bold = 0;
    a.font_italic = 1;
    a.depth = 0.55f;
    a.max_angle_y = 30.0f;
    a.tilt_x = 12.0f;
    a.period = 6.5f;
    a.base_r = 0.10f; a.base_g = 0.20f; a.base_b = 0.90f;
    config_save_to(&a, TESTKEY);

    Config b;
    config_load_from(&b, TESTKEY);
    EXPECT(strcmp(b.text, "Ola Mundo") == 0);
    EXPECT(wcscmp(b.font_family, L"Arial") == 0);
    EXPECT(b.font_bold == 0 && b.font_italic == 1);
    EXPECT(nearf(b.depth, 0.55f));
    EXPECT(nearf(b.max_angle_y, 30.0f));
    EXPECT(nearf(b.period, 6.5f));
    EXPECT(nearf(b.base_b, 0.90f));

    /* valor ausente -> default; valor fora de faixa -> clamp */
    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);
    HKEY k;
    RegCreateKeyExW(HKEY_CURRENT_USER, TESTKEY, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL);
    RegSetValueExW(k, L"depth", 0, REG_SZ, (const BYTE *)L"999", 4 * sizeof(wchar_t));
    RegSetValueExW(k, L"period", 0, REG_SZ, (const BYTE *)L"lixo", 5 * sizeof(wchar_t));
    RegCloseKey(k);

    Config c;
    config_load_from(&c, TESTKEY);
    EXPECT(c.depth <= 2.0f && c.depth >= 0.02f);          /* clampado */
    EXPECT(nearf(c.period, 9.0f));                        /* lixo -> default */
    EXPECT(strcmp(c.text, "Model 3D Text") != 0);         /* text ausente -> default */
    EXPECT(strcmp(c.text, "Modern 3D Text") == 0);

    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);
}
```

- [ ] **Step 2: `build/tests/test_main.c`** — declarar e chamar `run_config_tests();`.

- [ ] **Step 3: `build/Makefile`** — `TEST_SRC += build/tests/test_config.c`; `TEST_UNITS += src/config.c`; garantir `-ladvapi32` no link do teste:
```make
test: build/obj/test/stb_impl.o $(LIBTESS_TEST_OBJ)
	$(CC) -std=c11 -Wall -Wextra $(TEST_INC) \
	  $(TEST_SRC) $(TEST_UNITS) $^ \
	  -o build/obj/run_tests.exe -lm -lgdi32 -luser32 -ladvapi32
	./build/obj/run_tests.exe
```

- [ ] **Step 4: Rodar — confirmar falha** (`undefined reference to 'config_defaults'`).

- [ ] **Step 5: Escrever `src/config.h`** — exatamente o bloco Interfaces.

- [ ] **Step 6: Escrever `src/config.c`**

```c
#include "config.h"
#include "util/log.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define KEY_MAIN L"Software\\Modern3DText"
#define CFG_VERSION 1

void config_defaults(Config *c)
{
    memset(c, 0, sizeof *c);
    c->version = CFG_VERSION;
    c->content_mode = CONTENT_TEXT;
    strcpy(c->text, "Modern 3D Text");
    wcscpy(c->font_family, L"Segoe UI");
    c->font_bold = 1;
    c->font_italic = 0;
    c->depth = 0.30f;
    c->max_angle_y = 42.0f;
    c->tilt_x = 8.0f;
    c->period = 9.0f;
    c->base_r = 0.72f; c->base_g = 0.74f; c->base_b = 0.78f;
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* ---- helpers de leitura de valor REG_SZ ---- */
static int reg_get_w(HKEY k, const wchar_t *name, wchar_t *out, int cch)
{
    DWORD type = 0, cb = (DWORD)(cch * (int)sizeof(wchar_t));
    if (RegQueryValueExW(k, name, NULL, &type, (BYTE *)out, &cb) != ERROR_SUCCESS || type != REG_SZ)
        return 0;
    out[cch - 1] = 0;
    return 1;
}
static int reg_get_f(HKEY k, const wchar_t *name, float *out)
{
    wchar_t buf[64];
    if (!reg_get_w(k, name, buf, 64)) return 0;
    wchar_t *end = NULL;
    double v = wcstod(buf, &end);
    if (end == buf) return 0;
    *out = (float)v;
    return 1;
}
static int reg_get_i(HKEY k, const wchar_t *name, int *out)
{
    float f;
    if (!reg_get_f(k, name, &f)) return 0;
    *out = (int)(f + 0.5f);
    return 1;
}

static void set_w(HKEY k, const wchar_t *name, const wchar_t *val)
{
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)val,
                   (DWORD)((wcslen(val) + 1) * sizeof(wchar_t)));
}
static void set_f(HKEY k, const wchar_t *name, float v)
{
    wchar_t buf[32];
    _snwprintf(buf, 32, L"%.4g", (double)v);
    buf[31] = 0;
    set_w(k, name, buf);
}

void config_load_from(Config *c, const wchar_t *subkey)
{
    config_defaults(c);

    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subkey, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return;   /* primeira execucao: tudo default */

    wchar_t wtext[512];
    if (reg_get_w(k, L"text", wtext, 512))
        WideCharToMultiByte(CP_UTF8, 0, wtext, -1, c->text, (int)sizeof c->text, NULL, NULL);
    reg_get_w(k, L"font_family", c->font_family, 64);
    reg_get_i(k, L"font_bold", &c->font_bold);
    reg_get_i(k, L"font_italic", &c->font_italic);
    reg_get_i(k, L"version", &c->version);
    { int m = 0; if (reg_get_i(k, L"content_mode", &m)) c->content_mode = CONTENT_TEXT; (void)m; }

    float f;
    if (reg_get_f(k, L"depth", &f))       c->depth = f;
    if (reg_get_f(k, L"max_angle_y", &f)) c->max_angle_y = f;
    if (reg_get_f(k, L"tilt_x", &f))      c->tilt_x = f;
    if (reg_get_f(k, L"period", &f))      c->period = f;

    wchar_t col[16];
    if (reg_get_w(k, L"base_color", col, 16) && col[0] == L'#' && wcslen(col) >= 7) {
        unsigned rgb = (unsigned)wcstoul(col + 1, NULL, 16);
        c->base_r = ((rgb >> 16) & 0xFF) / 255.0f;
        c->base_g = ((rgb >> 8) & 0xFF) / 255.0f;
        c->base_b = (rgb & 0xFF) / 255.0f;
    }
    RegCloseKey(k);

    /* clamps / saneamento */
    c->font_bold = c->font_bold ? 1 : 0;
    c->font_italic = c->font_italic ? 1 : 0;
    c->depth = clampf(c->depth, 0.02f, 2.0f);
    c->max_angle_y = clampf(c->max_angle_y, 5.0f, 170.0f);
    c->tilt_x = clampf(c->tilt_x, 0.0f, 30.0f);
    c->period = clampf(c->period, 2.0f, 30.0f);
    c->base_r = clampf(c->base_r, 0.0f, 1.0f);
    c->base_g = clampf(c->base_g, 0.0f, 1.0f);
    c->base_b = clampf(c->base_b, 0.0f, 1.0f);
    if (c->text[0] == 0) strcpy(c->text, "Modern 3D Text");
    if (c->font_family[0] == 0) wcscpy(c->font_family, L"Segoe UI");
}

void config_save_to(const Config *c, const wchar_t *subkey)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, subkey, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL)
        != ERROR_SUCCESS) {
        log_errorf("config: nao criou %ls", subkey);
        return;
    }

    wchar_t wtext[512];
    MultiByteToWideChar(CP_UTF8, 0, c->text, -1, wtext, 512);
    wtext[511] = 0;
    set_w(k, L"text", wtext);
    set_w(k, L"font_family", c->font_family);
    set_f(k, L"font_bold", (float)c->font_bold);
    set_f(k, L"font_italic", (float)c->font_italic);
    set_f(k, L"version", (float)CFG_VERSION);
    set_f(k, L"content_mode", (float)c->content_mode);
    set_f(k, L"depth", c->depth);
    set_f(k, L"max_angle_y", c->max_angle_y);
    set_f(k, L"tilt_x", c->tilt_x);
    set_f(k, L"period", c->period);

    wchar_t col[16];
    unsigned r = (unsigned)(c->base_r * 255.0f + 0.5f);
    unsigned g = (unsigned)(c->base_g * 255.0f + 0.5f);
    unsigned b = (unsigned)(c->base_b * 255.0f + 0.5f);
    _snwprintf(col, 16, L"#%02X%02X%02X", r & 0xFF, g & 0xFF, b & 0xFF);
    col[15] = 0;
    set_w(k, L"base_color", col);

    RegCloseKey(k);
}

void config_load(Config *c) { config_load_from(c, KEY_MAIN); }
void config_save(const Config *c) { config_save_to(c, KEY_MAIN); }
```

- [ ] **Step 7: Rodar — passa** (`all tests passed`).

- [ ] **Step 8: Commit**

```sh
git add src/config.h src/config.c build/tests/test_config.c build/tests/test_main.c build/Makefile
git commit -m "$(printf 'feat: Config struct + HKCU registry load/save (human-readable REG_SZ)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 2: `scene` lê `Config` (sem `#define SC_*`)

**Files:**
- Modify: `src/scene.h`, `src/scene.c`
- Modify: `src/host_win32.c` (carrega `Config` no startup, passa adiante)

**Interfaces:**
- Consumes: `Config` (Task 1).
- Produces:
  ```c
  SceneRenderer *scene_create(const Config *cfg);              /* NULL em falha */
  void scene_set_config(SceneRenderer *s, const Config *cfg);  /* reconstroi malha so se preciso */
  void scene_render(SceneRenderer *s, double t, int fb_w, int fb_h);
  void scene_destroy(SceneRenderer *s);
  ```

- [ ] **Step 1: `src/scene.h`** — trocar `scene_create(void)` pela assinatura acima; `#include "config.h"`.

- [ ] **Step 2: `src/scene.c`** — remover todos os `#define SC_*`. `struct SceneRenderer` ganha um snapshot dos campos que afetam a malha:
```c
struct SceneRenderer {
    Material mat;
    GlMesh   mesh;
    float    hx, hy, hz;
    /* config corrente */
    char     text[512];
    wchar_t  font_family[64];
    int      bold, italic;
    float    depth;
    float    max_angle_y, tilt_x, period;
    v3       base_color;
    int      have_mesh;
};

static int rebuild_mesh(SceneRenderer *s)   /* usa s->text/font_family/bold/italic/depth */
{
    ContourSet cs;
    if (!font_build_contours(s->text, s->font_family, s->bold, s->italic, 0.004f, &cs))
        return 0;
    MeshData md;
    int ok = contour_mesh_build(&cs, (MeshParams){ s->depth }, &md);
    contourset_free(&cs);
    if (!ok) return 0;
    if (s->have_mesh) gl_mesh_free(&s->mesh);
    s->mesh = gl_mesh_upload(md.verts, md.nverts, md.idx, md.nidx);
    s->hx = 0.5f * (md.maxx - md.minx);
    s->hy = 0.5f * (md.maxy - md.miny);
    s->hz = 0.5f * (md.maxz - md.minz);
    if (s->hx < 1e-3f) s->hx = 1.0f;
    if (s->hy < 1e-3f) s->hy = 1.0f;
    mesh_data_free(&md);
    s->have_mesh = 1;
    return 1;
}

SceneRenderer *scene_create(const Config *cfg)
{
    SceneRenderer *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    if (!material_init(&s->mat)) { free(s); return NULL; }
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    scene_set_config(s, cfg);
    if (!s->have_mesh) { material_destroy(&s->mat); free(s); return NULL; }
    return s;
}

void scene_set_config(SceneRenderer *s, const Config *cfg)
{
    int mesh_dirty = !s->have_mesh
        || strcmp(s->text, cfg->text) != 0
        || wcscmp(s->font_family, cfg->font_family) != 0
        || s->bold != cfg->font_bold || s->italic != cfg->font_italic
        || s->depth != cfg->depth;

    strcpy(s->text, cfg->text);
    wcscpy(s->font_family, cfg->font_family);
    s->bold = cfg->font_bold; s->italic = cfg->font_italic;
    s->depth = cfg->depth;
    s->max_angle_y = cfg->max_angle_y;
    s->tilt_x = cfg->tilt_x;
    s->period = cfg->period;
    s->base_color = (v3){ cfg->base_r, cfg->base_g, cfg->base_b };

    if (mesh_dirty) {
        if (!rebuild_mesh(s))
            log_errorf("scene: rebuild_mesh falhou (text='%s')", s->text);
    }
}
```
`scene_render` usa `s->max_angle_y`, `s->tilt_x`, `s->period`, `s->base_color` no lugar dos antigos `SC_*`. Guardar contra `!s->have_mesh` (não desenha).

- [ ] **Step 3: `src/host_win32.c`** — no topo de `host_run_saver` e `host_run_preview`:
```c
    Config cfg;
    config_load(&cfg);
```
e passar `&cfg` para `gl_window_create`. Como `gl_window_create` é `static` aqui (ainda não extraído), adicionar um parâmetro `const Config *cfg` e usar em `g->scene = scene_create(cfg);`. `#include "config.h"`.

- [ ] **Step 4: Build + verificação headless**

```sh
mingw32-make -f build/Makefile
# grava um Config de teste na chave REAL e confirma que a cena muda
powershell -NoProfile -Command "New-Item -Path 'HKCU:\Software\Modern3DText' -Force | Out-Null; \
  Set-ItemProperty -Path 'HKCU:\Software\Modern3DText' -Name text -Value 'ProArt'; \
  Set-ItemProperty -Path 'HKCU:\Software\Modern3DText' -Name font_family -Value 'Arial'; \
  Set-ItemProperty -Path 'HKCU:\Software\Modern3DText' -Name depth -Value '0.6'"
cp dist/Modern3DText.scr "$TEMP/m.exe"
M3DT_SELFTEST=1 M3DT_SHOT_T=2.25 M3DT_SHOT="$TEMP/cfg.png" "$TEMP/m.exe" //s
# abrir cfg.png: deve mostrar "ProArt" em Arial, mais profundo
powershell -NoProfile -Command "Remove-Item 'HKCU:\Software\Modern3DText' -Recurse -Force"
```
Esperado: o PNG mostra "ProArt" (não "Modern 3D Text"), confirmando que a cena lê o registro.

- [ ] **Step 5: Testes unitários** (regressão) — `mingw32-make -f build/Makefile test` → `all tests passed`.

- [ ] **Step 6: Commit**

```sh
git add src/scene.h src/scene.c src/host_win32.c
git commit -m "$(printf 'feat: scene reads Config; mesh rebuilds only when text/font/depth change\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 3: Diálogo — tab control + aba Conteúdo

**Files:**
- Modify: `src/resource.h` (IDs)
- Modify: `res/screensaver.rc` (diálogo principal + `IDD_TAB_CONTENT`)
- Modify: `src/config_dialog.c` (reescrito)

**Interfaces:**
- Consumes: `config_load`/`config_save`, `Config`.
- Produces: `config_dialog_run(HINSTANCE, HWND parent)` — carrega `Config`, mostra o diálogo com abas, e no `OK`/`Aplicar` grava; `Cancelar`/`Esc`/`X` descartam.

- [ ] **Step 1: `src/resource.h`**

```c
#ifndef M3DT_RESOURCE_H
#define M3DT_RESOURCE_H

#define IDD_CONFIG        101
#define IDD_TAB_CONTENT   110
#define IDD_TAB_MOTION    111

#define IDC_TABS          1000
#define IDC_PREVIEW       1001
#define IDC_APPLY         1002

/* aba Conteudo */
#define IDC_TEXT          1100
#define IDC_FONT          1101
#define IDC_BOLD          1102
#define IDC_ITALIC        1103
#define IDC_COLOR         1104

/* aba Movimento */
#define IDC_DEPTH         1200
#define IDC_DEPTH_VAL     1201
#define IDC_ANGLE         1202
#define IDC_ANGLE_VAL     1203
#define IDC_TILT          1204
#define IDC_TILT_VAL      1205
#define IDC_PERIOD        1206
#define IDC_PERIOD_VAL    1207

#endif
```

- [ ] **Step 2: `res/screensaver.rc`** — substituir o `IDD_CONFIG` da Fase 1 por:

```rc
IDD_CONFIG DIALOGEX 0, 0, 420, 240
STYLE DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Modern 3D Text"
FONT 9, "Segoe UI", 400, 0, 0x1
BEGIN
    CONTROL         "", IDC_TABS, "SysTabControl32", WS_TABSTOP, 6, 6, 220, 228
    CONTROL         "", IDC_PREVIEW, "Static", SS_BLACKFRAME, 234, 20, 180, 180
    DEFPUSHBUTTON   "OK",       IDOK,     234, 208, 56, 14
    PUSHBUTTON      "Cancelar", IDCANCEL, 294, 208, 56, 14
    PUSHBUTTON      "Aplicar",  IDC_APPLY, 356, 208, 56, 14
END

IDD_TAB_CONTENT DIALOGEX 0, 0, 208, 200
STYLE DS_SETFONT | DS_CONTROL | WS_CHILD
FONT 9, "Segoe UI", 400, 0, 0x1
BEGIN
    LTEXT           "Texto:", -1, 8, 10, 60, 9
    EDITTEXT        IDC_TEXT, 8, 22, 192, 40, ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL
    LTEXT           "Fonte:", -1, 8, 72, 60, 9
    COMBOBOX        IDC_FONT, 8, 84, 192, 120, CBS_DROPDOWNLIST | CBS_SORT | WS_VSCROLL | WS_TABSTOP
    AUTOCHECKBOX    "Negrito",  IDC_BOLD,   8, 106, 60, 12
    AUTOCHECKBOX    "Itálico",  IDC_ITALIC, 76, 106, 60, 12
    LTEXT           "Cor:", -1, 8, 128, 60, 9
    PUSHBUTTON      "Escolher cor...", IDC_COLOR, 8, 140, 90, 14
END
```
(A aba Movimento entra no Task 4; deixar `IDD_TAB_MOTION` de fora por ora.)

- [ ] **Step 3: `src/config_dialog.c`** — reescrever:

```c
#include "config_dialog.h"
#include "resource.h"
#include "config.h"
#include "util/log.h"

#include <windows.h>
#include <commctrl.h>
#include <stdbool.h>
#include <string.h>

static Config  g_work;          /* config sendo editada */
static HWND    g_content;       /* sub-dialogo da aba Conteudo */
static bool    g_selftest;

static bool env_selftest(void)
{
    char b[8];
    DWORD k = GetEnvironmentVariableA("M3DT_SELFTEST", b, sizeof b);
    return k > 0 && k < sizeof b && b[0] != '0';
}

/* ---- aba Conteudo ---- */
static INT_PTR CALLBACK content_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
        case WM_INITDIALOG: {
            wchar_t wtext[512];
            MultiByteToWideChar(CP_UTF8, 0, g_work.text, -1, wtext, 512);
            SetDlgItemTextW(h, IDC_TEXT, wtext);
            CheckDlgButton(h, IDC_BOLD, g_work.font_bold ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(h, IDC_ITALIC, g_work.font_italic ? BST_CHECKED : BST_UNCHECKED);

            /* popular fontes */
            HDC dc = GetDC(h);
            LOGFONTW lf; memset(&lf, 0, sizeof lf); lf.lfCharSet = DEFAULT_CHARSET;
            EnumFontFamiliesExW(dc, &lf,
                (FONTENUMPROCW)+[](const LOGFONTW *lfp, const TEXTMETRICW *tm, DWORD type, LPARAM lp) -> int {
                    (void)tm; (void)type;
                    HWND cb = (HWND)lp;
                    if (lfp->lfFaceName[0] != L'@')
                        SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)lfp->lfFaceName);
                    return 1;
                }, (LPARAM)GetDlgItem(h, IDC_FONT), 0);
            ReleaseDC(h, dc);
            SendDlgItemMessageW(h, IDC_FONT, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)g_work.font_family);
            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDC_TEXT:
                    if (HIWORD(w) == EN_CHANGE) {
                        wchar_t wtext[512];
                        GetDlgItemTextW(h, IDC_TEXT, wtext, 512);
                        WideCharToMultiByte(CP_UTF8, 0, wtext, -1, g_work.text, sizeof g_work.text, NULL, NULL);
                        PostMessageW(GetParent(GetParent(h)), WM_APP + 1, 0, 0);   /* preview dirty */
                    }
                    break;
                case IDC_FONT:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        int i = (int)SendDlgItemMessageW(h, IDC_FONT, CB_GETCURSEL, 0, 0);
                        if (i >= 0) {
                            SendDlgItemMessageW(h, IDC_FONT, CB_GETLBTEXT, i, (LPARAM)g_work.font_family);
                            PostMessageW(GetParent(GetParent(h)), WM_APP + 1, 0, 0);
                        }
                    }
                    break;
                case IDC_BOLD:
                    g_work.font_bold = IsDlgButtonChecked(h, IDC_BOLD) == BST_CHECKED;
                    PostMessageW(GetParent(GetParent(h)), WM_APP + 1, 0, 0);
                    break;
                case IDC_ITALIC:
                    g_work.font_italic = IsDlgButtonChecked(h, IDC_ITALIC) == BST_CHECKED;
                    PostMessageW(GetParent(GetParent(h)), WM_APP + 1, 0, 0);
                    break;
                case IDC_COLOR: {
                    static COLORREF custom[16];
                    CHOOSECOLORW cc; memset(&cc, 0, sizeof cc);
                    cc.lStructSize = sizeof cc;
                    cc.hwndOwner = h;
                    cc.lpCustColors = custom;
                    cc.rgbResult = RGB((int)(g_work.base_r*255), (int)(g_work.base_g*255), (int)(g_work.base_b*255));
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColorW(&cc)) {
                        g_work.base_r = GetRValue(cc.rgbResult) / 255.0f;
                        g_work.base_g = GetGValue(cc.rgbResult) / 255.0f;
                        g_work.base_b = GetBValue(cc.rgbResult) / 255.0f;
                        PostMessageW(GetParent(GetParent(h)), WM_APP + 1, 0, 0);
                    }
                    break;
                }
            }
            return TRUE;
    }
    return FALSE;
}
```

> **Nota sobre o lambda `+[]`:** MinGW/gcc em C **não** aceita lambdas. Trocar o `EnumFontFamiliesExW` por uma função nomeada de arquivo:
> ```c
> static int CALLBACK enum_fonts_cb(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM lp)
> {
>     (void)tm; (void)type;
>     if (lf->lfFaceName[0] != L'@')
>         SendMessageW((HWND)lp, CB_ADDSTRING, 0, (LPARAM)lf->lfFaceName);
>     return 1;
> }
> ```
> e `EnumFontFamiliesExW(dc, &lf, enum_fonts_cb, (LPARAM)GetDlgItem(h, IDC_FONT), 0);`

```c
/* ---- dialogo principal ---- */
static INT_PTR CALLBACK dlg_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
        case WM_INITDIALOG: {
            HWND tabs = GetDlgItem(h, IDC_TABS);
            TCITEMW ti; memset(&ti, 0, sizeof ti);
            ti.mask = TCIF_TEXT;
            ti.pszText = L"Conteúdo";
            TabCtrl_InsertItem(tabs, 0, &ti);
            ti.pszText = L"Movimento";
            TabCtrl_InsertItem(tabs, 1, &ti);

            RECT rc; GetClientRect(tabs, &rc);
            TabCtrl_AdjustRect(tabs, FALSE, &rc);
            MapWindowPoints(tabs, h, (POINT *)&rc, 2);

            g_content = CreateDialogW(GetModuleHandleW(NULL),
                                      MAKEINTRESOURCEW(IDD_TAB_CONTENT), h, content_proc);
            SetWindowPos(g_content, HWND_TOP, rc.left, rc.top,
                         rc.right - rc.left, rc.bottom - rc.top, SWP_SHOWWINDOW);

            if (g_selftest) SetTimer(h, 1, 700, NULL);
            return TRUE;
        }
        case WM_TIMER:
            if (w == 1) { KillTimer(h, 1); EndDialog(h, IDCANCEL); return TRUE; }
            break;
        case WM_NOTIFY:
            if (((LPNMHDR)l)->idFrom == IDC_TABS && ((LPNMHDR)l)->code == TCN_SELCHANGE) {
                int sel = TabCtrl_GetCurSel(GetDlgItem(h, IDC_TABS));
                ShowWindow(g_content, sel == 0 ? SW_SHOW : SW_HIDE);
                /* aba Movimento: Task 4 */
                return TRUE;
            }
            break;
        case WM_APP + 1:
            /* preview dirty — Task 5 conecta ao mini-preview */
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDOK:      config_save(&g_work); EndDialog(h, IDOK); return TRUE;
                case IDC_APPLY: config_save(&g_work); return TRUE;
                case IDCANCEL:  EndDialog(h, IDCANCEL); return TRUE;
            }
            break;
        case WM_CLOSE:
            EndDialog(h, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

int config_dialog_run(HINSTANCE hInst, HWND parent)
{
    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES };
    InitCommonControlsEx(&icc);

    config_load(&g_work);
    g_selftest = env_selftest();

    HWND owner = IsWindow(parent) ? parent : NULL;
    DialogBoxParamW(hInst, MAKEINTRESOURCEW(IDD_CONFIG), owner, dlg_proc, 0);
    return 0;
}
```

- [ ] **Step 4: Build + verificação headless**

```sh
mingw32-make -f build/Makefile debug
cp dist/Modern3DText.scr "$TEMP/m.exe"
powershell -NoProfile -Command "Remove-Item 'HKCU:\Software\Modern3DText' -Recurse -Force -EA SilentlyContinue"
M3DT_SELFTEST=1 "$TEMP/m.exe" //c ; echo "exit=$?"
cat "$LOCALAPPDATA/Modern3DText/log.txt" | tail -5
```
Esperado: `exit=0`; o diálogo abre (aba Conteúdo com campo de texto, combo de fontes, checkboxes, botão de cor) e fecha sozinho pelo timer do selftest.

- [ ] **Step 5: Verificação manual** (rápida, interativa): `Modern3DText.scr /c` — digitar um texto, escolher fonte, marcar Negrito, `OK`. Rodar `regedit` → `HKCU\Software\Modern3DText` tem `text`, `font_family`, `font_bold` atualizados.

- [ ] **Step 6: Testes unitários** (regressão).

- [ ] **Step 7: Commit**

```sh
git add src/resource.h res/screensaver.rc src/config_dialog.c
git commit -m "$(printf 'feat: config dialog with tab control and Content tab (text/font/color)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 4: Aba Movimento (sliders)

**Files:**
- Modify: `res/screensaver.rc` (`IDD_TAB_MOTION`)
- Modify: `src/config_dialog.c` (segunda sub-janela + `WM_HSCROLL`)

**Interfaces:**
- Consumes: `g_work` (Task 3).
- Produces: aba **Movimento** com trackbars para `depth`, `max_angle_y`, `tilt_x`, `period`, cada um com rótulo numérico ao vivo; mexer grava em `g_work` e dispara `WM_APP+1`.

- [ ] **Step 1: `res/screensaver.rc` — `IDD_TAB_MOTION`**

```rc
IDD_TAB_MOTION DIALOGEX 0, 0, 208, 200
STYLE DS_SETFONT | DS_CONTROL | WS_CHILD
FONT 9, "Segoe UI", 400, 0, 0x1
BEGIN
    LTEXT    "Profundidade:", -1, 8, 10, 90, 9
    LTEXT    "", IDC_DEPTH_VAL, 150, 10, 50, 9
    CONTROL  "", IDC_DEPTH, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 20, 192, 18
    LTEXT    "Ângulo máx. (graus):", -1, 8, 44, 100, 9
    LTEXT    "", IDC_ANGLE_VAL, 150, 44, 50, 9
    CONTROL  "", IDC_ANGLE, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 54, 192, 18
    LTEXT    "Inclinação X (graus):", -1, 8, 78, 100, 9
    LTEXT    "", IDC_TILT_VAL, 150, 78, 50, 9
    CONTROL  "", IDC_TILT, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 88, 192, 18
    LTEXT    "Período (s):", -1, 8, 112, 90, 9
    LTEXT    "", IDC_PERIOD_VAL, 150, 112, 50, 9
    CONTROL  "", IDC_PERIOD, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 122, 192, 18
END
```

- [ ] **Step 2: `src/config_dialog.c` — sub-janela Movimento**

Adicionar `static HWND g_motion;` e uma `motion_proc`. Os trackbars usam inteiros; mapear:
- `depth`: 2..200 → `/100` (0.02 .. 2.00)
- `max_angle_y`: 5..170
- `tilt_x`: 0..30
- `period`: 20..300 → `/10` (2.0 .. 30.0)

```c
static void set_slider(HWND dlg, int id, int lo, int hi, int pos)
{
    SendDlgItemMessageW(dlg, id, TBM_SETRANGE, TRUE, MAKELPARAM(lo, hi));
    SendDlgItemMessageW(dlg, id, TBM_SETPOS, TRUE, pos);
}
static void motion_labels(HWND h)
{
    wchar_t b[32];
    _snwprintf(b, 32, L"%.2f", g_work.depth);        SetDlgItemTextW(h, IDC_DEPTH_VAL, b);
    _snwprintf(b, 32, L"%.0f", g_work.max_angle_y);  SetDlgItemTextW(h, IDC_ANGLE_VAL, b);
    _snwprintf(b, 32, L"%.0f", g_work.tilt_x);       SetDlgItemTextW(h, IDC_TILT_VAL, b);
    _snwprintf(b, 32, L"%.1f", g_work.period);       SetDlgItemTextW(h, IDC_PERIOD_VAL, b);
}

static INT_PTR CALLBACK motion_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)w; (void)l;
    switch (m) {
        case WM_INITDIALOG:
            set_slider(h, IDC_DEPTH,  2, 200, (int)(g_work.depth * 100.0f + 0.5f));
            set_slider(h, IDC_ANGLE,  5, 170, (int)(g_work.max_angle_y + 0.5f));
            set_slider(h, IDC_TILT,   0, 30,  (int)(g_work.tilt_x + 0.5f));
            set_slider(h, IDC_PERIOD, 20, 300, (int)(g_work.period * 10.0f + 0.5f));
            motion_labels(h);
            return TRUE;
        case WM_HSCROLL: {
            g_work.depth       = (float)SendDlgItemMessageW(h, IDC_DEPTH, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.max_angle_y = (float)SendDlgItemMessageW(h, IDC_ANGLE, TBM_GETPOS, 0, 0);
            g_work.tilt_x      = (float)SendDlgItemMessageW(h, IDC_TILT, TBM_GETPOS, 0, 0);
            g_work.period      = (float)SendDlgItemMessageW(h, IDC_PERIOD, TBM_GETPOS, 0, 0) / 10.0f;
            motion_labels(h);
            PostMessageW(GetParent(GetParent(h)), WM_APP + 1, 0, 0);
            return TRUE;
        }
    }
    return FALSE;
}
```

No `dlg_proc` `WM_INITDIALOG`, criar também `g_motion` (posicionado igual a `g_content`, começando escondido). No `TCN_SELCHANGE`, alternar visibilidade das duas sub-janelas.

- [ ] **Step 3: Build + selftest headless** — `M3DT_SELFTEST=1 ...scr /c` fecha sozinho, `exit=0`, log limpo.

- [ ] **Step 4: Verificação manual** — `/c`, aba Movimento, arrastar cada slider (rótulo numérico acompanha), `OK`, conferir os valores em `regedit`.

- [ ] **Step 5: Testes** (regressão).

- [ ] **Step 6: Commit**

```sh
git add res/screensaver.rc src/config_dialog.c src/resource.h
git commit -m "$(printf 'feat: config dialog Motion tab (depth/angle/tilt/period sliders)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 5: Extrair `gl_window` + mini-preview 3D ao vivo

**Files:**
- Create: `src/gl_window.h`, `src/gl_window.c` (extraído de `host_win32.c`)
- Modify: `src/host_win32.c` (usa `gl_window`)
- Modify: `src/config_dialog.c` (mini-preview no `IDC_PREVIEW`)
- Modify: `build/Makefile` (`SRC_C += src/gl_window.c`)

**Interfaces:**
- Produces:
  ```c
  /* gl_window.h */
  #include <windows.h>
  #include "config.h"
  typedef struct GlWindow GlWindow;

  void       gl_window_global_init(HINSTANCE hInst);   /* DPI + WGL bootstrap, idempotente */
  GlWindow  *gl_window_create(HINSTANCE hInst, DWORD style, DWORD exstyle, HWND parent,
                              int x, int y, int w, int h, const wchar_t *cls, WNDPROC proc,
                              const Config *cfg);
  void       gl_window_frame(GlWindow *g, double t);
  void       gl_window_set_config(GlWindow *g, const Config *cfg);
  void       gl_window_size(GlWindow *g, int *w, int *h);
  HWND       gl_window_hwnd(GlWindow *g);
  HDC        gl_window_dc(GlWindow *g);
  HGLRC      gl_window_rc(GlWindow *g);
  void       gl_window_destroy(GlWindow *g);
  ```
  `GlWindow` passa a ser **opaco** (ponteiro alocado); `host_win32.c` guarda `GlWindow*` em vez do struct por valor.

- [ ] **Step 1: Criar `src/gl_window.h`** conforme acima.

- [ ] **Step 2: Criar `src/gl_window.c`** — mover de `host_win32.c`: as constantes WGL ARB, os `PFN_*` e ponteiros, `m3dt_set_dpi_aware`, `m3dt_wgl_bootstrap` (viram `gl_window_global_init`), o `struct GlWindow` (agora com `SceneRenderer *scene`), `gl_window_create` (aloca com `calloc`, chama `scene_create(cfg)`), `gl_window_frame`, `gl_window_destroy`, `gl_window_set_config` (chama `scene_set_config`). Getters triviais.

- [ ] **Step 3: `src/host_win32.c`** — remover o que foi movido; `#include "gl_window.h"`. `GlWindow *win[16]` (ponteiros). Trocar:
  - `gl_window_create(hInst, &win[nwin], style, ...)` → `win[nwin] = gl_window_create(hInst, style, ..., &cfg)` e checar `!= NULL`.
  - `SetWindowPos(win[nwin].hwnd, ...)` → `SetWindowPos(gl_window_hwnd(win[nwin]), ...)`.
  - `gl_window_frame(&win[i], t)` → `gl_window_frame(win[i], t)`.
  - `win[0].dc` / `win[0].w` no bloco `M3DT_SHOT` → `gl_window_dc(win[0])` / `gl_window_size(win[0], &W, &H)`.
  - `gl_window_destroy(&win[i])` → `gl_window_destroy(win[i])`.
  - `m3dt_set_dpi_aware(); m3dt_wgl_bootstrap(hInst);` → `gl_window_global_init(hInst);`.
  - No preview: idem.

- [ ] **Step 4: Build + rodar os selftests da Fase 2a** (garantir que a extração não regrediu):
```sh
mingw32-make -f build/Makefile
cp dist/Modern3DText.scr "$TEMP/m.exe"
M3DT_SELFTEST=1 M3DT_SHOT_T=2.25 M3DT_SHOT="$TEMP/x.png" "$TEMP/m.exe" //s   # PNG com o texto 3D
```
Esperado: PNG idêntico ao da Fase 2a (texto 3D, sem glError).

- [ ] **Step 5: `src/config_dialog.c` — mini-preview**

```c
static GlWindow *g_preview;

/* no dlg_proc: */
case WM_INITDIALOG:
    ...
    gl_window_global_init(GetModuleHandleW(NULL));
    {
        HWND ph = GetDlgItem(h, IDC_PREVIEW);
        RECT pr; GetClientRect(ph, &pr);
        g_preview = gl_window_create(GetModuleHandleW(NULL), WS_CHILD | WS_VISIBLE, 0, ph,
                                     0, 0, pr.right, pr.bottom, L"M3DTCfgPreview",
                                     DefWindowProcW, &g_work);
    }
    SetTimer(h, 2, 33, NULL);              /* ~30 fps */
    g_dirty = false;
    QueryPerformanceCounter(&g_pstart);
    return TRUE;

case WM_TIMER:
    if (w == 2 && g_preview) {
        if (g_dirty) { gl_window_set_config(g_preview, &g_work); g_dirty = false; }
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        double t = (double)(now.QuadPart - g_pstart.QuadPart) / (double)g_pfreq.QuadPart;
        gl_window_frame(g_preview, t);
        return TRUE;
    }
    if (w == 1) { KillTimer(h, 1); EndDialog(h, IDCANCEL); return TRUE; }
    break;

case WM_APP + 1:                            /* algum controle mudou */
    g_dirty = true;                          /* debounce natural: aplica no proximo tick */
    return TRUE;

/* antes de EndDialog em IDOK/IDCANCEL/WM_CLOSE: */
    if (g_preview) { KillTimer(h, 2); gl_window_destroy(g_preview); g_preview = NULL; }
```
(declarar `static bool g_dirty; static LARGE_INTEGER g_pstart, g_pfreq;` e `QueryPerformanceFrequency(&g_pfreq)` em `config_dialog_run`).

- [ ] **Step 6: Build + verificação headless**

```sh
mingw32-make -f build/Makefile debug
cp dist/Modern3DText.scr "$TEMP/m.exe"
powershell -NoProfile -Command "Remove-Item 'HKCU:\Software\Modern3DText' -Recurse -Force -EA SilentlyContinue"
M3DT_SELFTEST=1 "$TEMP/m.exe" //c ; echo "exit=$?"
grep -E "glError|scene_create|scene: mesh" "$LOCALAPPDATA/Modern3DText/log.txt" | tail -5
```
Esperado: `exit=0`; log mostra `scene: mesh ...` (o preview criou uma cena) e **sem** `glError`. Sem processo órfão.

- [ ] **Step 7: Verificação manual** — `/c`: o retângulo à direita mostra o texto 3D girando; digitar outro texto / arrastar o slider de profundidade → o preview atualiza em ~0,1 s. `Cancelar` fecha sem travar; abrir de novo mostra o valor salvo anteriormente (ou o default).

- [ ] **Step 8: Testes** (regressão) + **estabilidade**: abrir/fechar o `/c` 10× seguidas (via selftest num loop) — sem vazamento de handle/contexto.

- [ ] **Step 9: Commit**

```sh
git add src/gl_window.h src/gl_window.c src/host_win32.c src/config_dialog.c build/Makefile
git commit -m "$(printf 'feat: extract gl_window module; live 3D mini-preview in the config dialog\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 6: Integração, QA da Fase 2b, tag

**Files:**
- Create: `docs/qa-checklist-phase-2b.md`
- Modify: `README.md`

- [ ] **Step 1: `make` / `make debug` / `make test` verdes de ponta a ponta**; `.scr` < 3 MB.

- [ ] **Step 2: `docs/qa-checklist-phase-2b.md`**

```markdown
# QA — Fase 2b (configuração)

## Headless (máquina de dev)
- [ ] `make`, `make debug`, `make test` verdes, sem warnings; `.scr` < 3 MB.
- [ ] `test_config`: defaults, round-trip real no registro (`_test`), clamp de
      valor fora de faixa, valor inválido -> default.
- [ ] Gravar um Config na chave real (PowerShell) e rodar `/s` com `M3DT_SHOT`:
      o PNG reflete o texto/fonte/profundidade salvos.
- [ ] `M3DT_SELFTEST=1 ...scr /c`: diálogo abre e fecha sozinho, `exit=0`,
      log com `scene: mesh` do preview e sem `glError`.

## Interativo
- [ ] `/c` pelo painel do Windows: abas Conteúdo e Movimento; todos os controles.
- [ ] Digitar texto / trocar fonte / negrito / itálico / cor → mini-preview atualiza.
- [ ] Sliders de profundidade / ângulo / inclinação / período → preview e rótulos.
- [ ] `OK` grava em `HKCU\Software\Modern3DText`; `Aplicar` grava sem fechar;
      `Cancelar` descarta.
- [ ] Reabrir `/c` mostra os valores salvos.
- [ ] `/s` (botão Visualizar) usa a config salva; muda de verdade ao salvar outra.
- [ ] 100 / 150 / 200 % DPI: layout sem corte.
- [ ] Abrir/fechar `/c` 10× — memória e handles estáveis.
```

- [ ] **Step 3: `README.md`** — status: "Fase 2b — configuração pelo registro + diálogo Win32 com abas Conteúdo/Movimento e mini-preview 3D ao vivo".

- [ ] **Step 4: Rodar o checklist interativo.**

- [ ] **Step 5: Commit + tag**

```sh
git add docs/qa-checklist-phase-2b.md README.md
git commit -m "$(printf 'feat: phase 2b - config dialog drives the screensaver\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
git tag -a v0.2.1-phase2b -m "Fase 2b: Config no registro + diálogo com abas + mini-preview ao vivo"
```

---

## Self-Review

**1. Cobertura (recorte da Fase 2b — a parte de config do spec §17 item 2 + §10):**
- `Config` struct + registro (§10.1, §10.2) → Task 1. Subconjunto de campos: só os que a Fase 2a usa + `content_mode`/`version` para migração futura. Campos de SVG/mesh/clock/efeitos/etc. entram quando as features chegarem. ✓
- `scene` desacoplado dos `#define` → Task 2. ✓
- Diálogo `DIALOGEX` com `SysTabControl32`, abas **Conteúdo** e **Movimento** (§10.3) → Tasks 3–4. As outras 4 abas do §10.3 (Material/Fundo/Efeitos/Desempenho) entram nas fases das respectivas features. ✓
- Pickers nativos: `EnumFontFamiliesExW` (fonte), `ChooseColorW` (cor), `msctls_trackbar32` (sliders) (§10.3) → Tasks 3–4. ✓
- Mini-preview ao vivo com o mesmo código de render (§10.3) → Task 5 (extração `gl_window` + preview no `IDC_PREVIEW`). ✓
- `OK`/`Aplicar`/`Cancelar` (§10.3) → Task 3. ✓
- **Fora da 2b:** import/export `.ini`, presets, i18n PT/EN, abas de features futuras. Anotado nas Global Constraints.

**2. Placeholders:** o `content_proc` mostra primeiro uma versão com lambda C++ e **em seguida a correção obrigatória** (função nomeada `enum_fonts_cb`) — a versão nomeada é a que vale. Sem "TODO"/"TBD". Verificações de GL/diálogo têm comando + resultado esperado; Task 2 e Task 5 produzem PNG conferível.

**3. Consistência de tipos:**
- `Config` — definido em `config.h` (Task 1), consumido por `scene_create`/`scene_set_config` (Task 2), `config_dialog.c` (`g_work`, Tasks 3–5), `gl_window_create`/`gl_window_set_config` (Task 5), `host_win32.c` (Task 2). Campos idênticos em todo lugar (`text` char[512] UTF-8, `font_family` wchar_t[64], floats). ✓
- `scene_create(const Config*)` / `scene_set_config(SceneRenderer*, const Config*)` — `scene.h` (Task 2), chamados por `gl_window.c` (Task 5). A Fase 2a chamava `scene_create(void)`; Task 2 Step 1 troca a assinatura e todos os call sites (só `gl_window`/`host` via a extração). ✓
- `GlWindow` passa de struct-por-valor (Fase 2a, em `host_win32.c`) para **ponteiro opaco** (`gl_window.h`, Task 5). Task 5 Step 3 lista cada call site de `host_win32.c` a converter (`.hwnd`→`gl_window_hwnd()`, `&win[i]`→`win[i]`, etc.). ✓
- IDs de `resource.h` (Task 3) — usados no `.rc` (Tasks 3–4) e em `config_dialog.c` (Tasks 3–5). `IDC_PREVIEW` (static frame no `.rc`) vira o parent do `gl_window` do preview (Task 5). ✓
- `WM_APP + 1` — postado pelas sub-janelas (Tasks 3–4) para `GetParent(GetParent(h))` (= diálogo principal), tratado no `dlg_proc` (Task 3 stub → Task 5 liga ao `g_dirty`). ✓
- Mapeamento slider↔float (Task 4): `depth` ×100 (2..200), `period` ×10 (20..300), `max_angle_y`/`tilt_x` diretos. As faixas batem com os clamps do `config_load` (Task 1: depth 0.02..2.0, angle 5..170, tilt 0..30, period 2..30). ✓

Sem inconsistências pendentes.

---

## Execution Handoff

**Plano completo e salvo em `docs/superpowers/plans/2026-09-10-modern-3d-text-phase-2b-config.md`. Duas opções de execução:**

**1. Subagent-Driven (recomendado)** — subagente novo por task, revisão entre tasks.
**2. Inline Execution** — nesta sessão, com checkpoints.

**Qual abordagem?**

> Como nas fases anteriores: build/headless aqui; o QA interativo do diálogo (abas, pickers, DPI, o preview atualizando ao vivo) fica pra você. Tasks 2 e 5 geram PNG que eu abro pra conferir.
