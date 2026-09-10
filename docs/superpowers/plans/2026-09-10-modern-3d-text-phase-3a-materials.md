# Modern 3D Text — Fase 3a: Materiais — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Dar ao usuário os 4 materiais do spec §7 — **especular clássico** (0, atual), **metálico** (1), **vidro** (2) e **fosco** (3) — com um **ambiente refletido** (procedural por padrão, imagem equiretangular opcional via `material.env_path`). É o que produz a pegada "Reflexo" cromada do Texto 3D clássico. Escolha e ajuste (metalness / roughness / imagem) numa aba **Material** no diálogo, com o mini-preview refletindo em tempo real.

**Architecture:** `model.frag` passa a ramificar em `uMode` (branch dinâmico de uniform — todos os fragmentos tomam o mesmo caminho). O ambiente é uma função procedural no shader (céu/chão + 2 "luzes" de estúdio); se `material.env_path` aponta para uma imagem, `env.c` carrega com `stb_image`, sobe como textura 2D com mipmaps e o shader amostra por coordenada equiretangular com LOD ∝ roughness. O modo **vidro** desenha num passe transparente simples (blend + depth-mask off, aceitando erro mínimo de ordenação num objeto quase convexo — o WBOIT do spec fica para a Fase 7). `material.c` ganha os uniforms novos; `scene.c` os alimenta a partir da `Config` e liga/desliga o blend conforme o modo.

**Tech Stack:** C11 · w64devkit · OpenGL 3.3 / GLSL 330 · `stb_image.h` (novo) · reuso de `config`/`scene`/`material`/`gl_core`/`gl_window`/`config_dialog`.

## Global Constraints

Do spec e do estado pós-Fase 2b. Todo task herda esta seção.

- **Linguagem:** C11. Flags `-std=c11 -municode -Wall -Wextra` (+ `-O2 -DNDEBUG` release). **Sem `-ffast-math`.** Build **sem warnings**.
- **Toolchain:** só w64devkit (`C:\Users\alanm\w64devkit`, `bin` no PATH).
- **`stb_image.h`** — baixado de `raw.githubusercontent.com/nothings/stb/master/stb_image.h`, adicionado a `src/geometry/stb_impl.c` (compilado com `-w`).
- **Sem asset embutido** de matcap/ambiente — o ambiente padrão é procedural (no shader). Imagem de ambiente é só a opção `material.env_path` do usuário.
- **Registro:** só `HKCU\Software\Modern3DText` (testes usam `_test`). Sem escrita fora disso e de `%LOCALAPPDATA%\Modern3DText\`.
- **`.scr` ≤ 3 MB.** i18n ainda não (Fase 8) — strings do diálogo em PT direto no `.rc`.
- **Commits frequentes.** TDD em `config` (round-trip dos campos novos). Shader/GL: captura PNG conferível (um por modo).
- **Atribuição:** todo commit termina com `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>`.

---

## File Structure

| Arquivo | Responsabilidade |
|---|---|
| `src/config.h` / `.c` | (+) `material_mode` (0..3), `metalness` (0..1), `roughness` (0..1), `env_path` (wchar[512]) |
| `shaders/model.frag` | (reescrito) ramifica em `uMode`; `proc_env()` + `sample_env()`; modos 1/2/3 |
| `src/material.h` / `.c` | (+) uniforms `uMode`/`uMetalness`/`uRoughness`/`uEnvTex`/`uHasEnv`; `material_set_style(...)` |
| `src/env.h` / `.c` | `env_load_texture(const wchar_t *path)` → id de textura GL (0 = nenhuma); `env_free` |
| `src/gl_core.h` / `.c` | (+) `gl_texture_2d_rgba(w, h, pixels, mipmaps)` helper |
| `src/scene.c` | alimenta os uniforms de material; carrega/troca a textura de ambiente; passe transparente no modo 2 |
| `third_party/stb_image.h` | baixado |
| `src/geometry/stb_impl.c` | (+) `#define STB_IMAGE_IMPLEMENTATION` |
| `res/screensaver.rc` | (+) `IDD_TAB_MATERIAL` |
| `src/resource.h` | (+) IDs da aba Material |
| `src/config_dialog.c` | (+) aba **Material** (combo de modo, 2 sliders, picker de imagem) |
| `build/tests/test_config.c` | (+) round-trip dos campos de material |
| `build/Makefile` | `src/env.c` em `SRC_C` |

---

## Task 1: Config — campos de material (TDD)

**Files:**
- Modify: `src/config.h`, `src/config.c`, `build/tests/test_config.c`

**Interfaces:**
- Produces: `Config` ganha
  ```c
  int      material_mode;   /* 0 classico, 1 metalico, 2 vidro, 3 fosco */
  float    metalness;       /* 0..1 */
  float    roughness;       /* 0..1 */
  wchar_t  env_path[512];   /* vazio = ambiente procedural */
  ```
  Padrões: `material_mode = 0`, `metalness = 0.9`, `roughness = 0.25`, `env_path = L""`.
  Chaves no registro: `material_mode`, `metalness`, `roughness`, `env_path` (REG_SZ).

- [ ] **Step 1: `build/tests/test_config.c`** — no round-trip, setar e conferir os 4 campos:
```c
    a.material_mode = 2;
    a.metalness = 0.4f;
    a.roughness = 0.7f;
    wcscpy(a.env_path, L"C:\\img\\studio.jpg");
    /* ... apos config_load_from(&b, TESTKEY): */
    EXPECT(b.material_mode == 2);
    EXPECT(nearf(b.metalness, 0.4f));
    EXPECT(nearf(b.roughness, 0.7f));
    EXPECT(wcscmp(b.env_path, L"C:\\img\\studio.jpg") == 0);
```
E no bloco de clamp: `material_mode` fora de 0..3 → 0; `metalness`/`roughness` → [0,1].

- [ ] **Step 2: `src/config.h`** — adicionar os 4 campos ao struct (depois de `base_b`).

- [ ] **Step 3: `src/config.c`**
- `config_defaults`: `c->material_mode = 0; c->metalness = 0.9f; c->roughness = 0.25f; c->env_path[0] = 0;`
- `config_load_from`: ler `material_mode` (via `reg_get_i`), `metalness`/`roughness` (via `reg_get_f`), `env_path` (via `reg_get_w(k, L"env_path", c->env_path, 512)`). Clamp: `if (c->material_mode < 0 || c->material_mode > 3) c->material_mode = 0;` e `clampf(metalness/roughness, 0, 1)`.
- `config_save_to`: `set_f(k, L"material_mode", (float)c->material_mode); set_f(k, L"metalness", c->metalness); set_f(k, L"roughness", c->roughness); set_w(k, L"env_path", c->env_path);`

- [ ] **Step 4: Rodar — falha depois passa** (`mingw32-make -f build/Makefile test`).

- [ ] **Step 5: Commit**
```sh
git add src/config.h src/config.c build/tests/test_config.c
git commit -m "$(printf 'feat: config material fields (mode/metalness/roughness/env_path)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 2: `model.frag` — 4 modos + ambiente procedural

**Files:**
- Modify: `shaders/model.frag`
- Modify: `src/material.h`, `src/material.c`
- Modify: `src/scene.c`

**Interfaces:**
- Produces:
  ```c
  /* material.h */
  typedef struct {
      unsigned prog;
      int uModel, uView, uProj, uCamPos, uBaseColor;
      int uMode, uMetalness, uRoughness, uEnvTex, uHasEnv;
  } Material;
  void material_begin(const Material *m, m4 view, m4 proj, v3 campos, v3 base);
  void material_set_style(const Material *m, int mode, float metalness, float roughness,
                          unsigned env_tex /* 0 = nenhuma */);
  void material_set_model(const Material *m, m4 model);
  ```

- [ ] **Step 1: Reescrever `shaders/model.frag`**

```glsl
#version 330 core
in vec3 vWorld;
in vec3 vNrm;
in float vSurf;

uniform vec3  uCamPos;
uniform vec3  uBaseColor;
uniform int   uMode;         // 0 classico, 1 metalico, 2 vidro, 3 fosco
uniform float uMetalness;    // 0..1
uniform float uRoughness;    // 0..1
uniform sampler2D uEnvTex;
uniform int   uHasEnv;       // 0/1

out vec4 fragColor;

const float PI = 3.14159265;
const vec3 KEY_DIR  = normalize(vec3(-0.40, -0.75, -0.55));
const vec3 FILL_DIR = normalize(vec3( 0.55,  0.30, -0.45));

vec3 proc_env(vec3 d)
{
    vec3 sky    = mix(vec3(0.10, 0.12, 0.17), vec3(0.50, 0.60, 0.82), clamp(d.y * 0.5 + 0.5, 0.0, 1.0));
    vec3 ground = vec3(0.06, 0.055, 0.05);
    vec3 col = mix(ground, sky, smoothstep(-0.15, 0.15, d.y));
    col += vec3(1.00, 0.96, 0.88) * pow(max(dot(d, normalize(vec3( 0.45, 0.55, 0.35))), 0.0), 40.0) * 4.0;
    col += vec3(0.35, 0.45, 0.65) * pow(max(dot(d, normalize(vec3(-0.55, 0.15, -0.30))), 0.0), 10.0) * 0.8;
    return col;
}

vec3 sample_env(vec3 d, float rough)
{
    vec3 e;
    if (uHasEnv == 1) {
        vec2 uv = vec2(atan(d.z, d.x) / (2.0 * PI) + 0.5, acos(clamp(d.y, -1.0, 1.0)) / PI);
        e = textureLod(uEnvTex, uv, rough * 6.0).rgb;
    } else {
        e = proc_env(d);
    }
    vec3 ambient = proc_env(vec3(0.0, 1.0, 0.0)) * 0.6 + proc_env(vec3(0.0, -1.0, 0.0)) * 0.4;
    return mix(e, ambient, rough * 0.6);
}

void main()
{
    vec3 N = normalize(vNrm);
    if (!gl_FrontFacing) N = -N;
    vec3 V = normalize(uCamPos - vWorld);
    vec3 R = reflect(-V, N);
    vec3 base = uBaseColor;
    float fres = pow(1.0 - max(dot(N, V), 0.0), 5.0);

    if (uMode == 1) {                         // metalico
        vec3 env  = sample_env(R, uRoughness);
        vec3 tint = mix(vec3(1.0), base, uMetalness);
        vec3 col  = env * tint;
        col += vec3(1.0) * fres * (0.15 + 0.85 * (1.0 - uRoughness));
        float kd = max(dot(N, -KEY_DIR), 0.0);
        col = mix(col, base * (0.2 + 0.8 * kd), (1.0 - uMetalness) * 0.5);
        if (vSurf > 1.5 && vSurf < 2.5) col *= 0.9;
        fragColor = vec4(col, 1.0);
        return;
    }
    if (uMode == 2) {                         // vidro (passe transparente)
        vec3 env  = sample_env(R, uRoughness * 0.5);
        vec3 refr = base * 0.6;
        float m   = clamp(fres + 0.15, 0.0, 1.0);
        vec3 col  = mix(refr, env, m);
        vec3 H = normalize(-KEY_DIR + V);
        col += vec3(1.0) * pow(max(dot(N, H), 0.0), 120.0);
        fragColor = vec4(col, mix(0.35, 0.95, m));
        return;
    }
    if (uMode == 3) {                         // fosco
        float w = dot(N, -KEY_DIR) * 0.5 + 0.5;
        vec3 col = base * (0.15 + 0.85 * w * w);
        col += base * max(dot(N, -FILL_DIR), 0.0) * 0.20;
        if (vSurf > 1.5) col *= 0.82;
        fragColor = vec4(col, 1.0);
        return;
    }

    /* uMode == 0: especular classico */
    float kd = max(dot(N, -KEY_DIR), 0.0);
    vec3 col = base * (0.12 + 0.88 * kd) * vec3(1.00, 0.96, 0.88);
    col += base * max(dot(N, -FILL_DIR), 0.0) * 0.30 * vec3(0.55, 0.62, 0.80);
    vec3 H = normalize(-KEY_DIR + V);
    col += vec3(1.0) * pow(max(dot(N, H), 0.0), 96.0) * 0.85;
    col += vec3(0.55, 0.68, 0.95) * pow(1.0 - max(dot(N, V), 0.0), 3.0) * 0.35;
    if (vSurf > 1.5 && vSurf < 2.5) col *= 0.92;
    fragColor = vec4(col, 1.0);
}
```

- [ ] **Step 2: `src/material.h` / `.c`**
- `Material` ganha `uMode, uMetalness, uRoughness, uEnvTex, uHasEnv`.
- `material_init`: `glGetUniformLocation` para cada; após `glUseProgram` numa inicialização, `glUniform1i(m->uEnvTex, 0)` (unidade de textura 0).
- Novo:
  ```c
  void material_set_style(const Material *m, int mode, float metalness, float roughness, unsigned env_tex)
  {
      glUseProgram(m->prog);
      glUniform1i(m->uMode, mode);
      glUniform1f(m->uMetalness, metalness);
      glUniform1f(m->uRoughness, roughness);
      glUniform1i(m->uHasEnv, env_tex ? 1 : 0);
      if (env_tex) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, env_tex); }
  }
  ```

- [ ] **Step 3: `src/scene.c`**
- `SceneRenderer` guarda `int material_mode; float metalness, roughness; unsigned env_tex; wchar_t env_path[512];`
- `scene_set_config`: copiar `material_mode`/`metalness`/`roughness`; se `cfg->env_path` mudou → `env_free(s->env_tex); s->env_tex = env_load_texture(cfg->env_path);` (Task 4 fornece `env_*`; até lá, `s->env_tex = 0`).
- `scene_render`: após `material_begin(...)`, `material_set_style(&s->mat, s->material_mode, s->metalness, s->roughness, s->env_tex);` antes do `gl_mesh_draw`.

- [ ] **Step 4: Build + captura de 4 PNGs**

```sh
mingw32-make -f build/Makefile
cp dist/Modern3DText.scr "$TEMP/m.exe"
for MODE in 0 1 2 3; do
  powershell -NoProfile -Command "New-Item 'HKCU:\Software\Modern3DText' -Force | Out-Null; \
    Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name material_mode -Value $MODE; \
    Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name text -Value 'ProArt'"
  M3DT_SELFTEST=1 M3DT_SHOT_T=1.6 M3DT_SHOT="$TEMP/mat$MODE.png" "$TEMP/m.exe" //s
done
powershell -NoProfile -Command "Remove-Item 'HKCU:\Software\Modern3DText' -Recurse -Force"
```
Abrir os 4 PNGs: **0** = como antes; **1** = cromado/metálico refletindo o ambiente procedural; **2** = translúcido com brilho de borda; **3** = fosco chapado. Sem `glError` no log.

- [ ] **Step 5: Testes** (regressão) + **Commit**
```sh
git add shaders/model.frag src/material.h src/material.c src/scene.c
git commit -m "$(printf 'feat: model shader branches on material mode; procedural env reflection\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 3: Passe transparente do vidro (modo 2)

**Files:**
- Modify: `src/scene.c`

**Interfaces:** nenhuma nova; `scene_render` passa a configurar o blend quando `material_mode == 2`.

- [ ] **Step 1: `src/scene.c` — `scene_render`**

Antes do `gl_mesh_draw`:
```c
    int glass = (s->material_mode == 2);
    if (glass) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }
```
Depois do `gl_mesh_draw`:
```c
    if (glass) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
```
O fundo (o `glClear` com a cor sólida) já dá o "atrás do vidro". Objeto quase convexo → erro de ordenação mínimo, aceito no v1 (WBOIT é Fase 7 — anotar no QA).

- [ ] **Step 2: Build + captura do modo 2** (comparar com o PNG do Task 2 — agora com transparência de verdade, dá pra ver a face de trás através da frente).

- [ ] **Step 3: Commit**
```sh
git add src/scene.c
git commit -m "$(printf 'feat: single-pass transparent draw for glass material\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 4: Imagem de ambiente opcional (`env.c` + stb_image)

**Files:**
- Create: `third_party/stb_image.h` (baixado)
- Modify: `src/geometry/stb_impl.c`
- Create: `src/env.h`, `src/env.c`
- Modify: `src/gl_core.h` / `.c` (helper de textura)
- Modify: `src/scene.c` (usa `env_load_texture`)
- Modify: `build/Makefile` (`src/env.c`)

**Interfaces:**
- Produces:
  ```c
  /* env.h */
  unsigned env_load_texture(const wchar_t *path);   /* 0 = sem imagem / falha */
  void     env_free(unsigned tex);
  /* gl_core.h */
  unsigned gl_texture_2d_rgb8(int w, int h, const unsigned char *rgb, int mipmaps);
  ```

- [ ] **Step 1: Baixar `stb_image.h`**
```sh
curl -sL -o third_party/stb_image.h https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
```

- [ ] **Step 2: `src/geometry/stb_impl.c`** — adicionar no topo:
```c
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
```

- [ ] **Step 3: `src/gl_core.c`** — helper:
```c
unsigned gl_texture_2d_rgb8(int w, int h, const unsigned char *rgb, int mipmaps)
{
    unsigned t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (mipmaps) glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}
```
(declarar em `gl_core.h`.)

- [ ] **Step 4: `src/env.c`**
```c
#include "env.h"
#include "gl_core.h"
#include "util/log.h"
#include <windows.h>
#include <glad/gl.h>
#include "stb_image.h"

unsigned env_load_texture(const wchar_t *path)
{
    if (!path || !path[0]) return 0;

    /* wchar -> utf8 para o stb_image */
    char u8[1024];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, u8, sizeof u8, NULL, NULL);

    int w = 0, h = 0, ch = 0;
    unsigned char *px = stbi_load(u8, &w, &h, &ch, 3);
    if (!px) { log_errorf("env: nao carregou %s", u8); return 0; }

    unsigned t = gl_texture_2d_rgb8(w, h, px, 1);
    stbi_image_free(px);
    log_infof("env: %s (%dx%d) -> tex %u", u8, w, h, t);
    return t;
}

void env_free(unsigned tex)
{
    if (tex) glDeleteTextures(1, &tex);
}
```

- [ ] **Step 5: `src/scene.c`** — `scene_set_config`: se `wcscmp(s->env_path, cfg->env_path) != 0` → `env_free(s->env_tex); wcscpy(s->env_path, cfg->env_path); s->env_tex = env_load_texture(cfg->env_path);`. `scene_destroy`: `env_free(s->env_tex)`.

- [ ] **Step 6: Build + verificação** — apontar `env_path` para um `.jpg`/`.png` qualquer (ex.: uma foto), capturar o modo 1; o reflexo deve mostrar a imagem enrolada em vez do céu procedural. Sem imagem → volta ao procedural.

- [ ] **Step 7: Commit**
```sh
git add third_party/stb_image.h src/geometry/stb_impl.c src/env.h src/env.c src/gl_core.h src/gl_core.c src/scene.c build/Makefile
git commit -m "$(printf 'feat: optional equirectangular environment image (stb_image)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 5: Aba Material no diálogo

**Files:**
- Modify: `src/resource.h`, `res/screensaver.rc`, `src/config_dialog.c`

**Interfaces:** aba **Material** — combo de modo (Clássico/Metálico/Vidro/Fosco), sliders **Metalização** e **Rugosidade**, botão **Imagem de ambiente...** + **Limpar** + rótulo com o nome do arquivo. Mexer → `g_work` + `WM_APP+1`.

- [ ] **Step 1: `src/resource.h`**
```c
#define IDD_TAB_MATERIAL  112
#define IDC_MATMODE       1300
#define IDC_METAL         1301
#define IDC_METAL_VAL     1302
#define IDC_ROUGH         1303
#define IDC_ROUGH_VAL     1304
#define IDC_ENVPATH       1305
#define IDC_ENVPICK       1306
#define IDC_ENVCLEAR      1307
```

- [ ] **Step 2: `res/screensaver.rc` — `IDD_TAB_MATERIAL`**
```rc
IDD_TAB_MATERIAL DIALOGEX 0, 0, 208, 196
STYLE DS_SETFONT | DS_CONTROL | WS_CHILD
FONT 9, "Segoe UI", 400, 0, 0x1
BEGIN
    LTEXT     "Material:", -1, 8, 10, 60, 9
    COMBOBOX  IDC_MATMODE, 8, 22, 150, 80, CBS_DROPDOWNLIST | WS_TABSTOP
    LTEXT     "Metalizacao:", -1, 8, 46, 90, 9
    LTEXT     "", IDC_METAL_VAL, 150, 46, 50, 9
    CONTROL   "", IDC_METAL, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 56, 192, 18
    LTEXT     "Rugosidade:", -1, 8, 80, 90, 9
    LTEXT     "", IDC_ROUGH_VAL, 150, 80, 50, 9
    CONTROL   "", IDC_ROUGH, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 90, 192, 18
    LTEXT     "Imagem de ambiente (opcional):", -1, 8, 116, 180, 9
    LTEXT     "(procedural)", IDC_ENVPATH, 8, 128, 192, 9, SS_PATHELLIPSIS
    PUSHBUTTON "Escolher...", IDC_ENVPICK, 8, 140, 60, 14
    PUSHBUTTON "Limpar",      IDC_ENVCLEAR, 72, 140, 50, 14
END
```

- [ ] **Step 3: `src/config_dialog.c` — `material_proc`**

```c
static HWND g_material;

static void material_labels(HWND h)
{
    wchar_t b[32];
    swprintf(b, 32, L"%.2f", (double)g_work.metalness);  SetDlgItemTextW(h, IDC_METAL_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.roughness);  SetDlgItemTextW(h, IDC_ROUGH_VAL, b);
    SetDlgItemTextW(h, IDC_ENVPATH, g_work.env_path[0] ? g_work.env_path : L"(procedural)");
}

static INT_PTR CALLBACK material_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            const wchar_t *names[] = { L"Classico", L"Metalico", L"Vidro", L"Fosco" };
            for (int i = 0; i < 4; ++i)
                SendDlgItemMessageW(h, IDC_MATMODE, CB_ADDSTRING, 0, (LPARAM)names[i]);
            SendDlgItemMessageW(h, IDC_MATMODE, CB_SETCURSEL, g_work.material_mode, 0);
            set_slider(h, IDC_METAL, 0, 100, (int)(g_work.metalness * 100.0f + 0.5f));
            set_slider(h, IDC_ROUGH, 0, 100, (int)(g_work.roughness * 100.0f + 0.5f));
            material_labels(h);
            return TRUE;
        }
        case WM_HSCROLL:
            g_work.metalness = (float)SendDlgItemMessageW(h, IDC_METAL, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.roughness = (float)SendDlgItemMessageW(h, IDC_ROUGH, TBM_GETPOS, 0, 0) / 100.0f;
            material_labels(h);
            preview_dirty(h);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDC_MATMODE:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.material_mode =
                            (int)SendDlgItemMessageW(h, IDC_MATMODE, CB_GETCURSEL, 0, 0);
                        preview_dirty(h);
                    }
                    break;
                case IDC_ENVPICK: {
                    wchar_t file[512] = L"";
                    OPENFILENAMEW ofn;
                    memset(&ofn, 0, sizeof ofn);
                    ofn.lStructSize = sizeof ofn;
                    ofn.hwndOwner = h;
                    ofn.lpstrFilter = L"Imagens\0*.jpg;*.jpeg;*.png;*.bmp;*.tga;*.hdr\0Todos\0*.*\0";
                    ofn.lpstrFile = file;
                    ofn.nMaxFile = 512;
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                    if (GetOpenFileNameW(&ofn)) {
                        wcsncpy(g_work.env_path, file, 511);
                        g_work.env_path[511] = 0;
                        material_labels(h);
                        preview_dirty(h);
                    }
                    break;
                }
                case IDC_ENVCLEAR:
                    g_work.env_path[0] = 0;
                    material_labels(h);
                    preview_dirty(h);
                    break;
            }
            return TRUE;
    }
    return FALSE;
}
```

- [ ] **Step 4: `dlg_proc`** — criar `g_material` como as outras abas (`CreateDialogW(... IDD_TAB_MATERIAL ..., material_proc)`, `place_tab_child`, `SW_HIDE`); `TabCtrl_InsertItem` para "Material" no índice 2; no `TCN_SELCHANGE` alternar as 3 abas; `preview_teardown` não muda.

- [ ] **Step 5: Build + selftest headless** (`M3DT_SELFTEST=1 ...scr /c` → `exit=0`, log com `scene: mesh`, sem `glError`).

- [ ] **Step 6: Verificação manual** — `/c` → aba Material: trocar o modo no combo (preview muda na hora), arrastar sliders, escolher uma imagem de ambiente, Limpar, `OK`; conferir `material_mode`/`metalness`/`roughness`/`env_path` em `regedit`.

- [ ] **Step 7: Testes** (regressão) + **Commit**
```sh
git add src/resource.h res/screensaver.rc src/config_dialog.c
git commit -m "$(printf 'feat: config dialog Material tab (mode/metalness/roughness/env image)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 6: Integração, QA da Fase 3a, tag

**Files:**
- Create: `docs/qa-checklist-phase-3a.md`
- Modify: `README.md`, `res/screensaver.rc` (versão → 0.3.0)

- [ ] **Step 1:** `make` / `make debug` / `make test` verdes de ponta a ponta; `.scr` < 3 MB.

- [ ] **Step 2:** Capturar os 4 modos (`text=ProArt`, `M3DT_SHOT_T=1.6`) e salvar `docs/img/phase3a-mat{0..3}.png`. Conferir cada um. Bumpar `VERSIONINFO` para `0,3,0,0`.

- [ ] **Step 3: `docs/qa-checklist-phase-3a.md`**
```markdown
# QA — Fase 3a (materiais)

## Headless (dev)
- [ ] make / make debug / make test verdes, sem warnings; .scr < 3 MB.
- [ ] test_config: round-trip de material_mode/metalness/roughness/env_path, clamps.
- [ ] 4 PNGs (um por modo, text=ProArt): 0 = especular; 1 = cromado refletindo o
      ambiente procedural; 2 = translucido com brilho de borda; 3 = fosco chapado.
      Sem glError.
- [ ] env_path apontando para uma imagem: modo 1 reflete a imagem (equirect).
      Sem env_path: volta ao procedural.
- [ ] /c selftest: aba Material abre, combo + sliders + picker; exit=0, sem glError.

## Interativo
- [ ] /c aba Material: trocar o modo -> mini-preview muda na hora.
- [ ] Sliders Metalizacao / Rugosidade -> reflexo do preview muda ao vivo.
- [ ] Escolher... abre GetOpenFileNameW; a imagem aparece refletida; Limpar volta ao procedural.
- [ ] OK grava; reabrir mostra os valores; /s (Visualizar) usa o material salvo.
- [ ] 100/150/200% DPI: aba sem corte.

## Notas (fases posteriores)
- Vidro: passe transparente simples (1 pass). WBOIT proprio = Fase 7.
- Env por imagem: amostragem equiretangular; foto comum fica "enrolada" (esperado).
- Roughness com imagem usa LOD de mipmap (aproximado, nao um pre-filtro real).
```

- [ ] **Step 4:** `README.md` status → "Fase 3a — 4 materiais (especular / metálico / vidro / fosco) com ambiente refletido procedural + imagem equirect opcional; aba Material na config".

- [ ] **Step 5: Rodar o checklist interativo.**

- [ ] **Step 6: Commit + tag**
```sh
git add docs/qa-checklist-phase-3a.md docs/img/phase3a-mat*.png README.md res/screensaver.rc
git commit -m "$(printf 'feat: phase 3a - four materials with reflected environment\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
git tag -a v0.3.0-phase3a -m "Fase 3a: 4 materiais + ambiente refletido (procedural + imagem opcional)"
```

---

## Self-Review

**1. Cobertura (materiais — spec §7, adiantados da Fase 7):**
- Modo 0 especular clássico (mantido), 1 metálico (matcap/env), 2 vidro, 3 fosco → Task 2. ✓
- "matcap embutido **ou** env lat-long de `material.envImagePath`" (§7) → ambiente **procedural** por padrão + imagem equirect opcional (Task 4). Sem matcap embutido — decisão registrada nas Global Constraints (evita binário no repo; procedural dá reflexo que acompanha a rotação). ✓
- `material.metalness` / `roughness` modulando modos 1 e 3 (spec §7 nota) → Task 1 (config) + Task 2 (shader). ✓
- `surfaceId` mantendo o bevel/parede com tratamento próprio (§7) → preservado no shader (`vSurf`). ✓
- Vidro order-independent (§7 diz WBOIT) → **passe simples** na 3a, WBOIT explicitamente adiado para a Fase 7 (spec §17 item 7). Anotado. ✓
- Aba **Material** no diálogo (§10.3) → Task 5. ✓
- **Fora da 3a:** bevel SDF/geometria + shell (Fase 3b), pós-processamento, SVG/mesh, i18n, presets.

**2. Placeholders:** nenhum "TODO"/"TBD". `env_*` é usado em `scene.c` já no Task 2 com `s->env_tex = 0` fixo, e ligado de verdade no Task 4 — a ordem está explícita nos dois tasks. Shaders e handlers de diálogo vêm completos. Verificações produzem PNG conferível (4, um por modo).

**3. Consistência de tipos:**
- `Config` campos novos (`material_mode` int, `metalness`/`roughness` float, `env_path` wchar[512]) — definidos no Task 1, lidos por `scene_set_config` (Task 2/4), escritos pelo `material_proc` (Task 5). ✓
- `Material` uniforms novos (`uMode`/`uMetalness`/`uRoughness`/`uEnvTex`/`uHasEnv`) — `material.h` (Task 2), setados em `material_set_style` (Task 2), nomes batem com os `uniform` do `model.frag` (Task 2). ✓
- `material_set_style(const Material*, int mode, float metalness, float roughness, unsigned env_tex)` — assinatura no Task 2, chamada em `scene_render` (Task 2) com `s->material_mode, s->metalness, s->roughness, s->env_tex`. ✓
- `env_load_texture(const wchar_t*)` → `unsigned` / `env_free(unsigned)` — `env.h` (Task 4), usados em `scene.c` (Task 4). `gl_texture_2d_rgb8(int,int,const unsigned char*,int)` — `gl_core.h` (Task 4), usado em `env.c` (Task 4). ✓
- IDs `IDD_TAB_MATERIAL`/`IDC_MATMODE`/... — `resource.h` (Task 5), usados no `.rc` e em `config_dialog.c` (Task 5). Índice 2 no tab control; `g_content`/`g_motion`/`g_material` alternados no `TCN_SELCHANGE`. ✓
- `set_slider` / `preview_dirty` / `g_work` — já existem de Fase 2b, reusados por `material_proc`. ✓

Sem inconsistências.

---

## Execution Handoff

**Plano completo e salvo em `docs/superpowers/plans/2026-09-10-modern-3d-text-phase-3a-materials.md`. Duas opções de execução:**

**1. Subagent-Driven (recomendado)** — subagente novo por task, revisão entre tasks.
**2. Inline Execution** — nesta sessão, com checkpoints.

**Qual abordagem?**

> Como nas fases anteriores: build/headless aqui, com um PNG por modo de material pra eu conferir o visual. O QA interativo da aba Material (combo, sliders, picker de imagem) fica pra você. Um download novo: `stb_image.h` (Task 4), da mesma fonte oficial do stb.
