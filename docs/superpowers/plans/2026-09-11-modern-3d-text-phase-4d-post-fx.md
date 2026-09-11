# Modern 3D Text — Fase 4d: aberração cromática + vinheta + FXAA — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fecha o §8.1 do spec (frame graph de pós-processamento). A cena +
bloom + streaks (Fases 4a-4c) ganham **aberração cromática** (deslocamento de
UV radial em R/G/B), **vinheta** (escurecimento multiplicativo nas bordas) e
**FXAA** (anti-serrilhado pós-tonemap) — os 3 com toggle individual, uma **7ª
aba "Pós"** no diálogo. A cadeia de composição é reestruturada em 3 passes
(era 1): **combinar** (cena+bloom+streaks, ainda HDR) → **finalizar** (CA +
vinheta + tonemap ACES + gamma) → **FXAA** (opcional, último passe, LDR→LDR).

**Architecture:** `post_composite.frag` é renomeado pra `post_combine.frag` e
perde o tonemap (vira só soma aditiva, ainda em HDR). Dois shaders novos:
`post_finish.frag` (CA + vinheta + ACES + gamma, opera no HDR combinado, saída
LDR) e `post_fxaa.frag` (o "poor man's FXAA" clássico — detecção de borda por
luma + blend direcional — opera na saída LDR do finish). `post.c` ganha duas
novas texturas dimensionadas pela resolução de **saída** (`out_w × out_h`, que
pode ser maior que o alvo *interno* quando `render_scale < 1`, ou diferente
dele quando o monitor redimensiona): `comp` (RGBA16F, o HDR combinado) e `ldr`
(RGBA16F reaproveitado, o resultado pós-tonemap-pré-FXAA). Um
`ensure_output_size()` novo (paralelo ao `ensure_size()` existente, que cuida
só do alvo *interno* da cena) cria/libera essas duas conforme `out_w`/`out_h`
mudam. `PostParams` ganha `chroma_on/chroma_strength/vignette_on/
vignette_amount/fxaa_on`. `gl_window` copia de `Config` e aplica o mesmo gate
de preview/reduced que bloom e streaks já usam — mas como CA/vinheta/FXAA
**não** entram no ladder de auto-qualidade (são baratos; o spec só lista bloom/
streaks/MSAA/renderScale como degraus adaptativos), o gate é direto
`!preview && tier != REDUCED`, sem tocar em `RenderQuality`.

**Tech Stack:** C11 · w64devkit · OpenGL 3.3 / GLSL 330 · reuso de
`config`/`post`/`gl_core`/`gl_window`/`config_dialog`/`embed`. Sem libs novas.

## Global Constraints

Do spec (§8.1 passos 7-8-10, §10.1 `effects.chroma`/`vignette`/`fxaa`) e do
estado pós-Fase 4c. Todo task herda esta seção.

- **Linguagem:** C11. Flags `-std=c11 -municode -Wall -Wextra` (+ `-O2 -DNDEBUG`
  release). **Sem `-ffast-math`.** Build **sem warnings** — warning é bug.
- **Toolchain:** só w64devkit em `C:\Users\alanm\w64devkit`. **Sem downloads
  novos.** Todo comando de shell começa com
  `export PATH="/c/Users/alanm/w64devkit/bin:$PATH"`.
- **API gráfica:** OpenGL 3.3 core. `comp`/`ldr` usam `gl_fbo_color16f(w,h,0)`
  (RGBA16F, sem depth) — reaproveita o helper existente em vez de criar um
  formato RGBA8 novo em `gl_core.c` (o LDR não precisa da precisão extra, mas
  não vale a pena abrir mais API de `gl_core` só por isso).
- **Ordem do frame graph** (spec §8.1, passos 6-10, já com bloom+streaks
  somados no passo 6 pela Fase 4c): **combinar** (cena+bloom·int+streaks·int,
  HDR) → **CA** (radial, em R/G/B, sobre o HDR) → **vinheta** (multiplicativa,
  sobre o HDR pós-CA) → **tonemap ACES + gamma 2.2** (→ LDR) → **FXAA**
  (opcional, sobre o LDR). CA e vinheta ficam **dentro do mesmo shader**
  (`post_finish.frag`) por serem baratos e sequenciais; só o FXAA é um passe
  GPU separado (precisa da imagem LDR já resolvida pra fazer detecção de
  borda por luma).
- **Registro:** só `HKCU\Software\Modern3DText` (testes: `..._test`). Nada fora
  disso e de `%LOCALAPPDATA%\Modern3DText\`.
- **`.scr` ≤ 3 MB.** i18n só na Fase 8. **Sem bump de `CFG_VERSION`** (fica 2 —
  mesmo padrão da Fase 4c: campo ausente no registro → default, sem migração
  dedicada).
- **Modo preview (`/p`) e tier REDUCED:** sem CA/vinheta/FXAA — mesmo
  tratamento que bloom e streaks. O **mini-preview do diálogo roda a cadeia
  completa** (decisão da Fase 4c, documentada no spec §8.1 já corrigido) — só
  `/p` e REDUCED simplificam. CA/vinheta/FXAA **não entram no ladder de
  auto-qualidade** (não há degrau novo em `render_quality_for_step`; o ladder
  continua com 8 passos da Fase 4c). O gate é `!g->preview && g->tier !=
  M3DT_TIER_REDUCED`, calculado direto em `gl_window.c` — não usa
  `RenderQuality`.
- **Fora desta fase, adiado (decisão do usuário 2026-09-11):** o comprimento
  do streak (Fase 4c) continuar sendo absoluto em texels do buffer, não
  relativo à tela/`render_scale` — já documentado em
  `docs/qa-checklist-phase-4c.md`. Não mexer nisso aqui.
- **Layout do diálogo (decisão do usuário):** CA/vinheta/FXAA ganham uma **7ª
  aba "Pós"** dedicada — a aba Efeitos (Fase 4c) já está cheia com Bloom +
  Streaks. `TCS_MULTILINE` já está ligado no tab control (infra da Fase 4b);
  com 7 abas o controle deve quebrar em 2 linhas sozinho, sem mudança de
  layout necessária.
- **Acoplamento shader↔C:** o `post_composite.frag` vira `post_combine.frag`
  (renomeado, papel diferente: só soma, sem tonemap). Isso **quebra a build**
  até `post.c` ser atualizado no mesmo task — por isso o rename do shader e a
  reestrutura de `post.c`/`post.h` estão no **mesmo task (Task 2)**, um único
  commit. Não faça o rename isoladamente.
- **Commits frequentes**, um por task. TDD em `config` (campos novos). GL: PNG
  conferível (CA on/off, vinheta on/off, FXAA on/off, e a combinação dos 3).
- **Atribuição:** todo commit termina com
  `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>`.
- **`.exe` gotcha:** o build só atualiza `dist/Modern3DText.scr`. Antes de
  rodar o binário direto: `cp -f dist/Modern3DText.scr dist/Modern3DText.exe`.
  MSYS mastiga `/s` → `MSYS_NO_PATHCONV=1`.
- **Hooks de teste** reusados, nenhum novo nesta fase: `M3DT_SELFTEST=1`,
  `M3DT_SHOT=<path>` + `M3DT_SHOT_T=<s>`, `M3DT_LOG_APPEND=1`,
  `M3DT_TAB=<0..6>` (sobe de `<0..5>` pra caber a 7ª aba), `M3DT_HOLD_MS=<ms>`,
  `M3DT_FORCE_TIER`, `M3DT_AQ_FORCE_MS`/`M3DT_AQ_FAST`.
- **Captura de diálogo:** o script `shot_dialog.ps1` (Fase 4a, no scratchpad da
  sessão) já suporta `-Tab N`.

---

## File Structure

| Arquivo | Responsabilidade |
|---|---|
| `src/config.h` / `.c` | (+) `int chroma_on; float chroma_strength; int vignette_on; float vignette_amount; int fxaa_on;` após `streaks_length`. Defaults, clamps, chaves REG_SZ. |
| `shaders/post_composite.frag` → **renomeado** `shaders/post_combine.frag` | Perde `aces()`/gamma; vira só a soma aditiva (cena+bloom+streaks), saída HDR. |
| `shaders/post_finish.frag` | **novo.** CA (radial R/G/B) + vinheta (multiplicativa) + ACES + gamma. Entrada HDR, saída LDR. |
| `shaders/post_fxaa.frag` | **novo.** FXAA clássico por luma (5 amostras + blend direcional). Entrada/saída LDR. |
| `build/Makefile` | `EMBED_INPUTS`: troca `post_composite.frag` por `post_combine.frag`, `+ post_finish.frag post_fxaa.frag`. |
| `src/post.h` / `.c` | `PostParams` (+) os 5 campos. `Post` (+) `GlFbo comp, ldr; unsigned prog_finish, prog_fxaa; int out_w, out_h;` (renomeia `prog_comp`→`prog_combine`, `EMBED_post_composite_frag`→`EMBED_post_combine_frag`). `ensure_output_size()` novo. `post_present`: combine→(CA+vinheta+tonemap)→FXAA opcional. |
| `src/gl_window.c` | `gl_window_set_config` copia `cfg->chroma_*/vignette_*/fxaa_on` pra `g->post_params`. `render_into_post`: gate direto (sem `RenderQuality`) `!g->preview && g->tier != M3DT_TIER_REDUCED`. `gl_window_create`'s ramo `cfg==NULL` ganha `g->post_params.fxaa_on = 1;` (mesmo default de `config_defaults`) por clareza. |
| `build/tests/test_config.c` | (+) round-trip + clamps dos 5 campos novos. |
| `src/resource.h`, `res/screensaver.rc` | (+) `IDD_TAB_POST 116`, IDs `IDC_CHROMA/IDC_CSTR/IDC_CSTR_VAL/IDC_VIGNETTE/IDC_VAMT/IDC_VAMT_VAL/IDC_FXAA` (1700+). 7ª aba. |
| `src/config_dialog.c` | `g_post` sub-diálogo + `post_proc`; `select_tab` até 6; `M3DT_TAB` clamp até 6; `TabCtrl_InsertItem` índice 6. |
| `docs/qa-checklist-phase-4d.md`, `README.md`, `res/screensaver.rc` (versão) | novo checklist; status → 4d; versão 0.4.3. |

---

## Task 1: Config — campos de CA/vinheta/FXAA (TDD)

**Files:** `src/config.h`, `src/config.c`, `build/tests/test_config.c`

**Interfaces — `Config` ganha (após `streaks_length`):**
```c
int   chroma_on;          /* 0/1 */
float chroma_strength;    /* 0 .. 1 */
int   vignette_on;        /* 0/1 */
float vignette_amount;    /* 0 .. 1 */
int   fxaa_on;             /* 0/1 */
```
Padrões: `chroma_on = 0`, `chroma_strength = 0.4f`, `vignette_on = 0`,
`vignette_amount = 0.35f`, `fxaa_on = 1` (spec + decisão da Fase 4c: FXAA
ligado por padrão; CA e vinheta desligados, mesmo espírito do preset
"Clássico" — bloom sutil, sem CA/streaks/vinheta). Clamps: `chroma_on`/
`vignette_on`/`fxaa_on` → `!= 0 ? 1 : 0`; `chroma_strength`/`vignette_amount`
→ `clampf(0, 1)`. **Sem** bump de `CFG_VERSION` (fica `2`; registro sem essas
chaves carrega com os defaults acima via `config_defaults`).

- [ ] **Step 1: `build/tests/test_config.c`** — no bloco round-trip existente
  (o mesmo que já seta `streaks_mode` etc. antes do `config_save_to(&a,
  TESTKEY)`), adicionar:
```c
    a.chroma_on = 1;
    a.chroma_strength = 0.65f;
    a.vignette_on = 1;
    a.vignette_amount = 0.5f;
    a.fxaa_on = 0;
```
  e depois do `config_load_from(&b, TESTKEY)`:
```c
    EXPECT(b.chroma_on == 1);
    EXPECT(nearf(b.chroma_strength, 0.65f));
    EXPECT(b.vignette_on == 1);
    EXPECT(nearf(b.vignette_amount, 0.5f));
    EXPECT(b.fxaa_on == 0);
```
  No bloco de clamp (o que já grava `streaks_mode`="9" etc. via `kv[]` e lê
  pra `Config e`), estender o array `kv[]` (crescer o tamanho declarado e o
  bound do `for`) com:
```c
        { L"chroma_on", L"7" }, { L"chroma_strength", L"9" },
        { L"vignette_amount", L"-3" }, { L"fxaa_on", L"5" },
```
  e depois do `config_load_from(&e, TESTKEY)`:
```c
    EXPECT(e.chroma_on == 1);                    /* 7 -> !=0 -> 1 */
    EXPECT(e.chroma_strength <= 1.0f);            /* 9 -> clamp */
    EXPECT(e.vignette_amount >= 0.0f);            /* -3 -> clamp */
    EXPECT(e.fxaa_on == 1);                       /* 5 -> !=0 -> 1 */
```

- [ ] **Step 2: rodar — falha** (`'Config' has no member named 'chroma_on'`).
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```

- [ ] **Step 3: `src/config.h`** — os 5 campos após `streaks_length`, com os
  comentários de faixa acima. `src/config.c`:
  - `config_defaults`: `c->chroma_on = 0; c->chroma_strength = 0.4f;
    c->vignette_on = 0; c->vignette_amount = 0.35f; c->fxaa_on = 1;`
  - `config_load_from`, junto das leituras de `streaks_*`:
```c
    reg_get_i(k, L"chroma_on", &c->chroma_on);
    if (reg_get_f(k, L"chroma_strength", &f)) c->chroma_strength = f;
    reg_get_i(k, L"vignette_on", &c->vignette_on);
    if (reg_get_f(k, L"vignette_amount", &f)) c->vignette_amount = f;
    reg_get_i(k, L"fxaa_on", &c->fxaa_on);
```
  - saneamento, junto do bloco de `streaks_*`:
```c
    c->chroma_on = c->chroma_on ? 1 : 0;
    c->chroma_strength = clampf(c->chroma_strength, 0.0f, 1.0f);
    c->vignette_on = c->vignette_on ? 1 : 0;
    c->vignette_amount = clampf(c->vignette_amount, 0.0f, 1.0f);
    c->fxaa_on = c->fxaa_on ? 1 : 0;
```
  - `config_save_to`, junto dos `set_f` de `streaks_*`:
```c
    set_f(k, L"chroma_on", (float)c->chroma_on);
    set_f(k, L"chroma_strength", c->chroma_strength);
    set_f(k, L"vignette_on", (float)c->vignette_on);
    set_f(k, L"vignette_amount", c->vignette_amount);
    set_f(k, L"fxaa_on", (float)c->fxaa_on);
```

- [ ] **Step 4: rodar — passa.**
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```

- [ ] **Step 5: Commit**
```sh
git add src/config.h src/config.c build/tests/test_config.c
git commit -m "$(printf 'feat: config chroma/vignette/fxaa fields\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 2: Shaders (combine/finish/fxaa) + `post.c`/`post.h` — reestrutura em 3 passes

**Files:** `shaders/post_composite.frag` (renomeado → `shaders/post_combine.frag`),
`shaders/post_finish.frag` (novo), `shaders/post_fxaa.frag` (novo),
`build/Makefile`, `src/post.h`, `src/post.c`

Um único task/commit: o rename do shader quebra a build até `post.c` ser
atualizado (o embed gera `EMBED_post_combine_frag`, e `post.c` ainda diria
`EMBED_post_composite_frag`), então shader + C andam juntos.

**Interfaces — `PostParams` ganha (após `streaks_length`):**
```c
int   chroma_on;
float chroma_strength;    /* 0 .. 1 */
int   vignette_on;
float vignette_amount;    /* 0 .. 1 */
int   fxaa_on;
```

- [ ] **Step 1: renomear** `shaders/post_composite.frag` para
  `shaders/post_combine.frag` (`git mv`) e reescrever pro papel novo (só soma,
  sem tonemap):
```glsl
#version 330 core
in vec2 vUV;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform sampler2D uStreaks;
uniform float uBloomIntensity;
uniform float uStreaksIntensity;
uniform int   uHasBloom;
uniform int   uHasStreaks;
out vec4 o;

void main()
{
    vec3 c = texture(uScene, vUV).rgb;
    if (uHasBloom == 1)   c += texture(uBloom, vUV).rgb   * uBloomIntensity;
    if (uHasStreaks == 1) c += texture(uStreaks, vUV).rgb * uStreaksIntensity;
    o = vec4(c, 1.0);
}
```

- [ ] **Step 2: `shaders/post_finish.frag`** — CA + vinheta + tonemap:
```glsl
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform int   uHasChroma;
uniform float uChromaStrength;   // 0..1 do usuario
uniform int   uHasVignette;
uniform float uVignetteAmount;   // 0..1
out vec4 o;

vec3 aces(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
    vec3 c;
    if (uHasChroma == 1) {
        // deslocamento radial em R/G/B: off = dir_do_centro * strength * r^2
        // (spec 8.1 passo 7). strength do usuario (0..1) escalado por 0.02
        // pra manter o deslocamento em fracao pequena de tela.
        vec2  d  = vUV - vec2(0.5);
        float r2 = dot(d, d);
        vec2  off = d * (uChromaStrength * 0.02) * r2;
        c.r = texture(uTex, vUV - off).r;
        c.g = texture(uTex, vUV).g;
        c.b = texture(uTex, vUV + off).b;
    } else {
        c = texture(uTex, vUV).rgb;
    }

    if (uHasVignette == 1) {
        vec2  d = vUV - vec2(0.5);
        float r = length(d) * 1.4142136;             // 0 no centro, ~1 no canto
        float v = 1.0 - uVignetteAmount * smoothstep(0.25, 1.0, r);
        c *= clamp(v, 0.0, 1.0);
    }

    c = aces(c);
    c = pow(c, vec3(1.0 / 2.2));
    o = vec4(c, 1.0);
}
```

- [ ] **Step 3: `shaders/post_fxaa.frag`** — FXAA clássico (5 amostras, luma,
  blend direcional):
```glsl
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2  uTexel;
out vec4 o;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main()
{
    vec3 rgbM  = texture(uTex, vUV).rgb;
    vec3 rgbNW = texture(uTex, vUV + vec2(-1.0, -1.0) * uTexel).rgb;
    vec3 rgbNE = texture(uTex, vUV + vec2( 1.0, -1.0) * uTexel).rgb;
    vec3 rgbSW = texture(uTex, vUV + vec2(-1.0,  1.0) * uTexel).rgb;
    vec3 rgbSE = texture(uTex, vUV + vec2( 1.0,  1.0) * uTexel).rgb;

    float lM  = luma(rgbM);
    float lNW = luma(rgbNW), lNE = luma(rgbNE);
    float lSW = luma(rgbSW), lSE = luma(rgbSE);

    vec2 dir;
    dir.x = -((lNW + lNE) - (lSW + lSE));
    dir.y =   (lNW + lSW) - (lNE + lSE);

    float dirReduce = max((lNW + lNE + lSW + lSE) * 0.03125, 1.0 / 128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, -8.0, 8.0) * uTexel;

    vec3 rgbA = 0.5 * (
        texture(uTex, vUV + dir * (1.0 / 3.0 - 0.5)).rgb +
        texture(uTex, vUV + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(uTex, vUV + dir * -0.5).rgb +
        texture(uTex, vUV + dir *  0.5).rgb);

    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
    float lB   = luma(rgbB);

    o = vec4((lB < lMin || lB > lMax) ? rgbA : rgbB, 1.0);
}
```

- [ ] **Step 4: `build/Makefile` — `EMBED_INPUTS`** troca `post_composite.frag`
  por `post_combine.frag` e adiciona os dois novos:
```make
EMBED_INPUTS := shaders/model.vert shaders/model.frag shaders/fullscreen.vert \
                shaders/post_bright.frag shaders/post_down.frag shaders/post_up.frag \
                shaders/post_combine.frag shaders/post_streak.frag \
                shaders/post_finish.frag shaders/post_fxaa.frag
```

- [ ] **Step 5: `post.h`** — os 5 campos novos em `PostParams` (após
  `streaks_length`).

- [ ] **Step 6: `post.c` — `struct Post`**: renomear `unsigned prog_comp;` →
  `unsigned prog_combine;`; adicionar `GlFbo comp, ldr; unsigned prog_finish,
  prog_fxaa; int out_w, out_h;`.

- [ ] **Step 7: `post_create`** — troca `EMBED_post_composite_frag` por
  `EMBED_post_combine_frag` (variável renomeada `prog_combine`); compila
  `prog_finish` e `prog_fxaa`:
```c
    p->prog_combine = prog(EMBED_post_combine_frag);
    p->prog_finish  = prog(EMBED_post_finish_frag);
    p->prog_fxaa    = prog(EMBED_post_fxaa_frag);
    if (!p->prog_bright || !p->prog_down || !p->prog_up || !p->prog_combine ||
        !p->prog_streak || !p->prog_finish || !p->prog_fxaa) {
        post_destroy(p);
        return NULL;
    }
```
  e nos uniforms de amostra (troca o bloco `glUseProgram(p->prog_comp); ...`
  por):
```c
    glUseProgram(p->prog_combine);
    glUniform1i(glGetUniformLocation(p->prog_combine, "uScene"), 0);
    glUniform1i(glGetUniformLocation(p->prog_combine, "uBloom"), 1);
    glUniform1i(glGetUniformLocation(p->prog_combine, "uStreaks"), 2);
    glUseProgram(p->prog_finish);
    glUniform1i(glGetUniformLocation(p->prog_finish, "uTex"), 0);
    glUseProgram(p->prog_fxaa);
    glUniform1i(glGetUniformLocation(p->prog_fxaa, "uTex"), 0);
```

- [ ] **Step 8: `ensure_output_size` novo**, logo depois de `ensure_size`:
```c
static void ensure_output_size(Post *p, int w, int h)
{
    if (p->out_w == w && p->out_h == h && p->comp.fbo) return;
    p->out_w = w; p->out_h = h;
    gl_fbo_free(&p->comp);
    gl_fbo_free(&p->ldr);
    p->comp = gl_fbo_color16f(w, h, 0);
    p->ldr  = gl_fbo_color16f(w, h, 0);
    if (!p->comp.fbo || !p->ldr.fbo) log_errorf("post: FBO combine/ldr incompleto");
}
```

- [ ] **Step 9: `post_present`** — logo após o clamp de `out_w`/`out_h` no
  topo (antes do `if (p->ms_on) gl_blit_resolve(...)`), chamar
  `ensure_output_size(p, out_w, out_h);`.

- [ ] **Step 10: `post_present` — substituir o bloco final** (a "composição ->
  framebuffer padrao" que hoje faz tudo num passe só) pelos 3 passes. O bloco
  atual termina assim (depois do `if (has_streaks) { ... }`):
```c
    /* composicao -> framebuffer padrao (faz upscale se in < out) */
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, out_w, out_h);
    glUseProgram(p->prog_comp);
    glUniform1i(glGetUniformLocation(p->prog_comp, "uHasBloom"), has_bloom);
    glUniform1f(glGetUniformLocation(p->prog_comp, "uBloomIntensity"), pr.intensity);
    glUniform1i(glGetUniformLocation(p->prog_comp, "uHasStreaks"), has_streaks);
    glUniform1f(glGetUniformLocation(p->prog_comp, "uStreaksIntensity"), pr.streaks_intensity);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, p->hdr.color);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, has_bloom ? p->bloom[0].color : p->hdr.color);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, has_streaks ? p->streaks_acc.color : p->hdr.color);
    gl_fullscreen_draw();
    glActiveTexture(GL_TEXTURE0);

    glEnable(GL_DEPTH_TEST);
```
  Substituir por:
```c
    /* combinar -> p->comp (HDR, out_w x out_h; faz upscale se interno < saida) */
    gl_fbo_bind(&p->comp);
    glUseProgram(p->prog_combine);
    glUniform1i(glGetUniformLocation(p->prog_combine, "uHasBloom"), has_bloom);
    glUniform1f(glGetUniformLocation(p->prog_combine, "uBloomIntensity"), pr.intensity);
    glUniform1i(glGetUniformLocation(p->prog_combine, "uHasStreaks"), has_streaks);
    glUniform1f(glGetUniformLocation(p->prog_combine, "uStreaksIntensity"), pr.streaks_intensity);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, p->hdr.color);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, has_bloom ? p->bloom[0].color : p->hdr.color);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, has_streaks ? p->streaks_acc.color : p->hdr.color);
    gl_fullscreen_draw();

    /* finalizar: CA + vinheta + tonemap ACES + gamma. Sem FXAA -> escreve
       direto no framebuffer padrao; com FXAA -> escreve em p->ldr, o FXAA
       resolve pro framebuffer padrao depois. */
    int has_chroma   = (pr.chroma_on   && pr.chroma_strength  > 1e-4f) ? 1 : 0;
    int has_vignette = (pr.vignette_on && pr.vignette_amount  > 1e-4f) ? 1 : 0;
    int has_fxaa     = pr.fxaa_on ? 1 : 0;

    if (has_fxaa) {
        gl_fbo_bind(&p->ldr);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, out_w, out_h);
    }
    glUseProgram(p->prog_finish);
    glUniform1i(glGetUniformLocation(p->prog_finish, "uHasChroma"), has_chroma);
    glUniform1f(glGetUniformLocation(p->prog_finish, "uChromaStrength"), pr.chroma_strength);
    glUniform1i(glGetUniformLocation(p->prog_finish, "uHasVignette"), has_vignette);
    glUniform1f(glGetUniformLocation(p->prog_finish, "uVignetteAmount"), pr.vignette_amount);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, p->comp.color);
    gl_fullscreen_draw();

    /* FXAA opcional: p->ldr -> framebuffer padrao */
    if (has_fxaa) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, out_w, out_h);
        glUseProgram(p->prog_fxaa);
        set_texel(p->prog_fxaa, &p->ldr);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, p->ldr.color);
        gl_fullscreen_draw();
    }

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
```

- [ ] **Step 11: `post_destroy`** — libera `p->comp`/`p->ldr`; renomeia
  `if (p->prog_comp) glDeleteProgram(p->prog_comp);` → `prog_combine`; soma
  `if (p->prog_finish) glDeleteProgram(p->prog_finish);` e idem `prog_fxaa`.

- [ ] **Step 12: build.**
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile 2>&1 | grep -Ei "warning:|error:|Built"
mingw32-make -f build/Makefile test 2>&1 | tail -1
```
  Deve dar **verde e sem warnings** — este é o primeiro ponto em que o código
  volta a compilar depois do rename do Step 1 (por isso shader + C estão no
  mesmo task). Ainda sem efeito visível de CA/vinheta — `PostParams.chroma_on`
  /`vignette_on` chegam sempre 0 até o Task 3 (ninguém em `gl_window.c` ainda
  copia esses campos de `Config`); `fxaa_on` idem (fica 0 por default de
  struct até o Task 3 ligar a cópia).

- [ ] **Step 13: Commit**
```sh
git add shaders/post_combine.frag shaders/post_finish.frag shaders/post_fxaa.frag build/Makefile src/post.h src/post.c
git rm --cached shaders/post_composite.frag 2>/dev/null || true
git commit -m "$(printf 'feat: post - split composite into combine/finish/fxaa passes (CA + vignette + FXAA)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 3: Ligar no `gl_window` (gate de preview/reduced)

**Files:** `src/gl_window.c`

- [ ] **Step 1: `gl_window_set_config`** — junto da cópia de `streaks_*` pra
  `g->post_params`:
```c
    g->post_params.chroma_on        = cfg->chroma_on;
    g->post_params.chroma_strength  = cfg->chroma_strength;
    g->post_params.vignette_on      = cfg->vignette_on;
    g->post_params.vignette_amount  = cfg->vignette_amount;
    g->post_params.fxaa_on          = cfg->fxaa_on;
```

- [ ] **Step 2: `gl_window_create`'s ramo `cfg == NULL`** (o `else` que hoje
  seta `bloom`/`threshold`/etc. e `streaks_mode = 0`) — adicionar
  `g->post_params.fxaa_on = 1;` (mesmo default de `config_defaults`; os outros
  4 campos novos já ficam 0 pelo `calloc` do `GlWindow`, que é o comportamento
  certo — CA e vinheta desligados por padrão).

- [ ] **Step 3: `render_into_post`** — depois da linha `pr.streaks_mode = ...`,
  adicionar:
```c
    int post_fx_ok = !g->preview && g->tier != M3DT_TIER_REDUCED;
    pr.chroma_on   = (post_fx_ok && g->post_params.chroma_on)   ? 1 : 0;
    pr.vignette_on = (post_fx_ok && g->post_params.vignette_on) ? 1 : 0;
    pr.fxaa_on     = (post_fx_ok && g->post_params.fxaa_on)     ? 1 : 0;
```
  (`chroma_strength`/`vignette_amount` já vêm certos de `g->post_params` via
  `PostParams pr = g->post_params;` no topo da função — só os 3 `_on` precisam
  do gate.)

- [ ] **Step 4: build + captura comparativa.**
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile && cp -f dist/Modern3DText.scr dist/Modern3DText.exe
S="C:/Users/alanm/AppData/Local/Temp/claude/<sessao>/scratchpad"   # ajuste pro path real
BASE="material_mode 1;metalness 1;roughness 0.14;base_color #C8CED8;bloom_on 1;bloom_intensity 0.7;env_path $S/testenv.png;max_angle_y 28;fxaa_on 0"
setcfg() { powershell -Command "New-Item 'HKCU:\Software\Modern3DText' -Force|Out-Null; '$1'.Split(';')|%{ \$p=\$_.Split(' ',2); Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name \$p[0] -Value \$p[1] -Type String }"; }
setcfg "$BASE;chroma_on 0;vignette_on 0"
MSYS_NO_PATHCONV=1 M3DT_LOG_APPEND=1 M3DT_SELFTEST=1 M3DT_SHOT="$S/t3_none.png" M3DT_SHOT_T=2.4 ./dist/Modern3DText.exe /s
setcfg "$BASE;chroma_on 1;chroma_strength 0.9;vignette_on 0"
MSYS_NO_PATHCONV=1 M3DT_LOG_APPEND=1 M3DT_SELFTEST=1 M3DT_SHOT="$S/t3_chroma.png" M3DT_SHOT_T=2.4 ./dist/Modern3DText.exe /s
setcfg "$BASE;chroma_on 0;vignette_on 1;vignette_amount 0.7"
MSYS_NO_PATHCONV=1 M3DT_LOG_APPEND=1 M3DT_SELFTEST=1 M3DT_SHOT="$S/t3_vignette.png" M3DT_SHOT_T=2.4 ./dist/Modern3DText.exe /s
setcfg "$BASE;chroma_on 1;chroma_strength 0.9;vignette_on 1;vignette_amount 0.7;fxaa_on 1"
MSYS_NO_PATHCONV=1 M3DT_LOG_APPEND=1 M3DT_SELFTEST=1 M3DT_SHOT="$S/t3_all.png" M3DT_SHOT_T=2.4 ./dist/Modern3DText.exe /s
powershell -Command "Remove-Item 'HKCU:\Software\Modern3DText' -Recurse -Force"
```
  **Conferir os 4 PNGs:**
  - `t3_none`: idêntico ao baseline da 4c (nada novo ligado).
  - `t3_chroma`: franjas coloridas (vermelho/azul separados) crescendo em
    direção às bordas/cantos, quase nada no centro (o `r²` no shader garante
    isso). Sem quebra visual.
  - `t3_vignette`: cantos visivelmente mais escuros, centro inalterado.
  - `t3_all`: os dois efeitos juntos + FXAA (bordas do texto ligeiramente
    mais suaves comparado a MSAA puro — comparar de perto/com zoom).
  Sem `glError` / FBO incompleto no log. Testar também
  `M3DT_FORCE_TIER=reduced` → **sem** CA/vinheta/FXAA mesmo com os 3 ligados
  no registro (confirmar via captura ou log).

- [ ] **Step 5: regressão + estabilidade.**
```sh
mingw32-make -f build/Makefile test         # all tests passed
for i in $(seq 1 10); do MSYS_NO_PATHCONV=1 M3DT_SELFTEST=1 ./dist/Modern3DText.exe /s >/dev/null 2>&1 || echo FAIL $i; done
for i in $(seq 1 10); do MSYS_NO_PATHCONV=1 M3DT_SELFTEST=1 ./dist/Modern3DText.exe /c >/dev/null 2>&1 || echo FAIL $i; done
```

- [ ] **Step 6: Commit**
```sh
git add src/gl_window.c
git commit -m "$(printf 'feat: wire chroma/vignette/fxaa into gl_window (preview and reduced tier skip them)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 4: 7ª aba "Pós" no diálogo

**Files:** `src/resource.h`, `res/screensaver.rc`, `src/config_dialog.c`

**IDs novos (`resource.h`, nova seção "aba Pós"):**
```c
#define IDD_TAB_POST      116

#define IDC_CHROMA        1700
#define IDC_CSTR          1701
#define IDC_CSTR_VAL      1702
#define IDC_VIGNETTE      1703
#define IDC_VAMT          1704
#define IDC_VAMT_VAL      1705
#define IDC_FXAA          1706
```

- [ ] **Step 1: `res/screensaver.rc`** — `IDD_TAB_POST DIALOGEX 0, 0, 240, 200`
  (mesmo tamanho dos outros templates de aba; nenhuma mudança no `IDD_CONFIG`
  nem no tab control — `TCS_MULTILINE` já cuida da 7ª aba), inserida antes do
  `VS_VERSION_INFO`:
```
IDD_TAB_POST DIALOGEX 0, 0, 240, 200
STYLE DS_SETFONT | DS_CONTROL | WS_CHILD
FONT 9, "Segoe UI", 400, 0, 0x1
BEGIN
    AUTOCHECKBOX "Aberracao cromatica", IDC_CHROMA, 8, 8, 160, 12
    LTEXT      "Intensidade:", -1, 8, 24, 80, 9
    LTEXT      "", IDC_CSTR_VAL, 182, 24, 50, 9
    CONTROL    "", IDC_CSTR, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 33, 224, 14

    AUTOCHECKBOX "Vinheta", IDC_VIGNETTE, 8, 55, 160, 12
    LTEXT      "Intensidade:", -1, 8, 71, 80, 9
    LTEXT      "", IDC_VAMT_VAL, 182, 71, 50, 9
    CONTROL    "", IDC_VAMT, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 80, 224, 14

    AUTOCHECKBOX "FXAA (anti-serrilhado)", IDC_FXAA, 8, 102, 200, 12

    LTEXT      "Aberracao cromatica e vinheta agem antes do tonemap; o FXAA e o ultimo passe, depois do tonemap.",
               -1, 8, 124, 224, 20
END
```

- [ ] **Step 2: `src/config_dialog.c`.**
  - `static HWND g_post;` junto dos outros `g_*` (perto de `g_perf`).
  - `select_tab(int sel)` — (+) `ShowWindow(g_post, sel == 6 ? SW_SHOW : SW_HIDE);`
  - `post_labels(HWND h)`:
```c
static void post_labels(HWND h)
{
    wchar_t b[32];
    swprintf(b, 32, L"%.2f", (double)g_work.chroma_strength);  SetDlgItemTextW(h, IDC_CSTR_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.vignette_amount);  SetDlgItemTextW(h, IDC_VAMT_VAL, b);
}
```
  - `post_enable(HWND h)`:
```c
static void post_enable(HWND h)
{
    EnableWindow(GetDlgItem(h, IDC_CSTR), g_work.chroma_on ? TRUE : FALSE);
    EnableWindow(GetDlgItem(h, IDC_VAMT), g_work.vignette_on ? TRUE : FALSE);
}
```
  - `post_proc`:
```c
static INT_PTR CALLBACK post_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG:
            CheckDlgButton(h, IDC_CHROMA, g_work.chroma_on ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(h, IDC_VIGNETTE, g_work.vignette_on ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(h, IDC_FXAA, g_work.fxaa_on ? BST_CHECKED : BST_UNCHECKED);
            set_slider(h, IDC_CSTR, 0, 100, (int)(g_work.chroma_strength * 100.0f + 0.5f));
            set_slider(h, IDC_VAMT, 0, 100, (int)(g_work.vignette_amount * 100.0f + 0.5f));
            post_labels(h);
            post_enable(h);
            return TRUE;
        case WM_HSCROLL:
            g_work.chroma_strength = (float)SendDlgItemMessageW(h, IDC_CSTR, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.vignette_amount = (float)SendDlgItemMessageW(h, IDC_VAMT, TBM_GETPOS, 0, 0) / 100.0f;
            post_labels(h);
            preview_dirty(h);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDC_CHROMA:
                    g_work.chroma_on = (IsDlgButtonChecked(h, IDC_CHROMA) == BST_CHECKED);
                    post_enable(h);
                    preview_dirty(h);
                    break;
                case IDC_VIGNETTE:
                    g_work.vignette_on = (IsDlgButtonChecked(h, IDC_VIGNETTE) == BST_CHECKED);
                    post_enable(h);
                    preview_dirty(h);
                    break;
                case IDC_FXAA:
                    g_work.fxaa_on = (IsDlgButtonChecked(h, IDC_FXAA) == BST_CHECKED);
                    preview_dirty(h);
                    break;
            }
            return TRUE;
    }
    return FALSE;
}
```
  - No `WM_INITDIALOG` do `dlg_proc`: `ti.pszText = L"Pos"; TabCtrl_InsertItem(tabs,
    6, &ti);` (depois do `Desempenho`, índice 5); criar `g_post = CreateDialogW(...,
    MAKEINTRESOURCEW(IDD_TAB_POST), h, post_proc);`; `place_tab_child(h, tabs,
    g_post);` (o `select_tab(0)` já esconde).
  - `M3DT_TAB` clamp: `if (sel > 5) sel = 5;` → `if (sel > 6) sel = 6;`.

- [ ] **Step 3: build + selftest de todas as 7 abas + captura.**
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile && cp -f dist/Modern3DText.scr dist/Modern3DText.exe
for t in 0 1 2 3 4 5 6; do MSYS_NO_PATHCONV=1 M3DT_SELFTEST=1 M3DT_TAB=$t ./dist/Modern3DText.exe /c >/dev/null 2>&1 || echo "FAIL tab $t"; done
powershell -Command "New-Item 'HKCU:\Software\Modern3DText' -Force|Out-Null; Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name chroma_on -Value '1' -Type String; Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name vignette_on -Value '1' -Type String"
powershell -ExecutionPolicy Bypass -File "$S/shot_dialog.ps1" -Tab 6 -Out "<sessao>/t4_post_tab.png"
powershell -Command "Remove-Item 'HKCU:\Software\Modern3DText' -Recurse -Force"
```
  Conferir `t4_post_tab.png`: 7 abas (quebrando em 2 linhas se não couberem
  numa só — conferir visualmente, sem corte de texto), checks CA/Vinheta
  marcados com os sliders **habilitados**, FXAA marcado, nada cortado na aba.
  Se as 7 abas não couberem legíveis mesmo em 2 linhas, **não redimensionar o
  diálogo sem avisar** — reportar como concern (a decisão foi confiar no
  `TCS_MULTILINE` existente).

- [ ] **Step 4: `make test`** → `all tests passed`.

- [ ] **Step 5: Commit**
```sh
git add src/resource.h res/screensaver.rc src/config_dialog.c
git commit -m "$(printf 'feat: config dialog Post tab (chromatic aberration, vignette, FXAA)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 5: integração, QA da Fase 4d, tag

**Files:** `docs/qa-checklist-phase-4d.md` (novo), `README.md`,
`res/screensaver.rc` (versão → 0.4.3), `docs/img/phase4d-*.png`

- [ ] **Step 1:** `make` (release, clean antes) + `make debug` (clean antes) +
  `make test` verdes, **sem warnings**; `.scr` < 3 MB (esperado ~412 KB).

- [ ] **Step 2: capturas `docs/img/`** (metálico + env de teste):
  - `phase4d-none.png` / `phase4d-chroma.png` / `phase4d-vignette.png` /
    `phase4d-all.png` (reusar os 4 cenários do Task 3 Step 4, capturados de
    novo com o build final).
  - `phase4d-post-tab.png` — a aba Pós (`shot_dialog.ps1 -Tab 6`).
  Conferir os 5.

- [ ] **Step 3: `docs/qa-checklist-phase-4d.md`** — formato dos anteriores
  (`docs/qa-checklist-phase-4c.md`):
  - **Headless (dev):** `make`/`debug`/`test` verdes sem warning; `.scr`
    tamanho. `test_config` (round-trip + clamps dos 5 campos novos). Capturas:
    CA = franjas cromáticas crescendo pro canto (quase nada no centro), vinheta
    = cantos escuros, FXAA = bordas mais suaves (comparação de perto). `nenhum`
    = idêntico ao baseline 4c. `M3DT_FORCE_TIER=reduced` + os 3 ligados → PNG
    sem nenhum dos 3 (mesmo comportamento de bloom/streaks). Sem `glError`/FBO
    incompleto. 10× `/s` + 10× `/c`.
  - **Interativo (olho humano):** `/c` aba **Pós** — cada controle reflete no
    mini-preview; checks desabilitam os sliders correspondentes; `OK` grava
    (`regedit`, REG_SZ). `/p` sem os 3 efeitos. Multi-monitor. 100/150/200% DPI
    — 7 abas sem corte (a mais provável de quebrar linha agora). 5 min `/s`
    com tudo ligado — sem vazar VRAM (2 FBOs novos em resolução de *saída*,
    não *interna* — maiores que os de bloom/streak quando `render_scale<1`).
  - **Notas conhecidas:** streak length ainda absoluto em texels (adiado da
    4c, ver `docs/qa-checklist-phase-4c.md`). §8.1 do spec está **fechado**
    com esta fase (todos os 10 passos do frame graph implementados). Próximas
    fases (5+) não tocam mais a cadeia de pós — mexem em conteúdo/fundo/
    partículas/malha/i18n/distribuição.

- [ ] **Step 4: `README.md`** — bloco **Status** → Fase 4d (CA + vinheta +
  FXAA, aba Pós, frame graph do §8.1 completo).

- [ ] **Step 5: versão** — `res/screensaver.rc`: `FILEVERSION 0,4,3,0`,
  `PRODUCTVERSION 0,4,3,0`, strings `"0.4.3.0"`. Rebuild, `cp` pro `.exe`,
  conferir `(Get-Item dist\Modern3DText.scr).VersionInfo`.

- [ ] **Step 6: rodar o checklist interativo** (headless marcado pelo
  executor; interativo fica pro Alan).

- [ ] **Step 7: Commit + tag**
```sh
git add docs/qa-checklist-phase-4d.md docs/img/phase4d-*.png README.md res/screensaver.rc
git commit -m "$(printf 'feat: phase 4d - chromatic aberration, vignette, FXAA (spec sec 8.1 complete)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
git tag -a v0.4.3-phase4d -m "Fase 4d: aberracao cromatica + vinheta + FXAA"
```

- [ ] **Step 8: finishing-a-development-branch** — `make test` no resultado
  do merge, apresentar as 3 opções, executar a escolha. Base = `main`.

---

## Self-Review

**1. Cobertura (spec §8.1 passos 7/8/10, §10.1 `chroma`/`vignette`/`fxaa`):**
- CA: "amostra compTex em R/G/B com deslocamento de UV = dir_do_centro *
  strength * r²" → `post_finish.frag` Task 2 Step 2, fórmula literal (com um
  fator de escala `0.02` pra mapear o slider 0..1 num deslocamento de tela
  pequeno — o spec não dá a constante, decisão de implementação). ✓
- Vinheta multiplicativa → mesmo shader, `smoothstep` radial. ✓
- Tonemap ACES + gamma → preservado do `post_composite.frag` antigo, agora em
  `post_finish.frag`. ✓
- FXAA com toggle → `post_fxaa.frag`, passe final condicional. Decisão da 4c:
  ligado por padrão. ✓
- Modo preview/reduzido sem os 3 → gate direto em `gl_window.c` (Task 3),
  **não** entra no ladder de auto-qualidade (decisão: são baratos, spec não
  lista no §11.4). ✓
- Controles no diálogo → Task 4, **aba nova** "Pós" (decisão do usuário —
  spec §10.3 lista só "Efeitos", mas a aba já estava cheia; precedente já
  existe: Geometria e Desempenho também não estavam na lista original do
  spec). ✓
- **Fora desta fase:** resolução independente do streak length (adiado,
  decisão do usuário 2026-09-11). Presets, i18n, backgrounds, partículas,
  malha = fases 5-8, inalteradas. ✓

**2. Placeholders:** sem "TODO"/"TBD". O Task 2 junta shader-rename + `post.c`
num único task/commit *de propósito*, porque separá-los deixaria o repo numa
build quebrada entre commits (dependência genuína, não um placeholder
disfarçado — explicado no Global Constraints e repetido no cabeçalho do
task). Todo o resto vem com código completo e passos numerados sem
condicionais escondidas.

**3. Consistência de tipos:**
- `Config` (+ 5 campos) — Task 1; copiados em `gl_window_set_config` (Task 3)
  pra `PostParams` (Task 2); escritos por `post_proc` (Task 4). Faixas dos
  controles (Task 4: sliders 0..100) batem com os clamps (Task 1: 0..1). ✓
- `PostParams` (+ 5 campos) — `post.h` (Task 2), montado em `render_into_post`
  (Task 3), consumido em `post_present` (Task 2). ✓
- `EMBED_post_combine_frag` / `_finish_frag` / `_fxaa_frag` — gerados pelo
  `embed` (Task 2), usados em `post_create` (Task 2, mesmo commit).
  `EMBED_post_composite_frag` não existe mais em lugar nenhum depois do Task 2. ✓
- `prog_combine`/`prog_finish`/`prog_fxaa`/`comp`/`ldr`/`out_w`/`out_h` — campos
  de `struct Post` (Task 2), criados em `ensure_output_size` (chamado do
  `post_present`), liberados em `post_destroy`. ✓
- IDs `IDC_CHROMA`..`IDC_FXAA` 1700+ — não colidem com Efeitos (1500-1514) nem
  Desempenho (1600-1605). `IDD_TAB_POST` 116, depois de `IDD_TAB_PERF` 115. ✓
- `M3DT_TAB` clamp 0..6 (Task 4) consistente com a 7ª aba. ✓

Sem inconsistências.

---

## Execution Handoff

**Plano completo e salvo em
`docs/superpowers/plans/2026-09-11-modern-3d-text-phase-4d-post-fx.md`. Duas
opções de execução:**

**1. Subagent-Driven** — um subagente novo por task, revisão entre tasks.

**2. Inline nesta sessão** — executo as tasks aqui via `executing-plans`, com
checkpoints.

**Qual abordagem?**
