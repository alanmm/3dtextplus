# Modern 3D Text — Fase 4a: HDR + Bloom — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A cena passa a ser renderizada em **HDR** (RGBA16F) com MSAA, e o frame final ganha **bloom/glare** (pirâmide de mips) + **tonemap ACES** + gamma. É a base do "brilho" cinematográfico — os realces especulares e o ambiente refletido passam a sangrar luz de forma controlada, em vez de estourar em branco chapado. Controles numa aba **Efeitos** no diálogo.

**Architecture:** Um módulo `post.c` gerencia os alvos de render e a cadeia de passes. Por janela GL: um FBO **HDR MSAA** (renderbuffers RGBA16F + depth24) recebe a cena; `glBlitFramebuffer` resolve para uma textura RGBA16F single-sample; um **bright-pass** com soft-knee extrai as áreas claras para meia-resolução; uma **pirâmide de downsample** (13-tap, ~6 níveis) e **upsample com tent-filter aditivo** montam o bloom; um passe de **composição** (`cena + bloom·intensidade` → ACES → gamma) escreve no framebuffer padrão; `SwapBuffers`. O `knee()` provisório do `model.frag` sai (o tonemap de verdade assume). Os passes fullscreen usam um VAO vazio + um vertex shader que gera o triângulo por `gl_VertexID`.

**Tech Stack:** C11 · w64devkit · OpenGL 3.3 / GLSL 330 · FBOs / renderbuffers RGBA16F · reuso de `config`/`scene`/`gl_core`/`gl_window`/`config_dialog`/`embed`.

## Global Constraints

Do spec (§8) e do estado pós-Fase 3b. Todo task herda esta seção.

- **Linguagem:** C11. Flags `-std=c11 -municode -Wall -Wextra` (+ `-O2 -DNDEBUG` release). **Sem `-ffast-math`.** Build **sem warnings**.
- **Toolchain:** só w64devkit. Sem downloads novos.
- **API gráfica:** OpenGL 3.3 core. Formatos: cor HDR = `GL_RGBA16F`; bloom = `GL_R11F_G11F_B10F` (mais leve, sem alpha). MSAA no FBO da cena limitado a `min(pedido, GL_MAX_SAMPLES)` (o default-FB MSAA da Fase 3b **sai** — o MSAA agora é no FBO HDR).
- **Registro:** só `HKCU\Software\Modern3DText` (testes: `_test`). Sem escrita fora disso e de `%LOCALAPPDATA%\Modern3DText\`.
- **`.scr` ≤ 3 MB.** i18n ainda não (Fase 8).
- **Modo preview / reduzido:** o `/p` e o fallback usam um caminho simplificado — resolve + tonemap, **sem** bloom (menos passes na janelinha). Decidido por um flag em `post`.
- **Commits frequentes.** TDD em `config` (campos de efeito). GL/pós: PNG conferível (com/sem bloom, threshold alto/baixo).
- **Atribuição:** todo commit termina com `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>`.

---

## File Structure

| Arquivo | Responsabilidade |
|---|---|
| `src/config.h` / `.c` | (+) `bloom_on` (0/1), `bloom_threshold`, `bloom_intensity`, `bloom_radius` |
| `src/gl_core.h` / `.c` | (+) helpers de FBO: `GlFbo` (cor tex + opções), `gl_fbo_color16f`, `gl_fbo_hdr_ms` (renderbuffers MSAA), `gl_fbo_bind`, `gl_fbo_free`, `gl_blit_resolve`; `gl_fullscreen_draw()` (VAO vazio + 3 vértices) |
| `src/post.h` / `.c` | `Post`: cria/redimensiona os alvos; `post_begin(p, w, h)` (liga o FBO HDR MSAA); `post_present(p, w, h, PostParams)` (resolve + bloom + tonemap → FB 0); `post_destroy` |
| `shaders/fullscreen.vert` | triângulo fullscreen por `gl_VertexID`; `out vec2 vUV` |
| `shaders/post_bright.frag` | bright-pass com soft-knee |
| `shaders/post_down.frag` | downsample 13-tap |
| `shaders/post_up.frag` | upsample tent 9-tap (aditivo) |
| `shaders/post_composite.frag` | `cena + bloom·int` → ACES → gamma 2.2 |
| `shaders/model.frag` | (modif.) remover `knee()` |
| `src/gl_window.c` | (modif.) cada `GlWindow` tem um `Post*`; `gl_window_frame` = `post_begin` → `scene_render` → `post_present`; `gl_window_render_scene_at` idem (sem SwapBuffers) |
| `res/screensaver.rc`, `src/resource.h` | (+) `IDD_TAB_EFFECTS` |
| `src/config_dialog.c` | (+) aba **Efeitos** (bloom on + 3 sliders) |
| `build/tests/test_config.c` | (+) round-trip dos campos de bloom |
| `build/Makefile` | `src/post.c` em `SRC_C`; novos shaders em `EMBED_INPUTS` |

---

## Task 1: Config — campos de bloom (TDD)

**Files:** `src/config.h`, `src/config.c`, `build/tests/test_config.c`

**Interfaces:** `Config` ganha
```c
int   bloom_on;         /* 0/1 */
float bloom_threshold;  /* 0.2 .. 3.0 (luminancia) */
float bloom_intensity;  /* 0 .. 2.0 */
float bloom_radius;     /* 0 .. 1  (peso do upsample; controla o "espalhamento") */
```
Padrões: `bloom_on = 1`, `bloom_threshold = 1.05`, `bloom_intensity = 0.6`, `bloom_radius = 0.55`. Chaves REG_SZ homônimas.

- [ ] **Step 1: `test_config.c`** — round-trip dos 4 + clamps (`bloom_on` "3"→1; threshold "9"→≤3; intensity "-1"→≥0; radius "5"→≤1).
- [ ] **Step 2: `config.h`** — 4 campos após `quality`.
- [ ] **Step 3: `config.c`** — defaults, `reg_get_i`/`reg_get_f`, clamps (`clampf(threshold, 0.2, 3.0)`, `clampf(intensity, 0, 2)`, `clampf(radius, 0, 1)`, `bloom_on = bloom_on ? 1 : 0`), `config_save_to`.
- [ ] **Step 4: Rodar — falha, depois passa.**
- [ ] **Step 5: Commit**
```sh
git add src/config.h src/config.c build/tests/test_config.c
git commit -m "$(printf 'feat: config bloom fields (on/threshold/intensity/radius)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 2: `gl_core` — helpers de FBO + fullscreen

**Files:** `src/gl_core.h`, `src/gl_core.c`

**Interfaces:**
```c
typedef struct { unsigned fbo, color, depth; int w, h; int ms; } GlFbo;

GlFbo gl_fbo_color16f(int w, int h, int with_depth);   /* cor RGBA16F tex, depth24 opcional */
GlFbo gl_fbo_r11f(int w, int h);                        /* cor R11F_G11F_B10F, sem depth (bloom) */
GlFbo gl_fbo_hdr_ms(int w, int h, int samples);         /* renderbuffers RGBA16F + depth24 MSAA */
void  gl_fbo_bind(const GlFbo *f);                       /* bind + glViewport */
void  gl_fbo_resize(GlFbo *f, int w, int h);             /* recria se mudou de tamanho */
void  gl_fbo_free(GlFbo *f);
void  gl_blit_resolve(const GlFbo *src_ms, const GlFbo *dst);   /* glBlitFramebuffer COLOR */
void  gl_fullscreen_draw(void);                          /* usa um VAO vazio interno + 3 verts */
int   gl_max_samples(void);
```

- [ ] **Step 1: implementar em `gl_core.c`.** Notas:
  - `gl_fbo_color16f`: `glTexImage2D(GL_RGBA16F ... GL_RGBA, GL_FLOAT, NULL)`, `GL_LINEAR`, `GL_CLAMP_TO_EDGE`; depth = renderbuffer `GL_DEPTH_COMPONENT24`.
  - `gl_fbo_hdr_ms`: `glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA16F, w, h)` + depth24 MSAA; `samples = min(samples, gl_max_samples())`.
  - `gl_blit_resolve`: `glBindFramebuffer(GL_READ_FRAMEBUFFER, src); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst); glBlitFramebuffer(0,0,w,h, 0,0,w,h, GL_COLOR_BUFFER_BIT, GL_NEAREST);`
  - `gl_fullscreen_draw`: um `static unsigned vao; if (!vao) glGenVertexArrays(1,&vao); glBindVertexArray(vao); glDrawArrays(GL_TRIANGLES,0,3); glBindVertexArray(0);`
  - `gl_max_samples`: `int s; glGetIntegerv(GL_MAX_SAMPLES, &s); return s;`
  - Checar `glCheckFramebufferStatus == GL_FRAMEBUFFER_COMPLETE`; log em erro; `GlFbo.fbo == 0` sinaliza falha.

- [ ] **Step 2: Verificação headless** — hook temporário: criar um `gl_fbo_color16f(64,64,1)`, bind, `glClear`, ler 1 pixel com `glReadPixels` (deve ser a cor de clear), `glGetError` limpo. Remover o hook.

- [ ] **Step 3: Commit**
```sh
git add src/gl_core.h src/gl_core.c
git commit -m "$(printf 'feat: gl_core FBO helpers (RGBA16F, MSAA HDR, blit-resolve) + fullscreen draw\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 3: Shaders de pós + `embed`

**Files:** `shaders/fullscreen.vert`, `shaders/post_bright.frag`, `shaders/post_down.frag`, `shaders/post_up.frag`, `shaders/post_composite.frag`; `build/Makefile` (`EMBED_INPUTS`); `shaders/model.frag` (remover `knee`).

- [ ] **Step 1: `shaders/fullscreen.vert`**
```glsl
#version 330 core
out vec2 vUV;
void main()
{
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUV = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
```

- [ ] **Step 2: `shaders/post_bright.frag`**
```glsl
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform float uThreshold;
uniform float uKnee;       // = 0.5 * threshold
out vec3 o;
void main()
{
    vec3 c = texture(uTex, vUV).rgb;
    float br = max(c.r, max(c.g, c.b));
    float k = max(uKnee, 1e-4);
    float soft = clamp(br - uThreshold + k, 0.0, 2.0 * k);
    soft = soft * soft / (4.0 * k);
    float w = max(soft, br - uThreshold) / max(br, 1e-4);
    o = c * w;
}
```

- [ ] **Step 3: `shaders/post_down.frag`** (13-tap, "Next Gen Post" / Sledgehammer)
```glsl
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uTexel;   // 1/tamanho da TEXTURA de origem
out vec3 o;
void main()
{
    vec2 t = uTexel;
    vec3 a = texture(uTex, vUV + t * vec2(-2,-2)).rgb;
    vec3 b = texture(uTex, vUV + t * vec2( 0,-2)).rgb;
    vec3 c = texture(uTex, vUV + t * vec2( 2,-2)).rgb;
    vec3 d = texture(uTex, vUV + t * vec2(-2, 0)).rgb;
    vec3 e = texture(uTex, vUV).rgb;
    vec3 f = texture(uTex, vUV + t * vec2( 2, 0)).rgb;
    vec3 g = texture(uTex, vUV + t * vec2(-2, 2)).rgb;
    vec3 h = texture(uTex, vUV + t * vec2( 0, 2)).rgb;
    vec3 i = texture(uTex, vUV + t * vec2( 2, 2)).rgb;
    vec3 j = texture(uTex, vUV + t * vec2(-1,-1)).rgb;
    vec3 k = texture(uTex, vUV + t * vec2( 1,-1)).rgb;
    vec3 l = texture(uTex, vUV + t * vec2(-1, 1)).rgb;
    vec3 m = texture(uTex, vUV + t * vec2( 1, 1)).rgb;
    o = e * 0.125
      + (a + c + g + i) * 0.03125
      + (b + d + f + h) * 0.0625
      + (j + k + l + m) * 0.125;
}
```

- [ ] **Step 4: `shaders/post_up.frag`** (tent 3x3)
```glsl
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2  uTexel;   // 1/tamanho da textura de origem
uniform float uRadius;  // 0..1 -> escala do raio do tent
out vec3 o;
void main()
{
    vec2 r = uTexel * (1.0 + uRadius * 2.0);
    vec3 s = texture(uTex, vUV + r * vec2(-1,-1)).rgb * 1.0;
    s += texture(uTex, vUV + r * vec2( 0,-1)).rgb * 2.0;
    s += texture(uTex, vUV + r * vec2( 1,-1)).rgb * 1.0;
    s += texture(uTex, vUV + r * vec2(-1, 0)).rgb * 2.0;
    s += texture(uTex, vUV).rgb * 4.0;
    s += texture(uTex, vUV + r * vec2( 1, 0)).rgb * 2.0;
    s += texture(uTex, vUV + r * vec2(-1, 1)).rgb * 1.0;
    s += texture(uTex, vUV + r * vec2( 0, 1)).rgb * 2.0;
    s += texture(uTex, vUV + r * vec2( 1, 1)).rgb * 1.0;
    o = s * (1.0 / 16.0);
}
```

- [ ] **Step 5: `shaders/post_composite.frag`**
```glsl
#version 330 core
in vec2 vUV;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uBloomIntensity;
uniform int   uHasBloom;
out vec4 o;

vec3 aces(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
void main()
{
    vec3 c = texture(uScene, vUV).rgb;
    if (uHasBloom == 1) c += texture(uBloom, vUV).rgb * uBloomIntensity;
    c = aces(c);
    c = pow(c, vec3(1.0 / 2.2));
    o = vec4(c, 1.0);
}
```

- [ ] **Step 6: `shaders/model.frag`** — remover a função `knee()` e trocar `fragColor = vec4(knee(col), ...)` por `fragColor = vec4(col, ...)` nos 4 pontos. A cena agora entrega HDR linear; o tonemap é no `post_composite`.

- [ ] **Step 7: `build/Makefile`**
```make
EMBED_INPUTS := shaders/model.vert shaders/model.frag shaders/fullscreen.vert \
                shaders/post_bright.frag shaders/post_down.frag shaders/post_up.frag \
                shaders/post_composite.frag
```

- [ ] **Step 8: `mingw32-make -f build/Makefile generated/embedded.h`** — confere que `EMBED_fullscreen_vert` etc. aparecem. Build da app (a cena renderiza direto no FB por ora — `post` entra no Task 5; sem bloom, texto mais escuro sem o tonemap... aceitável ate o Task 5). Melhor: pular a verificação visual aqui, so garantir que compila.

- [ ] **Step 9: Commit**
```sh
git add shaders/ build/Makefile
git commit -m "$(printf 'feat: post shaders (fullscreen, bright, down, up, composite); drop shader knee\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 4: `post.c` — pipeline HDR + bloom

**Files:** Create `src/post.h`, `src/post.c`; modify `build/Makefile`.

**Interfaces:**
```c
typedef struct {
    int   bloom;            /* 0 desliga a cadeia de bloom (preview/reduzido) */
    float threshold, intensity, radius;
} PostParams;

typedef struct Post Post;
Post *post_create(void);
void  post_begin(Post *p, int w, int h);                 /* redimensiona + liga o FBO HDR MSAA + limpa */
void  post_present(Post *p, int w, int h, PostParams pr);/* resolve + bloom + tonemap -> FB 0 (nao faz SwapBuffers) */
void  post_destroy(Post *p);
```

**`Post` internamente:** `GlFbo hdr_ms; GlFbo hdr; GlFbo bloom[BLOOM_MIPS];` (BLOOM_MIPS = 6), `unsigned prog_bright, prog_down, prog_up, prog_comp;`, `int w, h, samples;`.

- [ ] **Step 1: `post_create`** — compila os 4 programas (`gl_program(EMBED_fullscreen_vert, EMBED_post_*_frag)`), seta os samplers (`glUniform1i`), `samples = min(4, gl_max_samples())`.

- [ ] **Step 2: `post_begin`** — se `w/h` mudou: `gl_fbo_resize(&hdr_ms ...)`, `gl_fbo_resize(&hdr ...)`, e cada `bloom[i]` para `(w>>(i+1), h>>(i+1))` com piso 1. `gl_fbo_bind(&hdr_ms)`; `glEnable(GL_DEPTH_TEST); glClearColor(...); glClear(COLOR|DEPTH)`. (o `scene_render` também limpa — tudo bem, ou remover o clear de `scene_render` quando via post; manter por simplicidade.)

- [ ] **Step 3: `post_present`**
```
1. gl_blit_resolve(&hdr_ms, &hdr);                 // MSAA -> textura HDR
2. if (!pr.bloom) { composita direto (uHasBloom=0) -> FB 0; return; }
3. bright: bind bloom[0]; prog_bright; uTex=hdr.color; uThreshold/uKnee; fullscreen.
4. down:  for i=1..MIPS-1: bind bloom[i]; prog_down; uTex=bloom[i-1].color;
          uTexel = 1/size(bloom[i-1]); fullscreen.
5. up:    glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE);
          for i=MIPS-2..0: bind bloom[i]; prog_up; uTex=bloom[i+1].color;
          uTexel = 1/size(bloom[i+1]); uRadius=pr.radius; fullscreen.
          glDisable(GL_BLEND);
6. composite: glBindFramebuffer(0); glViewport(0,0,w,h); glDisable(GL_DEPTH_TEST);
          prog_comp; uScene=hdr.color (unidade 0); uBloom=bloom[0].color (unidade 1);
          uBloomIntensity=pr.intensity; uHasBloom=1; fullscreen.
```
Restaurar estado no fim (`glEnable(GL_DEPTH_TEST)`).

- [ ] **Step 4: `build/Makefile`** — `SRC_C += src/post.c`; garantir `build/obj/src/post.o` depende de `generated/embedded.h` (o padrao ja cobre).

- [ ] **Step 5: Verificação** — só compila nesta task; o teste visual é no Task 5.

- [ ] **Step 6: Commit**
```sh
git add src/post.h src/post.c build/Makefile
git commit -m "$(printf 'feat: post - HDR MSAA target, bloom mip pyramid, ACES tonemap\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 5: Ligar o `post` no `gl_window`

**Files:** `src/gl_window.h` / `.c`.

- [ ] **Step 1: `GlWindow`** ganha `Post *post;`. `gl_window_create`: `g->post = post_create();`. `gl_window_destroy`: `post_destroy(g->post)` (com contexto corrente).

- [ ] **Step 2: `gl_window` precisa dos `PostParams`.** Duas opções: (a) `gl_window_set_config` passa os campos de bloom para dentro; (b) `scene` expõe um getter. **Decisão:** `gl_window` guarda um `PostParams g->post_params` e um flag `g->preview_mode`; `gl_window_set_config` copia de `cfg` (`bloom_on`→`bloom`, threshold/intensity/radius). No `host_run_preview` e no fallback, `gl_window` é criado com preview_mode=1 → `PostParams.bloom = 0`.
  - Adicionar param `int preview` a `gl_window_create`, ou um `gl_window_set_preview(g, 1)`. **Decisão:** param novo em `gl_window_create` (todos os call sites já listados). `host_run_preview` passa 1; `host_run_saver` e o config-dialog passam 0.

- [ ] **Step 3: `gl_window_frame`**
```c
wglMakeCurrent(g->dc, g->rc);
RECT cr; GetClientRect(g->hwnd, &cr);
g->w = cr.right; g->h = cr.bottom;
post_begin(g->post, g->w, g->h);
if (g->scene) scene_render(g->scene, t, g->w, g->h);
else { glClearColor(0.1f,0,0,1); glClear(GL_COLOR_BUFFER_BIT); }
PostParams pr = g->post_params;
if (g->preview_mode || !g->post_params.bloom) pr.bloom = 0; else pr.bloom = 1;
post_present(g->post, g->w, g->h, pr);
SwapBuffers(g->dc);
```
`gl_window_render_scene_at` (para o `M3DT_SHOT`): igual mas sem `SwapBuffers` (o `glReadPixels` lê o FB 0 depois do `post_present`).

- [ ] **Step 4: `scene_render`** — pode manter o `glViewport` + `glClear` (agora agem no FBO HDR). Sem mudança obrigatória.

- [ ] **Step 5: Build + captura comparativa**
```sh
mingw32-make -f build/Makefile
cp dist/Modern3DText.scr "$TEMP/m.exe"
# com bloom
powershell ... material_mode=1 metalness=1 roughness=0.06 env_path=<img> bloom_on=1 bloom_intensity=0.8
M3DT_SELFTEST=1 M3DT_SHOT_T=1.5 M3DT_SHOT=bloom_on.png ...
# sem bloom
powershell ... bloom_on=0
M3DT_SELFTEST=1 M3DT_SHOT_T=1.5 M3DT_SHOT=bloom_off.png ...
```
Abrir: **bloom_on** — os realces cromados sangram um brilho suave; o tonemap ACES tira o "branco chapado", as transições ficam mais suaves. **bloom_off** — só o tonemap (já melhora vs o `knee` antigo). Sem `glError`. Comparar também `/p` (preview, sem bloom) e um `//c` (o mini-preview usa bloom).

- [ ] **Step 6: Testes** (regressão) + estabilidade (abrir/fechar `/c` 10×, sem vazamento de FBO).

- [ ] **Step 7: Commit**
```sh
git add src/gl_window.h src/gl_window.c src/host_win32.c src/config_dialog.c
git commit -m "$(printf 'feat: route the scene through the HDR/bloom post chain per GL window\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 6: Aba Efeitos no diálogo

**Files:** `src/resource.h`, `res/screensaver.rc`, `src/config_dialog.c`.

**Aba Efeitos (índice 4):** checkbox **Bloom (glare)**, sliders **Limiar** (`bloom_threshold`), **Intensidade** (`bloom_intensity`), **Espalhamento** (`bloom_radius`). Os 3 sliders só habilitam com o checkbox. Mexer → `g_work` + `WM_APP+1`; o mini-preview também precisa passar os `PostParams` novos (`gl_window_set_config` já cuida, se o Task 5 fez a cópia a partir de `cfg`).

- [ ] **Step 1: `resource.h`** — `IDD_TAB_EFFECTS 114` + `IDC_BLOOM/IDC_BTHRESH/IDC_BTHRESH_VAL/IDC_BINT/IDC_BINT_VAL/IDC_BRAD/IDC_BRAD_VAL` (1500+).
- [ ] **Step 2: `res/screensaver.rc`** — `IDD_TAB_EFFECTS DIALOGEX` com o check + 3 sliders + rótulos.
- [ ] **Step 3: `config_dialog.c`** — `effects_proc` (padrão dos outros): sliders ×100 (threshold 20..300, intensity 0..200, radius 0..100), `EnableWindow` conforme o check. `g_effects` criado no `WM_INITDIALOG`, `TabCtrl_InsertItem` índice 4, alternado no `TCN_SELCHANGE` (5 abas).
- [ ] **Step 4: Build + selftest headless** (`/c` abre com 5 abas, fecha, `exit=0`, sem `glError`).
- [ ] **Step 5: Verificação manual** — `/c` aba Efeitos: ligar/desligar bloom → preview muda; sliders → brilho responde; `OK` grava; `regedit`.
- [ ] **Step 6: Testes** + **Commit**
```sh
git add src/resource.h res/screensaver.rc src/config_dialog.c
git commit -m "$(printf 'feat: config dialog Effects tab (bloom on/threshold/intensity/radius)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 7: Integração, QA da Fase 4a, tag

**Files:** `docs/qa-checklist-phase-4a.md`, `README.md`, `res/screensaver.rc` (versão → 0.4.0), `docs/img/phase4a-*.png`.

- [ ] **Step 1:** `make` / `make debug` / `make test` verdes; `.scr` < 3 MB. Medir o custo de frame no log (opcional: `GL_TIME_ELAPSED` no `post_present`) — deve ficar bem abaixo de 16 ms a 1080p na RTX.
- [ ] **Step 2:** Capturas `docs/img/phase4a-bloom-on.png` / `-off.png` (metálico + env). Conferir.
- [ ] **Step 3: `docs/qa-checklist-phase-4a.md`** — headless (build/test, config, bloom on/off, `/p` sem bloom, sem glError, custo de frame) + interativo (aba Efeitos, cada controle → preview; multi-monitor com FBOs por janela; 100/150/200% DPI; 5 min rodando sem vazamento de VRAM).
- [ ] **Step 4:** `README.md` status → "Fase 4a — render HDR + bloom + tonemap ACES; aba Efeitos".
- [ ] **Step 5: Rodar o checklist interativo.**
- [ ] **Step 6: Commit + tag**
```sh
git add docs/qa-checklist-phase-4a.md docs/img/phase4a-*.png README.md res/screensaver.rc
git commit -m "$(printf 'feat: phase 4a - HDR rendering with bloom and ACES tonemap\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
git tag -a v0.4.0-phase4a -m "Fase 4a: HDR + bloom + tonemap ACES"
```

---

## Self-Review

**1. Cobertura (spec §8, recorte 4a):**
- RT HDR MSAA (RGBA16F + depth) + resolve → Tasks 2 (helpers) + 4 (`post`). ✓
- Bright-pass + pirâmide de downsample (13-tap) + upsample tent aditivo = bloom → Tasks 3 (shaders) + 4 (orquestração). 6 níveis. ✓
- Composição `cena + bloom·intensidade` → Task 4 Step 3. ✓
- Tonemap ACES filmic + gamma 2.2 → `post_composite.frag` (Task 3). O `knee()` provisório sai (Task 3 Step 6). ✓
- Modo preview = só resolve + tonemap, sem bloom (§8.1) → `PostParams.bloom = 0` no `/p` e fallback (Task 5 Step 2-3). ✓
- Controles no diálogo (§10.3, efeitos) → Task 6 (só bloom nesta fase). ✓
- **Fora da 4a (Fase 4b):** streaks de difração, aberração cromática, vinheta, FXAA, `render_tiers`/`autoQuality`, `perf.renderScale`. Anotado.

**2. Placeholders:** sem "TODO"/"TBD". Tasks 3-4 compilam mas o efeito só aparece quando o Task 5 liga o `post` no `gl_window` — a ordem está explícita. Shaders vêm completos. Verificações produzem PNG comparável (com/sem bloom).

**3. Consistência de tipos:**
- `Config` (+`bloom_on` int, `bloom_threshold`/`bloom_intensity`/`bloom_radius` float) — Task 1; lidos por `gl_window_set_config` → `PostParams` (Task 5), escritos por `effects_proc` (Task 6). Faixas dos sliders (Task 6) batem com os clamps (Task 1).
- `GlFbo` — `gl_core.h` (Task 2), usado por `post.c` (Task 4). Campos `fbo/color/depth/w/h/ms`.
- `PostParams { int bloom; float threshold, intensity, radius; }` — `post.h` (Task 4), montado em `gl_window_frame` (Task 5) a partir de `g->post_params`.
- `Post` opaco — `post.h` (Task 4), `Post *` em `GlWindow` (Task 5). `post_create/begin/present/destroy` chamados em `gl_window.c` (Task 5).
- `EMBED_fullscreen_vert` / `EMBED_post_bright_frag` / `_down` / `_up` / `_composite` — gerados pelo `embed` (Task 3), consumidos em `post_create` (Task 4). Nome = basename com não-alfanum→`_`.
- `gl_window_create(..., int preview)` — assinatura nova (Task 5 Step 2); call sites: `host_run_saver` (0), `host_run_preview` (1), `config_dialog.c` mini-preview (0). Todos atualizados no Task 5.
- IDs `IDD_TAB_EFFECTS`/`IDC_BLOOM`/... — `resource.h` (Task 6); 5ª aba índice 4; `g_content/g_motion/g_material/g_geometry/g_effects` alternados.

Sem inconsistências.

---

## Execution Handoff

**Plano completo e salvo em `docs/superpowers/plans/2026-09-10-modern-3d-text-phase-4a-hdr-bloom.md`. Duas opções de execução:**

**1. Subagent-Driven (recomendado)** — subagente novo por task, revisão entre tasks.
**2. Inline Execution** — nesta sessão, com checkpoints.

**Qual abordagem?**

> Como sempre: build/headless aqui, com PNGs comparativos (bloom on/off, threshold alto/baixo) pra eu conferir. O QA interativo da aba Efeitos fica pra você. Sem downloads novos.
