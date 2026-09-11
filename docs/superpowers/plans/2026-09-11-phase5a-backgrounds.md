# Fase 5a — Fundos configuráveis — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Substituir a cor de fundo fixa (`0.02, 0.03, 0.05`) da cena 3D por um
fundo configurável com 4 tipos — sólido, gradiente, imagem e nebulosa
procedural — persistido no `Config`, editável numa nova aba "Fundo" do
diálogo, e visível ao vivo no mini-preview.

**Architecture:** Sem arquivo `.c` novo. A lógica de fundo entra em
`scene.c`: um triângulo fullscreen (reaproveitando `shaders/fullscreen.vert`)
com seu próprio shader (`shaders/background.frag`), desenhado dentro de
`scene_render()` logo após o `glClear`, com `glDepthMask(GL_FALSE)` para não
escrever profundidade — o objeto 3D é desenhado depois e sempre fica por
cima. A textura de imagem de fundo é dona da `SceneRenderer`, carregada/
liberada exatamente como `env_tex` já é hoje (mesmo padrão, textura própria
e VAO próprio — nunca `static`/global, para não repetir o bug de VAO
compartilhado entre contextos GL corrigido nesta sessão em
`gl_fullscreen_draw`).

**Tech Stack:** C11 (w64devkit/MinGW-w64), OpenGL 3.3 core, Win32 dialog
(`config_dialog.c`), stb_image (já vendorizado, implementação única em
`src/geometry/stb_impl.c`), registro do Windows para persistência
(`HKCU\Software\Modern3DText`).

## Global Constraints

- Schema do `Config` continua na v2 — campos novos são opcionais num
  registro antigo (ausentes ⇒ usam o default), sem migração dedicada.
- Nenhum arquivo `.c` novo: toda a lógica de fundo vive em `scene.c`,
  seguindo a decisão de arquitetura do spec (`docs/superpowers/specs/2026-09-11-phase5a-backgrounds-design.md`
  §2).
- Nova aba "Fundo" é **inserida no final** (índice 7, depois de "Pós") —
  não renumerar as 7 abas existentes.
- `M3DT_TAB` passa a aceitar `0..7`.
- Nenhum objeto GL (textura, VAO, programa) pode ser `static`/compartilhado
  entre `SceneRenderer`s — cada instância é dona dos seus próprios recursos
  (regra reforçada pelo bug do segundo monitor corrigido nesta sessão).
- Build: `mingw32-make -f build/Makefile debug` (ou `release`) a partir da
  raiz do repo, com o w64devkit no PATH
  (`export PATH="/c/Users/alanm/w64devkit/bin:$PATH"`). Testes:
  `mingw32-make -f build/Makefile test`.

---

### Task 1: Campos de configuração + persistência

**Files:**
- Modify: `src/config.h:30-47` (struct `Config`, logo após `fxaa_on`)
- Modify: `src/config.c` (`config_defaults`, `config_load_from`,
  `config_save_to`)
- Test: `build/tests/test_config.c`

**Interfaces:**
- Produces: 15 novos campos em `Config` — `background_type` (int, 0..3),
  `bg_color1_r/g/b`, `bg_color2_r/g/b` (float, 0..1), `bg_grad_angle`
  (float, 0..360), `bg_image_path` (`wchar_t[512]`), `bg_image_fit` (int,
  0..2), `bg_pan_speed` (float, 0..1), `bg_neb_color1_r/g/b`,
  `bg_neb_color2_r/g/b` (float, 0..1). Consumidos pela Task 3 (`scene.c`) e
  pela Task 5 (`config_dialog.c`).

- [ ] **Step 1: Adicionar os campos em `Config`**

Em `src/config.h`, logo após `int fxaa_on;` (linha 46) e antes do `}
Config;`:

```c
    int          background_type;      /* 0 solido, 1 gradiente, 2 imagem, 3 nebulosa */
    float        bg_color1_r, bg_color1_g, bg_color1_b;
    float        bg_color2_r, bg_color2_g, bg_color2_b;
    float        bg_grad_angle;        /* graus, 0..360 */
    wchar_t      bg_image_path[512];
    int          bg_image_fit;         /* 0 cobrir, 1 conter, 2 repetir */
    float        bg_pan_speed;         /* 0..1 (UV/seg), so' com type=imagem */
    float        bg_neb_color1_r, bg_neb_color1_g, bg_neb_color1_b;
    float        bg_neb_color2_r, bg_neb_color2_g, bg_neb_color2_b;
```

- [ ] **Step 2: Defaults que reproduzem o visual atual**

Em `src/config.c`, dentro de `config_defaults`, logo após `c->fxaa_on = 1;`:

```c
    c->background_type = 0;
    c->bg_color1_r = 0.02f; c->bg_color1_g = 0.03f; c->bg_color1_b = 0.05f;
    c->bg_color2_r = 0.05f; c->bg_color2_g = 0.08f; c->bg_color2_b = 0.14f;
    c->bg_grad_angle = 90.0f;
    c->bg_image_path[0] = 0;
    c->bg_image_fit = 0;
    c->bg_pan_speed = 0.02f;
    c->bg_neb_color1_r = 0.03f; c->bg_neb_color1_g = 0.02f; c->bg_neb_color1_b = 0.08f;
    c->bg_neb_color2_r = 0.25f; c->bg_neb_color2_g = 0.10f; c->bg_neb_color2_b = 0.35f;
```

`background_type = 0` com `bg_color1` igual à cor de clear hardcoded atual
garante que um registro sem esses campos (usuário existente) renderize
exatamente como hoje.

- [ ] **Step 3: Carregar do registro em `config_load_from`**

Logo antes de `RegCloseKey(k);` (depois do bloco `fxaa_on`):

```c
    reg_get_i(k, L"background_type", &c->background_type);
    reg_get_w(k, L"bg_image_path", c->bg_image_path, 512);
    reg_get_i(k, L"bg_image_fit", &c->bg_image_fit);
    if (reg_get_f(k, L"bg_color1_r", &f)) c->bg_color1_r = f;
    if (reg_get_f(k, L"bg_color1_g", &f)) c->bg_color1_g = f;
    if (reg_get_f(k, L"bg_color1_b", &f)) c->bg_color1_b = f;
    if (reg_get_f(k, L"bg_color2_r", &f)) c->bg_color2_r = f;
    if (reg_get_f(k, L"bg_color2_g", &f)) c->bg_color2_g = f;
    if (reg_get_f(k, L"bg_color2_b", &f)) c->bg_color2_b = f;
    if (reg_get_f(k, L"bg_grad_angle", &f)) c->bg_grad_angle = f;
    if (reg_get_f(k, L"bg_pan_speed", &f)) c->bg_pan_speed = f;
    if (reg_get_f(k, L"bg_neb_color1_r", &f)) c->bg_neb_color1_r = f;
    if (reg_get_f(k, L"bg_neb_color1_g", &f)) c->bg_neb_color1_g = f;
    if (reg_get_f(k, L"bg_neb_color1_b", &f)) c->bg_neb_color1_b = f;
    if (reg_get_f(k, L"bg_neb_color2_r", &f)) c->bg_neb_color2_r = f;
    if (reg_get_f(k, L"bg_neb_color2_g", &f)) c->bg_neb_color2_g = f;
    if (reg_get_f(k, L"bg_neb_color2_b", &f)) c->bg_neb_color2_b = f;
```

E no bloco de saneamento (antes de `if (c->text[0] == 0) ...`):

```c
    if (c->background_type < 0 || c->background_type > 3) c->background_type = 0;
    c->bg_color1_r = clampf(c->bg_color1_r, 0.0f, 1.0f);
    c->bg_color1_g = clampf(c->bg_color1_g, 0.0f, 1.0f);
    c->bg_color1_b = clampf(c->bg_color1_b, 0.0f, 1.0f);
    c->bg_color2_r = clampf(c->bg_color2_r, 0.0f, 1.0f);
    c->bg_color2_g = clampf(c->bg_color2_g, 0.0f, 1.0f);
    c->bg_color2_b = clampf(c->bg_color2_b, 0.0f, 1.0f);
    c->bg_grad_angle = clampf(c->bg_grad_angle, 0.0f, 360.0f);
    if (c->bg_image_fit < 0 || c->bg_image_fit > 2) c->bg_image_fit = 0;
    c->bg_pan_speed = clampf(c->bg_pan_speed, 0.0f, 1.0f);
    c->bg_neb_color1_r = clampf(c->bg_neb_color1_r, 0.0f, 1.0f);
    c->bg_neb_color1_g = clampf(c->bg_neb_color1_g, 0.0f, 1.0f);
    c->bg_neb_color1_b = clampf(c->bg_neb_color1_b, 0.0f, 1.0f);
    c->bg_neb_color2_r = clampf(c->bg_neb_color2_r, 0.0f, 1.0f);
    c->bg_neb_color2_g = clampf(c->bg_neb_color2_g, 0.0f, 1.0f);
    c->bg_neb_color2_b = clampf(c->bg_neb_color2_b, 0.0f, 1.0f);
```

- [ ] **Step 4: Salvar no registro em `config_save_to`**

Logo após o `set_f(k, L"fxaa_on", ...)` existente:

```c
    set_f(k, L"background_type", (float)c->background_type);
    set_f(k, L"bg_color1_r", c->bg_color1_r);
    set_f(k, L"bg_color1_g", c->bg_color1_g);
    set_f(k, L"bg_color1_b", c->bg_color1_b);
    set_f(k, L"bg_color2_r", c->bg_color2_r);
    set_f(k, L"bg_color2_g", c->bg_color2_g);
    set_f(k, L"bg_color2_b", c->bg_color2_b);
    set_f(k, L"bg_grad_angle", c->bg_grad_angle);
    set_w(k, L"bg_image_path", c->bg_image_path);
    set_f(k, L"bg_image_fit", (float)c->bg_image_fit);
    set_f(k, L"bg_pan_speed", c->bg_pan_speed);
    set_f(k, L"bg_neb_color1_r", c->bg_neb_color1_r);
    set_f(k, L"bg_neb_color1_g", c->bg_neb_color1_g);
    set_f(k, L"bg_neb_color1_b", c->bg_neb_color1_b);
    set_f(k, L"bg_neb_color2_r", c->bg_neb_color2_r);
    set_f(k, L"bg_neb_color2_g", c->bg_neb_color2_g);
    set_f(k, L"bg_neb_color2_b", c->bg_neb_color2_b);
```

- [ ] **Step 5: Estender `build/tests/test_config.c`**

No bloco de defaults (logo após `EXPECT(d.version == 2);`):

```c
    EXPECT(d.background_type == 0);
    EXPECT(nearf(d.bg_color1_r, 0.02f) && nearf(d.bg_color1_g, 0.03f) && nearf(d.bg_color1_b, 0.05f));
```

No bloco de round-trip, junto das outras atribuições de `a` (antes de
`config_save_to(&a, TESTKEY);`):

```c
    a.background_type = 3;
    a.bg_color1_r = 0.11f; a.bg_color1_g = 0.22f; a.bg_color1_b = 0.33f;
    a.bg_color2_r = 0.44f; a.bg_color2_g = 0.55f; a.bg_color2_b = 0.66f;
    a.bg_grad_angle = 135.0f;
    wcscpy(a.bg_image_path, L"C:\\img\\fundo.jpg");
    a.bg_image_fit = 2;
    a.bg_pan_speed = 0.35f;
    a.bg_neb_color1_r = 0.05f; a.bg_neb_color1_g = 0.05f; a.bg_neb_color1_b = 0.20f;
    a.bg_neb_color2_r = 0.80f; a.bg_neb_color2_g = 0.30f; a.bg_neb_color2_b = 0.10f;
```

E, junto das verificações de `b` (antes de `EXPECT(b.version == 2);`):

```c
    EXPECT(b.background_type == 3);
    EXPECT(nearf(b.bg_color1_r, 0.11f) && nearf(b.bg_color1_g, 0.22f) && nearf(b.bg_color1_b, 0.33f));
    EXPECT(nearf(b.bg_color2_r, 0.44f) && nearf(b.bg_color2_g, 0.55f) && nearf(b.bg_color2_b, 0.66f));
    EXPECT(nearf(b.bg_grad_angle, 135.0f));
    EXPECT(wcscmp(b.bg_image_path, L"C:\\img\\fundo.jpg") == 0);
    EXPECT(b.bg_image_fit == 2);
    EXPECT(nearf(b.bg_pan_speed, 0.35f));
    EXPECT(nearf(b.bg_neb_color1_b, 0.20f));
    EXPECT(nearf(b.bg_neb_color2_r, 0.80f));
```

No bloco de valores fora de faixa/lixo (junto do array `kv[]` de perf, que
tem `12` entradas — trocar para `16` e adicionar):

```c
            { L"background_type", L"9" }, { L"bg_grad_angle", L"999" },
            { L"bg_image_fit", L"9" }, { L"bg_pan_speed", L"-1" },
```

(ajustar o `for (int i = 0; i < 12; ++i)` para `16`) e, junto das
verificações de `e`:

```c
    EXPECT(e.background_type == 0);             /* 9 -> fora de 0..3 -> 0 */
    EXPECT(e.bg_grad_angle <= 360.0f);           /* 999 -> clamp */
    EXPECT(e.bg_image_fit == 0);                 /* 9 -> fora de 0..2 -> 0 */
    EXPECT(e.bg_pan_speed >= 0.0f);              /* -1 -> clamp */
```

- [ ] **Step 6: Rodar os testes**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```

Esperado: build sem erro, todos os `EXPECT` (incluindo os novos) passam.

- [ ] **Step 7: Commit**

```bash
git add src/config.h src/config.c build/tests/test_config.c
git commit -m "feat: add background config fields (type, colors, image, nebula)"
```

---

### Task 2: Shader de fundo + embed

**Files:**
- Create: `shaders/background.frag`
- Modify: `build/Makefile:48-51` (`EMBED_INPUTS`)

**Interfaces:**
- Consumes: `shaders/fullscreen.vert` (já existente, varying `in vec2 vUV`).
- Produces: símbolo `EMBED_background_frag` (e `EMBED_background_frag_len`)
  em `generated/embedded.h`, no mesmo padrão de `EMBED_post_bright_frag`
  etc. Consumido pela Task 3.

- [ ] **Step 1: Escrever `shaders/background.frag`**

```glsl
#version 330 core
in vec2 vUV;
out vec4 fragColor;

uniform int   uType;        /* 0 solido, 1 gradiente, 2 imagem, 3 nebulosa */
uniform vec3  uColor1;
uniform vec3  uColor2;
uniform float uGradAngle;   /* radianos */
uniform sampler2D uBgTex;
uniform int   uHasBgTex;
uniform int   uBgFit;       /* 0 cobrir, 1 conter, 2 repetir */
uniform vec2  uUvScale;
uniform vec2  uUvOffset;
uniform float uPanSpeed;
uniform vec3  uNebColor1;
uniform vec3  uNebColor2;
uniform float uTime;

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float value_noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p)
{
    float sum = 0.0, amp = 0.5;
    for (int i = 0; i < 5; ++i) {
        sum += amp * value_noise(p);
        p *= 2.02;
        amp *= 0.5;
    }
    return sum;
}

vec3 gradient_bg(void)
{
    vec2 dir = vec2(cos(uGradAngle), sin(uGradAngle));
    float t = dot(vUV - 0.5, dir) + 0.5;
    /* dither ordenado 4x4 evita banding perceptivel apos o tonemap */
    float bayer[16] = float[16](
        0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0,
        3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0
    );
    ivec2 pc = ivec2(mod(gl_FragCoord.xy, 4.0));
    float dith = (bayer[pc.y * 4 + pc.x] / 16.0 - 0.5) / 255.0;
    return mix(uColor1, uColor2, clamp(t, 0.0, 1.0)) + vec3(dith);
}

vec3 image_bg(void)
{
    vec2 uv = vUV;
    if (uBgFit == 2) {
        uv.x += uPanSpeed * uTime;
        return texture(uBgTex, uv).rgb;
    }
    uv = (uv - uUvOffset) / uUvScale;
    uv.x += uPanSpeed * uTime;
    if (uBgFit == 1 && (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0))
        return uColor1;
    return texture(uBgTex, uv).rgb;
}

vec3 nebula_bg(void)
{
    vec2 p = vUV * 3.0;
    vec2 warp1 = vec2(fbm(p + uTime * 0.01), fbm(p + vec2(5.2, 1.3) + uTime * 0.01));
    vec2 warp2 = vec2(fbm(p + warp1 * 2.0), fbm(p + warp1 * 2.0 + vec2(8.3, 2.8)));
    float n = fbm(p + warp2 * 2.0);
    return mix(uNebColor1, uNebColor2, clamp(n, 0.0, 1.0));
}

void main()
{
    vec3 col;
    if (uType == 1) col = gradient_bg();
    else if (uType == 2 && uHasBgTex != 0) col = image_bg();
    else if (uType == 3) col = nebula_bg();
    else col = uColor1;
    fragColor = vec4(col, 1.0);
}
```

- [ ] **Step 2: Adicionar ao `EMBED_INPUTS`**

Em `build/Makefile:48-51`:

```makefile
EMBED_INPUTS := shaders/model.vert shaders/model.frag shaders/fullscreen.vert \
                shaders/post_bright.frag shaders/post_down.frag shaders/post_up.frag \
                shaders/post_combine.frag shaders/post_streak.frag \
                shaders/post_finish.frag shaders/post_fxaa.frag shaders/background.frag
```

- [ ] **Step 3: Verificar o embed isoladamente**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
rm -f generated/embedded.h
mingw32-make -f build/Makefile generated/embedded.h
grep -c "EMBED_background_frag\[\]" generated/embedded.h
```

Esperado: `1` (o símbolo foi gerado).

- [ ] **Step 4: Commit**

```bash
git add shaders/background.frag build/Makefile
git commit -m "feat: add background.frag shader (solid/gradient/image/nebula)"
```

---

### Task 3: Integração em `scene.c`

**Files:**
- Modify: `src/scene.c`

**Interfaces:**
- Consumes: `Config` (Task 1), `EMBED_background_frag`/`EMBED_fullscreen_vert`
  (Task 2), `gl_program`, `gl_fullscreen_draw(unsigned *vao_cache)`,
  `gl_texture_2d_rgb8` (já existentes em `gl_core.h`).
- Produces: nenhuma API pública nova — `scene_create`/`scene_set_config`/
  `scene_render`/`scene_destroy` continuam com a mesma assinatura
  (`src/scene.h` não muda).

- [ ] **Step 1: Includes**

Em `src/scene.c:1-14`, adicionar:

```c
#include <windows.h>
#include "stb_image.h"
#include "embedded.h"
```

(`windows.h` para `WideCharToMultiByte`; `stb_image.h` apenas declarações —
a implementação já está em `src/geometry/stb_impl.c`, mesmo padrão de
`src/env.c`.)

- [ ] **Step 2: Novos campos em `struct SceneRenderer`**

Em `src/scene.c`, dentro de `struct SceneRenderer` (depois do bloco
`sdf_min_x, sdf_min_y, sdf_size_x, sdf_size_y;`):

```c
    /* fundo */
    unsigned bg_prog, bg_vao;
    int      background_type;
    v3       bg_color1, bg_color2;
    float    bg_grad_angle;
    wchar_t  bg_image_path[512];
    unsigned bg_tex;
    int      bg_tex_w, bg_tex_h;
    int      bg_image_fit;
    float    bg_pan_speed;
    v3       bg_neb_color1, bg_neb_color2;
```

- [ ] **Step 3: Loader da imagem de fundo**

Nova função `static`, logo antes de `scene_create` (mesmo padrão de
`env_load_texture`, mas capturando `w`/`h` para o cálculo de
cobrir/conter, e forçando `GL_REPEAT` no eixo vertical para o modo
"repetir" — `gl_texture_2d_rgb8` já usa `GL_REPEAT` no eixo `S`):

```c
static unsigned bg_load_texture(const wchar_t *path, int *out_w, int *out_h)
{
    *out_w = *out_h = 0;
    if (!path || !path[0]) return 0;

    char u8[1024];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, u8, (int)sizeof u8, NULL, NULL);

    int w = 0, h = 0, ch = 0;
    unsigned char *px = stbi_load(u8, &w, &h, &ch, 3);
    if (!px) {
        log_errorf("scene: fundo nao carregou %s (%s)", u8, stbi_failure_reason());
        return 0;
    }

    unsigned t = gl_texture_2d_rgb8(w, h, px, 1);
    stbi_image_free(px);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    *out_w = w; *out_h = h;
    log_infof("scene: fundo %s (%dx%d) -> tex %u", u8, w, h, t);
    return t;
}
```

- [ ] **Step 4: Criar o programa de fundo em `scene_create`**

Em `src/scene.c`, dentro de `scene_create`, logo após
`if (!material_init(&s->mat)) { free(s); return NULL; }`:

```c
    s->bg_prog = gl_program((const char *)EMBED_fullscreen_vert,
                             (const char *)EMBED_background_frag);
    if (!s->bg_prog) {
        material_destroy(&s->mat);
        free(s);
        return NULL;
    }
```

- [ ] **Step 5: Copiar config + recarregar textura em `scene_set_config`**

Em `src/scene.c`, dentro de `scene_set_config`, logo após o bloco que
recarrega `env_tex` (`if (wcscmp(s->env_path, cfg->env_path) != 0) { ... }`)
e antes de `if (mesh_dirty) { ... }`:

```c
    s->background_type = cfg->background_type;
    s->bg_color1 = (v3){ cfg->bg_color1_r, cfg->bg_color1_g, cfg->bg_color1_b };
    s->bg_color2 = (v3){ cfg->bg_color2_r, cfg->bg_color2_g, cfg->bg_color2_b };
    s->bg_grad_angle = cfg->bg_grad_angle;
    s->bg_image_fit = cfg->bg_image_fit;
    s->bg_pan_speed = cfg->bg_pan_speed;
    s->bg_neb_color1 = (v3){ cfg->bg_neb_color1_r, cfg->bg_neb_color1_g, cfg->bg_neb_color1_b };
    s->bg_neb_color2 = (v3){ cfg->bg_neb_color2_r, cfg->bg_neb_color2_g, cfg->bg_neb_color2_b };

    if (wcscmp(s->bg_image_path, cfg->bg_image_path) != 0) {
        if (s->bg_tex) glDeleteTextures(1, &s->bg_tex);
        wcsncpy(s->bg_image_path, cfg->bg_image_path, 511);
        s->bg_image_path[511] = 0;
        s->bg_tex = bg_load_texture(s->bg_image_path, &s->bg_tex_w, &s->bg_tex_h);
    }
```

- [ ] **Step 6: Desenhar o fundo em `scene_render`**

Em `src/scene.c`, dentro de `scene_render`, logo após o `glClear(...)`
existente e **antes** de `if (!s->have_mesh) return;` (o fundo deve
aparecer mesmo antes da malha existir):

```c
    glDepthMask(GL_FALSE);
    glUseProgram(s->bg_prog);
    glUniform1i(glGetUniformLocation(s->bg_prog, "uType"), s->background_type);
    glUniform3f(glGetUniformLocation(s->bg_prog, "uColor1"), s->bg_color1.x, s->bg_color1.y, s->bg_color1.z);
    glUniform3f(glGetUniformLocation(s->bg_prog, "uColor2"), s->bg_color2.x, s->bg_color2.y, s->bg_color2.z);
    glUniform1f(glGetUniformLocation(s->bg_prog, "uGradAngle"), m3dt_radians(s->bg_grad_angle));
    glUniform3f(glGetUniformLocation(s->bg_prog, "uNebColor1"), s->bg_neb_color1.x, s->bg_neb_color1.y, s->bg_neb_color1.z);
    glUniform3f(glGetUniformLocation(s->bg_prog, "uNebColor2"), s->bg_neb_color2.x, s->bg_neb_color2.y, s->bg_neb_color2.z);
    glUniform1f(glGetUniformLocation(s->bg_prog, "uTime"), (float)t);

    int has_img = (s->background_type == 2 && s->bg_tex) ? 1 : 0;
    glUniform1i(glGetUniformLocation(s->bg_prog, "uHasBgTex"), has_img);
    if (has_img) {
        float scaleX = 1.0f, scaleY = 1.0f, offX = 0.0f, offY = 0.0f;
        float imgAspect = (float)s->bg_tex_w / (float)s->bg_tex_h;
        float viewAspect = (float)fb_w / (float)fb_h;
        if (s->bg_image_fit == 0) {          /* cobrir: recorta o excesso */
            if (viewAspect > imgAspect) { scaleY = imgAspect / viewAspect; offY = (1.0f - scaleY) * 0.5f; }
            else                        { scaleX = viewAspect / imgAspect; offX = (1.0f - scaleX) * 0.5f; }
        } else if (s->bg_image_fit == 1) {   /* conter: faixas na cor 1 */
            if (viewAspect > imgAspect) { scaleX = viewAspect / imgAspect; offX = (1.0f - scaleX) * 0.5f; }
            else                        { scaleY = imgAspect / viewAspect; offY = (1.0f - scaleY) * 0.5f; }
        }
        glUniform2f(glGetUniformLocation(s->bg_prog, "uUvScale"), scaleX, scaleY);
        glUniform2f(glGetUniformLocation(s->bg_prog, "uUvOffset"), offX, offY);
        glUniform1i(glGetUniformLocation(s->bg_prog, "uBgFit"), s->bg_image_fit);
        glUniform1f(glGetUniformLocation(s->bg_prog, "uPanSpeed"), s->bg_pan_speed);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s->bg_tex);
        glUniform1i(glGetUniformLocation(s->bg_prog, "uBgTex"), 0);
    }
    gl_fullscreen_draw(&s->bg_vao);
    glDepthMask(GL_TRUE);

```

- [ ] **Step 7: Liberar recursos em `scene_destroy`**

Em `src/scene.c`, dentro de `scene_destroy`, logo após
`if (s->sdf_tex) glDeleteTextures(1, &s->sdf_tex);`:

```c
    if (s->bg_tex) glDeleteTextures(1, &s->bg_tex);
    if (s->bg_prog) glDeleteProgram(s->bg_prog);
```

- [ ] **Step 8: Build de depuração completo**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile debug
```

Esperado: `Built dist/Modern3DText.scr (N bytes)`, sem erros de compilação
ou link (warnings de `-Wall`/`-Wextra` pré-existentes, se houver, não
travam o build).

- [ ] **Step 9: Commit**

```bash
git add src/scene.c
git commit -m "feat: render configurable background (solid/gradient/image/nebula) in scene.c"
```

---

### Task 4: IDs de recurso + template da aba "Fundo"

**Files:**
- Modify: `src/resource.h`
- Modify: `res/screensaver.rc`

**Interfaces:**
- Produces: `IDD_TAB_BG` (117); `IDC_BGTYPE`, `IDC_BGCOLOR1`,
  `IDC_BGCOLOR2`, `IDC_BGANGLE`, `IDC_BGANGLE_VAL`, `IDC_BGIMGPATH`,
  `IDC_BGIMGPICK`, `IDC_BGIMGCLEAR`, `IDC_BGFIT`, `IDC_BGPAN`,
  `IDC_BGPAN_VAL`, `IDC_BGNEBCOLOR1`, `IDC_BGNEBCOLOR2` (1800-1812).
  Consumidos pela Task 5.

- [ ] **Step 1: Novos IDs em `src/resource.h`**

Logo após `#define IDD_TAB_POST      116` (linha 11):

```c
#define IDD_TAB_BG        117
```

E no final do arquivo, logo antes do `#endif` (depois do bloco "aba Pos"):

```c
/* aba Fundo */
#define IDC_BGTYPE        1800
#define IDC_BGCOLOR1      1801
#define IDC_BGCOLOR2      1802
#define IDC_BGANGLE       1803
#define IDC_BGANGLE_VAL   1804
#define IDC_BGIMGPATH     1805
#define IDC_BGIMGPICK     1806
#define IDC_BGIMGCLEAR    1807
#define IDC_BGFIT         1808
#define IDC_BGPAN         1809
#define IDC_BGPAN_VAL     1810
#define IDC_BGNEBCOLOR1   1811
#define IDC_BGNEBCOLOR2   1812
```

- [ ] **Step 2: Template `IDD_TAB_BG` em `res/screensaver.rc`**

Logo após o `END` de `IDD_TAB_POST` (linha 157), antes de
`VS_VERSION_INFO VERSIONINFO`:

```rc
IDD_TAB_BG DIALOGEX 0, 0, 240, 200
STYLE DS_SETFONT | DS_CONTROL | WS_CHILD
FONT 9, "Segoe UI", 400, 0, 0x1
BEGIN
    LTEXT      "Tipo de fundo:", -1, 8, 10, 90, 9
    COMBOBOX   IDC_BGTYPE, 100, 8, 132, 80, CBS_DROPDOWNLIST | WS_TABSTOP

    LTEXT      "Cor 1:", -1, 8, 30, 60, 9
    PUSHBUTTON "Escolher cor...", IDC_BGCOLOR1, 100, 28, 100, 14
    LTEXT      "Cor 2:", -1, 8, 48, 60, 9
    PUSHBUTTON "Escolher cor...", IDC_BGCOLOR2, 100, 46, 100, 14

    LTEXT      "Angulo do gradiente (graus):", -1, 8, 66, 140, 9
    LTEXT      "", IDC_BGANGLE_VAL, 182, 66, 50, 9
    CONTROL    "", IDC_BGANGLE, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 76, 224, 16

    LTEXT      "Imagem de fundo:", -1, 8, 96, 200, 9
    LTEXT      "(nenhuma)", IDC_BGIMGPATH, 8, 108, 224, 9, SS_PATHELLIPSIS
    PUSHBUTTON "Escolher...", IDC_BGIMGPICK, 8, 120, 60, 14
    PUSHBUTTON "Limpar",      IDC_BGIMGCLEAR, 72, 120, 50, 14
    LTEXT      "Ajuste:", -1, 128, 122, 34, 9
    COMBOBOX   IDC_BGFIT, 162, 120, 70, 60, CBS_DROPDOWNLIST | WS_TABSTOP

    LTEXT      "Velocidade de pan:", -1, 8, 140, 110, 9
    LTEXT      "", IDC_BGPAN_VAL, 182, 140, 50, 9
    CONTROL    "", IDC_BGPAN, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 150, 224, 16

    LTEXT      "Nebulosa:", -1, 8, 172, 50, 9
    PUSHBUTTON "Cor 1...", IDC_BGNEBCOLOR1, 60, 170, 80, 14
    PUSHBUTTON "Cor 2...", IDC_BGNEBCOLOR2, 144, 170, 80, 14
END
```

- [ ] **Step 3: Verificar que o `.rc` compila**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
windres --include-dir res --include-dir src -O coff res/screensaver.rc -o build/obj/screensaver.res
echo "rc ok"
```

Esperado: sem erros do `windres` (o "rc ok" é impresso).

- [ ] **Step 4: Commit**

```bash
git add src/resource.h res/screensaver.rc
git commit -m "feat: add IDD_TAB_BG dialog template and resource IDs"
```

---

### Task 5: Wiring em `config_dialog.c`

**Files:**
- Modify: `src/config_dialog.c`

**Interfaces:**
- Consumes: `IDD_TAB_BG` e IDs `IDC_BG*` (Task 4); campos `background_type`
  etc. em `Config` (Task 1); `preview_dirty`, `set_slider` (já existentes
  no arquivo).

- [ ] **Step 1: Global `g_bg`**

Em `src/config_dialog.c:29`, logo após `static HWND g_post;`:

```c
static HWND       g_bg;          /* sub-dialogo da aba Fundo */
```

- [ ] **Step 2: `bg_labels` e `bg_enable`**

Logo após o fim de `post_proc` (depois do `}` que fecha a função, antes do
comentário `/* ---------------- dialogo principal ---------------- */`):

```c
/* ---------------- aba Fundo ---------------- */

static void bg_labels(HWND h)
{
    wchar_t b[32];
    swprintf(b, 32, L"%.0f", (double)g_work.bg_grad_angle); SetDlgItemTextW(h, IDC_BGANGLE_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.bg_pan_speed);  SetDlgItemTextW(h, IDC_BGPAN_VAL, b);
    SetDlgItemTextW(h, IDC_BGIMGPATH, g_work.bg_image_path[0] ? g_work.bg_image_path : L"(nenhuma)");
}

static void bg_enable(HWND h)
{
    int t = g_work.background_type;
    EnableWindow(GetDlgItem(h, IDC_BGCOLOR1), t == 0 || t == 1);
    EnableWindow(GetDlgItem(h, IDC_BGCOLOR2), t == 1);
    EnableWindow(GetDlgItem(h, IDC_BGANGLE), t == 1);
    EnableWindow(GetDlgItem(h, IDC_BGIMGPICK), t == 2);
    EnableWindow(GetDlgItem(h, IDC_BGIMGCLEAR), t == 2);
    EnableWindow(GetDlgItem(h, IDC_BGFIT), t == 2);
    EnableWindow(GetDlgItem(h, IDC_BGPAN), t == 2);
    EnableWindow(GetDlgItem(h, IDC_BGNEBCOLOR1), t == 3);
    EnableWindow(GetDlgItem(h, IDC_BGNEBCOLOR2), t == 3);
}

static INT_PTR CALLBACK bg_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            static const wchar_t *types[] = { L"Solido", L"Gradiente", L"Imagem", L"Nebulosa" };
            static const wchar_t *fits[]  = { L"Cobrir", L"Conter", L"Repetir" };
            for (int i = 0; i < 4; ++i)
                SendDlgItemMessageW(h, IDC_BGTYPE, CB_ADDSTRING, 0, (LPARAM)types[i]);
            for (int i = 0; i < 3; ++i)
                SendDlgItemMessageW(h, IDC_BGFIT, CB_ADDSTRING, 0, (LPARAM)fits[i]);
            SendDlgItemMessageW(h, IDC_BGTYPE, CB_SETCURSEL, g_work.background_type, 0);
            SendDlgItemMessageW(h, IDC_BGFIT, CB_SETCURSEL, g_work.bg_image_fit, 0);
            set_slider(h, IDC_BGANGLE, 0, 360, (int)(g_work.bg_grad_angle + 0.5f));
            set_slider(h, IDC_BGPAN, 0, 100, (int)(g_work.bg_pan_speed * 100.0f + 0.5f));
            bg_labels(h);
            bg_enable(h);
            return TRUE;
        }
        case WM_HSCROLL:
            g_work.bg_grad_angle = (float)SendDlgItemMessageW(h, IDC_BGANGLE, TBM_GETPOS, 0, 0);
            g_work.bg_pan_speed  = (float)SendDlgItemMessageW(h, IDC_BGPAN, TBM_GETPOS, 0, 0) / 100.0f;
            bg_labels(h);
            preview_dirty(h);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDC_BGTYPE:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.background_type = (int)SendDlgItemMessageW(h, IDC_BGTYPE, CB_GETCURSEL, 0, 0);
                        bg_enable(h);
                        preview_dirty(h);
                    }
                    break;
                case IDC_BGFIT:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.bg_image_fit = (int)SendDlgItemMessageW(h, IDC_BGFIT, CB_GETCURSEL, 0, 0);
                        preview_dirty(h);
                    }
                    break;
                case IDC_BGCOLOR1: {
                    static COLORREF custom[16];
                    CHOOSECOLORW cc;
                    memset(&cc, 0, sizeof cc);
                    cc.lStructSize = sizeof cc;
                    cc.hwndOwner = h;
                    cc.lpCustColors = custom;
                    cc.rgbResult = RGB((int)(g_work.bg_color1_r * 255.0f),
                                       (int)(g_work.bg_color1_g * 255.0f),
                                       (int)(g_work.bg_color1_b * 255.0f));
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColorW(&cc)) {
                        g_work.bg_color1_r = GetRValue(cc.rgbResult) / 255.0f;
                        g_work.bg_color1_g = GetGValue(cc.rgbResult) / 255.0f;
                        g_work.bg_color1_b = GetBValue(cc.rgbResult) / 255.0f;
                        preview_dirty(h);
                    }
                    break;
                }
                case IDC_BGCOLOR2: {
                    static COLORREF custom[16];
                    CHOOSECOLORW cc;
                    memset(&cc, 0, sizeof cc);
                    cc.lStructSize = sizeof cc;
                    cc.hwndOwner = h;
                    cc.lpCustColors = custom;
                    cc.rgbResult = RGB((int)(g_work.bg_color2_r * 255.0f),
                                       (int)(g_work.bg_color2_g * 255.0f),
                                       (int)(g_work.bg_color2_b * 255.0f));
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColorW(&cc)) {
                        g_work.bg_color2_r = GetRValue(cc.rgbResult) / 255.0f;
                        g_work.bg_color2_g = GetGValue(cc.rgbResult) / 255.0f;
                        g_work.bg_color2_b = GetBValue(cc.rgbResult) / 255.0f;
                        preview_dirty(h);
                    }
                    break;
                }
                case IDC_BGNEBCOLOR1: {
                    static COLORREF custom[16];
                    CHOOSECOLORW cc;
                    memset(&cc, 0, sizeof cc);
                    cc.lStructSize = sizeof cc;
                    cc.hwndOwner = h;
                    cc.lpCustColors = custom;
                    cc.rgbResult = RGB((int)(g_work.bg_neb_color1_r * 255.0f),
                                       (int)(g_work.bg_neb_color1_g * 255.0f),
                                       (int)(g_work.bg_neb_color1_b * 255.0f));
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColorW(&cc)) {
                        g_work.bg_neb_color1_r = GetRValue(cc.rgbResult) / 255.0f;
                        g_work.bg_neb_color1_g = GetGValue(cc.rgbResult) / 255.0f;
                        g_work.bg_neb_color1_b = GetBValue(cc.rgbResult) / 255.0f;
                        preview_dirty(h);
                    }
                    break;
                }
                case IDC_BGNEBCOLOR2: {
                    static COLORREF custom[16];
                    CHOOSECOLORW cc;
                    memset(&cc, 0, sizeof cc);
                    cc.lStructSize = sizeof cc;
                    cc.hwndOwner = h;
                    cc.lpCustColors = custom;
                    cc.rgbResult = RGB((int)(g_work.bg_neb_color2_r * 255.0f),
                                       (int)(g_work.bg_neb_color2_g * 255.0f),
                                       (int)(g_work.bg_neb_color2_b * 255.0f));
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColorW(&cc)) {
                        g_work.bg_neb_color2_r = GetRValue(cc.rgbResult) / 255.0f;
                        g_work.bg_neb_color2_g = GetGValue(cc.rgbResult) / 255.0f;
                        g_work.bg_neb_color2_b = GetBValue(cc.rgbResult) / 255.0f;
                        preview_dirty(h);
                    }
                    break;
                }
                case IDC_BGIMGPICK: {
                    wchar_t file[512] = L"";
                    OPENFILENAMEW ofn;
                    memset(&ofn, 0, sizeof ofn);
                    ofn.lStructSize = sizeof ofn;
                    ofn.hwndOwner = h;
                    ofn.lpstrFilter = L"Imagens\0*.jpg;*.jpeg;*.png;*.bmp;*.tga\0Todos\0*.*\0";
                    ofn.lpstrFile = file;
                    ofn.nMaxFile = 512;
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                    if (GetOpenFileNameW(&ofn)) {
                        wcsncpy(g_work.bg_image_path, file, 511);
                        g_work.bg_image_path[511] = 0;
                        bg_labels(h);
                        preview_dirty(h);
                    }
                    break;
                }
                case IDC_BGIMGCLEAR:
                    g_work.bg_image_path[0] = 0;
                    bg_labels(h);
                    preview_dirty(h);
                    break;
            }
            return TRUE;
    }
    return FALSE;
}
```

- [ ] **Step 3: Registrar a aba em `select_tab`**

Em `src/config_dialog.c`, dentro de `select_tab`, logo após
`ShowWindow(g_post, sel == 6 ? SW_SHOW : SW_HIDE);`:

```c
    ShowWindow(g_bg,       sel == 7 ? SW_SHOW : SW_HIDE);
```

- [ ] **Step 4: Criar/posicionar em `dlg_proc` (`WM_INITDIALOG`)**

Logo após `TabCtrl_InsertItem(tabs, 6, &ti);`:

```c
            ti.pszText = L"Fundo";
            TabCtrl_InsertItem(tabs, 7, &ti);
```

Logo após `g_post = CreateDialogW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_TAB_POST), h, post_proc);`:

```c
            g_bg = CreateDialogW(GetModuleHandleW(NULL),
                                 MAKEINTRESOURCEW(IDD_TAB_BG), h, bg_proc);
```

Logo após `place_tab_child(h, tabs, g_post);`:

```c
            place_tab_child(h, tabs, g_bg);
```

- [ ] **Step 5: Bump do clamp de `M3DT_TAB`**

Em `dlg_proc`, dentro do bloco `if (g_selftest) { ... }`, trocar
`if (sel > 6) sel = 6;` por:

```c
                    if (sel > 7) sel = 7;
```

- [ ] **Step 6: Build de depuração + checagem visual da aba**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile debug
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

```powershell
$env:M3DT_SELFTEST = "1"
$env:M3DT_TAB = "7"
$env:M3DT_HOLD_MS = "4000"
Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -Wait
```

Esperado: o processo abre e fecha sozinho após ~4s sem crash (confirma que
a aba 7 é criada, mostrada e destruída sem erro). Para inspeção visual dos
controles, usar o padrão `shot_dialog.ps1` já existente no scratchpad da
sessão anterior (PrintWindow captura layout/controles — nunca o render 3D
ao vivo).

- [ ] **Step 7: Commit**

```bash
git add src/config_dialog.c
git commit -m "feat: wire 'Fundo' tab into config dialog (8th tab, index 7)"
```

---

### Task 6: Verificação final e commit

**Files:** nenhum (só execução/validação).

- [ ] **Step 1: Suite de testes completa**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```

Esperado: todos os testes passam, incluindo os novos de `test_config.c`.

- [ ] **Step 2: Gerar uma imagem de teste não-quadrada**

```powershell
Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap(300, 150)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::FromArgb(255, 40, 120, 200))
$g.FillEllipse([System.Drawing.Brushes]::Orange, 90, 30, 120, 90)
$g.Dispose()
$bmp.Save("$env:TEMP\m3dt_bg_test.png", [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "saved $env:TEMP\m3dt_bg_test.png"
```

- [ ] **Step 3: Capturar os 4 tipos de fundo via `M3DT_SHOT`**

Para cada tipo, sobrescrever o registro real (o mesmo lido por
`config_load` ao abrir o diálogo) e capturar o preview:

```powershell
function Set-BgConfig($type, $extra) {
    $key = "HKCU:\Software\Modern3DText"
    New-Item -Path $key -Force | Out-Null
    Set-ItemProperty -Path $key -Name "background_type" -Value "$type"
    foreach ($kv in $extra.GetEnumerator()) {
        Set-ItemProperty -Path $key -Name $kv.Key -Value $kv.Value
    }
}

function Shoot($outPath) {
    $env:M3DT_SELFTEST = "1"
    $env:M3DT_SHOT = $outPath
    $env:M3DT_SHOT_T = "2.25"
    $env:M3DT_HOLD_MS = "4000"
    Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -Wait
}

# 0: solido (default)
Set-BgConfig 0 @{}
Shoot "$env:TEMP\m3dt_bg_solid.png"

# 1: gradiente
Set-BgConfig 1 @{ bg_color1_r="0.05"; bg_color1_g="0.02"; bg_color1_b="0.15";
                  bg_color2_r="0.95"; bg_color2_g="0.55"; bg_color2_b="0.15";
                  bg_grad_angle="45" }
Shoot "$env:TEMP\m3dt_bg_gradient.png"

# 2: imagem (cobrir)
Set-BgConfig 2 @{ bg_image_path="$env:TEMP\m3dt_bg_test.png"; bg_image_fit="0" }
Shoot "$env:TEMP\m3dt_bg_image_cover.png"

# 2: imagem (conter)
Set-BgConfig 2 @{ bg_image_path="$env:TEMP\m3dt_bg_test.png"; bg_image_fit="1" }
Shoot "$env:TEMP\m3dt_bg_image_contain.png"

# 3: nebulosa
Set-BgConfig 3 @{ bg_neb_color1_r="0.02"; bg_neb_color1_g="0.02"; bg_neb_color1_b="0.10";
                  bg_neb_color2_r="0.85"; bg_neb_color2_g="0.30"; bg_neb_color2_b="0.55" }
Shoot "$env:TEMP\m3dt_bg_nebula.png"
```

Depois de cada captura, usar a ferramenta `Read` para abrir o PNG e
confirmar visualmente: sólido mostra a cor 1 uniforme; gradiente mostra
transição diagonal sem banding perceptível; imagem-cobrir preenche o
quadro cortando o excesso; imagem-conter mostra faixas na cor 1; nebulosa
mostra um campo de ruído colorido sem costura.

- [ ] **Step 4: Checagem real em 2 monitores (regressão do bug de VAO)**

Com um fundo de imagem configurado (reaproveitando o registro do passo
anterior, tipo `2`), rodar o binário real (não-selftest) com captura nos
dois monitores, repetindo 2-3x se a primeira tentativa fechar cedo por
`WM_MOUSEMOVE` espúrio (flakiness já documentada nesta sessão):

```powershell
Remove-Item Env:\M3DT_SELFTEST -ErrorAction SilentlyContinue
$env:M3DT_SHOT = "$env:TEMP\m3dt_bg_mon1.png"
$env:M3DT_SHOT2 = "$env:TEMP\m3dt_bg_mon2.png"
$env:M3DT_SHOT_T = "2.25"
Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/s" -Wait
```

Esperado: ambos os PNGs existem, com o mesmo fundo de imagem aparecendo
corretamente nos dois (nenhuma janela preta, nenhum vazamento de estado
entre os dois contextos GL — a checagem direta de que `bg_tex`/`bg_prog`/
`bg_vao` são de fato por-`SceneRenderer` e não compartilhados).

- [ ] **Step 5: Restaurar o registro para os defaults**

```powershell
Remove-Item -Path "HKCU:\Software\Modern3DText" -Recurse -Force -ErrorAction SilentlyContinue
```

- [ ] **Step 6: Build release final**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile release
```

Esperado: `Built dist/Modern3DText.scr (N bytes)`, abaixo do limite de 3 MB.

- [ ] **Step 7: Commit final (se houver ajustes pendentes dos passos acima)**

```bash
git add -A
git status
```

Revisar a saída antes de commitar qualquer coisa além do que já foi
commitado nas Tasks 1-5 — este passo é só uma rede de segurança caso algum
ajuste tenha sido feito durante a verificação.

---

## Self-Review (executado antes de apresentar o plano)

1. **Cobertura do spec**: seção 3 (campos) → Task 1; seção 2/4
   (arquitetura, shader, os 4 tipos) → Tasks 2-3; seção 5 (interface) →
   Tasks 4-5; seção 6 (preview ao vivo) → automático, sem tarefa dedicada
   (já funciona assim que `scene_render` desenha o fundo); seção 7
   (testes) → Task 6, incluindo a checagem dual-monitor explicitamente
   pedida. Seção 8 (fora de escopo) não gera tarefas, como esperado.
2. **Placeholders**: nenhum "TBD"/"depois"/"similar à Task N" — todo
   trecho de código é completo e cola direto no arquivo indicado.
3. **Consistência de tipos**: `background_type`/`bg_image_fit` como `int`
   em `Config` e em `SceneRenderer` batem; nomes de campo idênticos entre
   `config.h`, `config.c`, `scene.c` e `config_dialog.c`; IDs `IDC_BG*`
   usados em `config_dialog.c` (Task 5) são exatamente os declarados em
   `resource.h` (Task 4); símbolo `EMBED_background_frag` usado em
   `scene.c` (Task 3) é exatamente o gerado a partir de
   `shaders/background.frag` (Task 2, confirmado pela regra de
   `build/tools/embed.c`).

---

**Plano completo e salvo em `docs/superpowers/plans/2026-09-11-phase5a-backgrounds.md`.**

Duas opções de execução:

1. **Subagent-Driven (recomendado)** — dispatco um subagente novo por
   task, com revisão entre elas e iteração rápida.
2. **Inline Execution** — executo as tasks nesta sessão via
   `executing-plans`, em lote com checkpoints para revisão.

Qual prefere?
