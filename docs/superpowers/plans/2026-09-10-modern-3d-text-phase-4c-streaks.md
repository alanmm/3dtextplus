# Modern 3D Text — Fase 4c: streaks de difração — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Os realces claros da cena ganham **streaks** (raios) — o "starburst" de
difração de lente que o pedido original citou, mais um modo **anamórfico**
(faixa horizontal azulada, estilo cinema). Escolhível na aba **Efeitos**
(Starburst / Anamórfico / Desligado + intensidade + comprimento), desligado por
padrão. Entra na cadeia de pós logo depois do bloom, somado na composição antes
do tonemap. O ladder da auto-qualidade ganha "streaks off" como primeiro degrau.

**Architecture:** Reusa o **bright-pass** (`post_bright.frag`) num alvo dedicado
em 1/4 de resolução (`streak_src`). Um shader novo `post_streak.frag` faz um blur
direcional simétrico com passo exponencial ao longo de **um eixo**; `post.c`
chama-o 3× (ping-pong, passos crescentes) por eixo e acumula aditivamente em
`streaks_acc`. **Starburst** = 3 eixos (0°/60°/120° → estrela de 6 pontas);
**anamórfico** = 1 eixo horizontal, passo base maior, tinta azulada. A composição
(`post_composite.frag`) passa a somar `streaks * uStreaksIntensity` junto do
bloom. `PostParams` ganha `streaks_mode` / `streaks_intensity` / `streaks_length`.
`RenderQuality` ganha um gate `streaks` (0/1) e o ladder FULL vai de 7 → 8 passos.

**Tech Stack:** C11 · w64devkit · OpenGL 3.3 / GLSL 330 · FBOs `R11F_G11F_B10F` em
1/4 de resolução · reuso de `config`/`post`/`gl_core`/`gl_window`/`render_tiers`/
`config_dialog`/`embed`.

## Global Constraints

Do spec (§8.1 passo 5, §10.1 `effects.streaks`) e do estado pós-Fase 4b. Todo
task herda esta seção.

- **Linguagem:** C11. Flags `-std=c11 -municode -Wall -Wextra` (+ `-O2 -DNDEBUG`
  release). **Sem `-ffast-math`.** Build **sem warnings** — warning é bug, corrige
  na hora.
- **Toolchain:** só w64devkit em `C:\Users\alanm\w64devkit`. **Sem downloads
  novos.** Todo comando de shell começa com
  `export PATH="/c/Users/alanm/w64devkit/bin:$PATH"`.
- **API gráfica:** OpenGL 3.3 core. Os buffers de streak são
  `GL_R11F_G11F_B10F` (`gl_fbo_r11f`) em **1/4** da resolução interna do `post`
  (streaks são borrados; 1/4 é de sobra e barato).
- **Registro:** só `HKCU\Software\Modern3DText` (testes: `..._test`). Nada fora
  disso e de `%LOCALAPPDATA%\Modern3DText\`.
- **`.scr` ≤ 3 MB** (o build falha se passar). i18n só na Fase 8 — strings novas
  em português direto no `.rc` / código.
- **Modo preview / reduzido:** o `/p`, o mini-preview e o tier reduzido **não**
  têm streaks (é um efeito caro e forte; a janelinha e a GPU fraca ficam sem).
  Decidido pelo gate `RenderQuality.streaks` + o check `!g->preview`.
- **Fora desta fase (Fase 4d):** aberração cromática, vinheta, **FXAA**
  (com toggle, ligado por padrão) + a reestruturação da cadeia que a CA exige
  (composição vira alvo HDR intermediário, chroma+vinheta+tonemap num passe,
  FXAA num passe LDR final). Anotado.
- **Commits frequentes**, um por task. TDD onde é determinístico (`config`,
  `render_quality_for_step`). GL: PNG conferível (starburst vs anamórfico vs off;
  intensidade/comprimento).
- **Atribuição:** todo commit termina com
  `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>`.
- **`.exe` gotcha:** o build só atualiza `dist/Modern3DText.scr`. Antes de rodar
  o binário direto: `cp -f dist/Modern3DText.scr dist/Modern3DText.exe`. E o MSYS
  mastiga `/s` → passar `MSYS_NO_PATHCONV=1`.
- **Hooks de teste** reusados: `M3DT_SELFTEST=1`, `M3DT_SHOT=<path>` +
  `M3DT_SHOT_T=<s>`, `M3DT_LOG_APPEND=1`, `M3DT_TAB=<0..5>`, `M3DT_HOLD_MS=<ms>`,
  `M3DT_FORCE_TIER`, `M3DT_AQ_FORCE_MS` / `M3DT_AQ_FAST`. Nenhum novo nesta fase.
- **Captura de diálogo:** `scratchpad/shot_dialog.ps1` (da Fase 4a) — `-Tab N`.

---

## File Structure

| Arquivo | Responsabilidade |
|---|---|
| `src/config.h` / `.c` | (+) `int streaks_mode; float streaks_intensity; float streaks_length;` após `render_scale`/`auto_quality`. Defaults, clamps, chaves REG_SZ. **Sem** bump de `CFG_VERSION` (campos novos → default; v2 já cobre). |
| `shaders/post_streak.frag` | **novo.** blur direcional simétrico, passo exponencial, tinta. |
| `shaders/post_composite.frag` | (modif.) (+) `uniform sampler2D uStreaks; uniform float uStreaksIntensity; uniform int uHasStreaks;` — soma `streaks * uStreaksIntensity` antes do ACES. |
| `build/Makefile` | `shaders/post_streak.frag` em `EMBED_INPUTS`. |
| `src/post.h` / `.c` | `PostParams` (+) `int streaks_mode; float streaks_intensity, streaks_length;`. `Post` (+) `GlFbo streak_src, streak_a, streak_b, streaks_acc;` (1/4 res) e `unsigned prog_streak;`. `post_present`: depois do bloom, se `streaks_mode != 0 && streaks_intensity > eps` → bright-pass → cadeia de eixos → `streaks_acc`; a composição passa `uStreaks`/`uStreaksIntensity`/`uHasStreaks`. |
| `src/render_tiers.h` / `.c` | `RenderQuality` (+) `int streaks;`. `render_quality_for_step`: ladder FULL 7 → **8** passos (novo passo 1 = "streaks off"); REDUCED → `streaks = 0`. `render_quality_ladder_len(FULL)` = 8. |
| `src/gl_window.c` | `gl_window_frame` / `render_into_post`: copia `cfg->streaks_*` em `g->post_params` (via `gl_window_set_config`) e monta `pr.streaks_mode = (!g->preview && g->quality.streaks && g->post_params.streaks_mode) ? g->post_params.streaks_mode : 0;`. |
| `build/tests/test_config.c` | (+) round-trip + clamps de `streaks_mode` / `streaks_intensity` / `streaks_length`. |
| `build/tests/test_render_tiers.c` | (+) o ladder de 8 passos, `streaks` gate por passo. |
| `src/resource.h`, `res/screensaver.rc` | (+) `IDC_STREAKMODE`, `IDC_SINT`, `IDC_SINT_VAL`, `IDC_SLEN`, `IDC_SLEN_VAL` (1510+). `IDD_TAB_EFFECTS` reorganizado p/ caber a seção de streaks. |
| `src/config_dialog.c` | aba **Efeitos**: (+) combo Streaks + 2 sliders; `effects_enable` também liga/desliga os 2 sliders conforme o combo. |
| `docs/qa-checklist-phase-4c.md`, `README.md` | novo checklist; status → 4c. |

---

## Task 1: Config — campos de streaks (TDD)

**Files:** `src/config.h`, `src/config.c`, `build/tests/test_config.c`

**Interfaces — `Config` ganha (após `auto_quality`):**
```c
int   streaks_mode;       /* 0 desligado | 1 starburst | 2 anamorfico */
float streaks_intensity;  /* 0 .. 2.0 */
float streaks_length;     /* 0 .. 1  (escala o passo do blur) */
```
Padrões: `streaks_mode = 0`, `streaks_intensity = 0.5f`, `streaks_length = 0.5f`.
Clamps: `streaks_mode` fora de `0..2` → `0`; `clampf(intensity, 0, 2)`;
`clampf(length, 0, 1)`. **Sem** bump de `CFG_VERSION` (2 já basta; campo ausente
→ default).

- [ ] **Step 1: `build/tests/test_config.c`** — no bloco round-trip existente,
  antes do `config_save_to(&a, TESTKEY)`:
  `a.streaks_mode = 2; a.streaks_intensity = 1.4f; a.streaks_length = 0.8f;`
  e depois do `config_load_from(&b, TESTKEY)`:
```c
    EXPECT(b.streaks_mode == 2);
    EXPECT(nearf(b.streaks_intensity, 1.4f));
    EXPECT(nearf(b.streaks_length, 0.8f));
```
  No bloco de clamp de perf (o que grava `fps_cap`=999 etc.), adicionar:
```c
        { L"streaks_mode", L"9" }, { L"streaks_intensity", L"-1" },
        { L"streaks_length", L"5" },
```
  na tabela `kv[]` (aumentar o tamanho de 5 para 8 e o loop `for (int i = 0; i < 8; ++i)`)
  e após o `config_load_from(&e, TESTKEY)`:
```c
    EXPECT(e.streaks_mode == 0);                /* 9 -> fora de 0..2 -> 0 */
    EXPECT(e.streaks_intensity >= 0.0f);        /* -1 -> clamp */
    EXPECT(e.streaks_length <= 1.0f);           /* 5 -> clamp */
```

- [ ] **Step 2: rodar — falha** (`'Config' has no member named 'streaks_mode'`).
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```

- [ ] **Step 3: `src/config.h`** — os 3 campos após `auto_quality`.
  `src/config.c`:
  - `config_defaults`: `c->streaks_mode = 0; c->streaks_intensity = 0.5f;
    c->streaks_length = 0.5f;`
  - `config_load_from`, junto das outras leituras de efeito:
```c
    reg_get_i(k, L"streaks_mode", &c->streaks_mode);
    if (reg_get_f(k, L"streaks_intensity", &f)) c->streaks_intensity = f;
    if (reg_get_f(k, L"streaks_length", &f))    c->streaks_length = f;
```
  - saneamento:
```c
    if (c->streaks_mode < 0 || c->streaks_mode > 2) c->streaks_mode = 0;
    c->streaks_intensity = clampf(c->streaks_intensity, 0.0f, 2.0f);
    c->streaks_length = clampf(c->streaks_length, 0.0f, 1.0f);
```
  - `config_save_to`, junto dos outros efeitos:
```c
    set_f(k, L"streaks_mode", (float)c->streaks_mode);
    set_f(k, L"streaks_intensity", c->streaks_intensity);
    set_f(k, L"streaks_length", c->streaks_length);
```

- [ ] **Step 4: rodar — passa.**
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```

- [ ] **Step 5: Commit**
```sh
git add src/config.h src/config.c build/tests/test_config.c
git commit -m "$(printf 'feat: config streaks fields (mode/intensity/length)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 2: Shader do streak + composição + embed

**Files:** `shaders/post_streak.frag` (novo), `shaders/post_composite.frag`,
`build/Makefile`

- [ ] **Step 1: `shaders/post_streak.frag`**
```glsl
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2  uTexel;    // 1/tamanho do buffer de streak
uniform vec2  uDir;      // eixo unitario (ex.: (1,0), (0.5,0.866))
uniform float uStep;     // passo desta iteracao (1, 4, 16)
uniform float uLength;   // 0..1 do usuario -> multiplica o espacamento
uniform vec3  uTint;     // branco (starburst) | azulado (anamorfico)
out vec3 o;

const int TAPS = 4;

void main()
{
    vec3  c = vec3(0.0);
    float wsum = 0.0;
    float spacing = uStep * (1.0 + uLength * 3.0);
    for (int i = 0; i < TAPS; ++i) {
        float fi = float(i);
        float w  = pow(0.82, uStep * fi);          // atenuacao exponencial
        vec2  off = uDir * uTexel * spacing * fi;
        c += texture(uTex, vUV + off).rgb * w;
        c += texture(uTex, vUV - off).rgb * w;     // simetrico -> raio nos 2 sentidos
        wsum += 2.0 * w;
    }
    o = (c / max(wsum, 1e-4)) * uTint;
}
```

- [ ] **Step 2: `shaders/post_composite.frag`** — somar streaks antes do ACES:
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

vec3 aces(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
    vec3 c = texture(uScene, vUV).rgb;
    if (uHasBloom == 1)   c += texture(uBloom, vUV).rgb   * uBloomIntensity;
    if (uHasStreaks == 1) c += texture(uStreaks, vUV).rgb * uStreaksIntensity;
    c = aces(c);
    c = pow(c, vec3(1.0 / 2.2));
    o = vec4(c, 1.0);
}
```

- [ ] **Step 3: `build/Makefile`** — `EMBED_INPUTS` += `shaders/post_streak.frag`:
```make
EMBED_INPUTS := shaders/model.vert shaders/model.frag shaders/fullscreen.vert \
                shaders/post_bright.frag shaders/post_down.frag shaders/post_up.frag \
                shaders/post_composite.frag shaders/post_streak.frag
```

- [ ] **Step 4: build — o `embed` regenera `generated/embedded.h` com
  `EMBED_post_streak_frag`.** Confirmar que compila (o `post.c` ainda não usa,
  então só o header muda):
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile
grep -c "EMBED_post_streak_frag" generated/embedded.h   # espera >= 1
```

- [ ] **Step 5: Commit**
```sh
git add shaders/post_streak.frag shaders/post_composite.frag build/Makefile
git commit -m "$(printf 'feat: post_streak shader (directional exponential blur) + composite streak input\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 3: `post.c` — cadeia de streaks + `PostParams`

**Files:** `src/post.h`, `src/post.c`

**Interfaces:** `PostParams` (+)
```c
int   streaks_mode;       /* 0 off | 1 starburst | 2 anamorfico */
float streaks_intensity;  /* 0 .. 2 */
float streaks_length;     /* 0 .. 1 */
```

- [ ] **Step 1: `post.h`** — 3 campos em `PostParams` (após `radius`).

- [ ] **Step 2: `post.c` — `struct Post`** ganha:
```c
    GlFbo streak_src;              /* bright-pass em 1/4 res */
    GlFbo streak_a, streak_b;      /* ping-pong do blur, 1/4 res */
    GlFbo streaks_acc;             /* acumulador dos eixos, 1/4 res */
    unsigned prog_streak;
```

- [ ] **Step 3: `post_create`** — compila `prog_streak`:
```c
    p->prog_streak = prog(EMBED_post_streak_frag);
    if (!p->prog_bright || !p->prog_down || !p->prog_up || !p->prog_comp || !p->prog_streak) {
        post_destroy(p);
        return NULL;
    }
    ...
    glUseProgram(p->prog_streak); glUniform1i(glGetUniformLocation(p->prog_streak, "uTex"), 0);
    glUseProgram(p->prog_comp);
    glUniform1i(glGetUniformLocation(p->prog_comp, "uScene"), 0);
    glUniform1i(glGetUniformLocation(p->prog_comp, "uBloom"), 1);
    glUniform1i(glGetUniformLocation(p->prog_comp, "uStreaks"), 2);
```

- [ ] **Step 4: `ensure_size`** — criar/liberar os 4 FBOs de streak em 1/4 de
  `w`/`h` (mín. 1):
```c
    gl_fbo_free(&p->streak_src); gl_fbo_free(&p->streak_a);
    gl_fbo_free(&p->streak_b);   gl_fbo_free(&p->streaks_acc);
    int sw = w / 4 > 1 ? w / 4 : 1;
    int sh = h / 4 > 1 ? h / 4 : 1;
    p->streak_src  = gl_fbo_r11f(sw, sh);
    p->streak_a    = gl_fbo_r11f(sw, sh);
    p->streak_b    = gl_fbo_r11f(sw, sh);
    p->streaks_acc = gl_fbo_r11f(sw, sh);
```
  (junto do bloco que já libera/cria `hdr`/`bloom[]`.)

- [ ] **Step 5: `post_present`** — entre o bloco de bloom e a composição.
  O **último** blur de cada eixo escreve direto em `streaks_acc` com blend
  aditivo (`GL_ONE, GL_ONE`); as iterações 0 e 1 vão pro ping-pong com blend
  desligado. Não precisa de shader de cópia.
```c
    int has_streaks = (pr.streaks_mode != 0 && pr.streaks_intensity > 1e-4f) ? 1 : 0;
    if (has_streaks) {
        /* bright-pass da cena -> streak_src (1/4 res) */
        gl_fbo_bind(&p->streak_src);
        glUseProgram(p->prog_bright);
        glUniform1f(glGetUniformLocation(p->prog_bright, "uThreshold"), pr.threshold);
        glUniform1f(glGetUniformLocation(p->prog_bright, "uKnee"), 0.5f * pr.threshold);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, p->hdr.color);
        gl_fullscreen_draw();

        /* limpa o acumulador */
        gl_fbo_bind(&p->streaks_acc);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        /* eixos: starburst = 3 (0, 60, 120 graus -> estrela de 6 pontas);
           anamorfico = 1 (horizontal) */
        static const float AX3[3][2] = {
            { 1.0f, 0.0f }, { 0.5f, 0.8660254f }, { -0.5f, 0.8660254f }
        };
        static const float TINT_WHITE[3] = { 1.0f, 1.0f, 1.0f };
        static const float TINT_ANAMO[3] = { 0.55f, 0.72f, 1.0f };
        int          naxes     = (pr.streaks_mode == 1) ? 3 : 1;
        float        base_step = (pr.streaks_mode == 2) ? 2.5f : 1.0f;
        float        len       = (pr.streaks_mode == 2) ? pr.streaks_length * 1.6f
                                                        : pr.streaks_length;
        const float *tint      = (pr.streaks_mode == 2) ? TINT_ANAMO : TINT_WHITE;

        glUseProgram(p->prog_streak);
        set_texel(p->prog_streak, &p->streak_src);   /* src/a/b/acc tem o mesmo tamanho */
        glUniform1f(glGetUniformLocation(p->prog_streak, "uLength"), len);
        glUniform3fv(glGetUniformLocation(p->prog_streak, "uTint"), 1, tint);

        for (int a = 0; a < naxes; ++a) {
            float dx = (pr.streaks_mode == 2) ? 1.0f : AX3[a][0];
            float dy = (pr.streaks_mode == 2) ? 0.0f : AX3[a][1];
            glUniform2f(glGetUniformLocation(p->prog_streak, "uDir"), dx, dy);

            const GlFbo *in = &p->streak_src;
            const GlFbo *ping[2] = { &p->streak_a, &p->streak_b };
            float steps[3] = { base_step, base_step * 4.0f, base_step * 16.0f };
            for (int it = 0; it < 3; ++it) {
                int last = (it == 2);
                if (last) {
                    gl_fbo_bind(&p->streaks_acc);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_ONE, GL_ONE);
                } else {
                    gl_fbo_bind(ping[it & 1]);
                    glDisable(GL_BLEND);
                }
                glUniform1f(glGetUniformLocation(p->prog_streak, "uStep"), steps[it]);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, in->color);
                gl_fullscreen_draw();
                in = last ? &p->streaks_acc : ping[it & 1];
            }
        }
        glDisable(GL_BLEND);
    }
```
  > Nota: no modo anamórfico o `uTint` azulado é aplicado em toda iteração — o
  > azul fica saturado, que é o look pretendido. Se nos prints ficar forte
  > demais, passar `TINT_WHITE` nas iterações 0/1 e o azul só na última.

- [ ] **Step 6: `post_present` — composição** passa os uniforms de streak:
```c
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
```

- [ ] **Step 7: `post_destroy`** — `gl_fbo_free` dos 4 buffers novos;
  `glDeleteProgram(p->prog_streak)`.

- [ ] **Step 8: build.**
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile 2>&1 | grep -Ei "warning:|error:|Built"
```
  (Ainda sem efeito visível — `PostParams.streaks_mode` chega sempre 0 até o
  Task 4.)

- [ ] **Step 9: Commit**
```sh
git add src/post.h src/post.c
git commit -m "$(printf 'feat: post - diffraction streak chain (bright -> per-axis exponential blur -> accumulate)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 4: Ligar no `gl_window` + degrau "streaks off" no ladder

**Files:** `src/render_tiers.h` / `.c`, `src/gl_window.c`,
`build/tests/test_render_tiers.c`

**Interfaces:** `RenderQuality` (+) `int streaks;` (após `bloom`).

**Ladder FULL novo (8 passos):**
| step | mudança acumulada |
|---|---|
| 0 | `streaks=1`, `bloom=1`, `msaa=cfg.msaa`, `scale=cfg.render_scale` |
| 1 | `streaks=0` |
| 2 | `bloom=0` |
| 3 | `msaa=min(cfg.msaa,4)` |
| 4 | `msaa=min(cfg.msaa,2)` |
| 5 | `msaa=0` |
| 6 | `scale=min(cfg.render_scale,0.75)` |
| 7 | `scale=0.5` |

REDUCED: 1 passo, `{ streaks=0, bloom=0, msaa=min(cfg.msaa,2), scale=min(cfg.render_scale,0.75) }`.

- [ ] **Step 1: `test_render_tiers.c`** — atualizar as expectativas do ladder:
```c
    RenderQuality q0 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 0);
    EXPECT(q0.streaks == 1 && q0.bloom == 1 && q0.msaa == 4 && q0.render_scale > 0.99f);
    RenderQuality q1 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 1);
    EXPECT(q1.streaks == 0 && q1.bloom == 1);
    RenderQuality q2 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 2);
    EXPECT(q2.streaks == 0 && q2.bloom == 0 && q2.msaa == 4);
    RenderQuality q5 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 5);
    EXPECT(q5.msaa == 0);
    RenderQuality q7 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 7);
    EXPECT(q7.render_scale <= 0.5f + 1e-4f);
    RenderQuality qhi = render_quality_for_step(&cfg, M3DT_TIER_FULL, 99);
    EXPECT(qhi.step == 7);
    EXPECT(render_quality_ladder_len(M3DT_TIER_FULL) == 8);

    RenderQuality qr = render_quality_for_step(&cfg, M3DT_TIER_REDUCED, 0);
    EXPECT(qr.streaks == 0 && qr.bloom == 0 && qr.msaa <= 2 && qr.render_scale <= 0.75f + 1e-4f);
```
  Também no `aq_decide` teste, `ladder_len` passa de 7 → **8** onde aparece.
  Rodar → falha.

- [ ] **Step 2: `render_tiers.h`** — `int streaks;` em `RenderQuality` (antes de
  `bloom`). `render_tiers.c`:
```c
int render_quality_ladder_len(M3dtTier tier)
{
    return tier == M3DT_TIER_REDUCED ? 1 : 8;
}

RenderQuality render_quality_for_step(const Config *cfg, M3dtTier tier, int step)
{
    int len = render_quality_ladder_len(tier);
    if (step < 0) step = 0;
    if (step > len - 1) step = len - 1;

    RenderQuality q;
    q.step = step;

    if (tier == M3DT_TIER_REDUCED) {
        q.streaks = 0;
        q.bloom = 0;
        q.msaa = mini(cfg->msaa, 2);
        q.render_scale = minf(cfg->render_scale, 0.75f);
        return q;
    }

    q.streaks = (step >= 1) ? 0 : 1;
    q.bloom   = (step >= 2) ? 0 : 1;
    q.msaa = cfg->msaa;
    if (step >= 3) q.msaa = mini(q.msaa, 4);
    if (step >= 4) q.msaa = mini(q.msaa, 2);
    if (step >= 5) q.msaa = 0;
    q.render_scale = cfg->render_scale;
    if (step >= 6) q.render_scale = minf(q.render_scale, 0.75f);
    if (step >= 7) q.render_scale = 0.5f;
    return q;
}
```
  Rodar → passa.

- [ ] **Step 3: `gl_window.c`.**
  - `gl_window_set_config`: junto da cópia de `bloom_*` p/ `g->post_params`:
```c
    g->post_params.streaks_mode      = cfg->streaks_mode;
    g->post_params.streaks_intensity = cfg->streaks_intensity;
    g->post_params.streaks_length    = cfg->streaks_length;
```
  - `render_into_post`: depois de `pr.bloom = frame_bloom(g);`:
```c
    pr.streaks_mode = (!g->preview && g->quality.streaks && g->post_params.streaks_mode)
                      ? g->post_params.streaks_mode : 0;
```
  - (o `gl_window_create` com `cfg == NULL` já cai no `else` de defaults da 4a —
    adicionar `g->post_params.streaks_mode = 0;` lá por clareza.)

- [ ] **Step 4: build + captura comparativa.**
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile && cp -f dist/Modern3DText.scr dist/Modern3DText.exe
S="C:/Users/alanm/AppData/Local/Temp/claude/<sessao>/scratchpad"
BASE="material_mode 1;metalness 1;roughness 0.14;base_color #C8CED8;bloom_on 1;bloom_intensity 0.7;env_path $S/testenv.png;max_angle_y 28"
setcfg() { powershell -Command "New-Item 'HKCU:\Software\Modern3DText' -Force|Out-Null; '$1'.Split(';')|%{ \$p=\$_.Split(' ',2); Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name \$p[0] -Value \$p[1] -Type String }"; }
setcfg "$BASE;streaks_mode 0"
MSYS_NO_PATHCONV=1 M3DT_LOG_APPEND=1 M3DT_SELFTEST=1 M3DT_SHOT="$S/s_off.png" M3DT_SHOT_T=2.4 ./dist/Modern3DText.exe /s
setcfg "$BASE;streaks_mode 1;streaks_intensity 0.8;streaks_length 0.5"
MSYS_NO_PATHCONV=1 M3DT_LOG_APPEND=1 M3DT_SELFTEST=1 M3DT_SHOT="$S/s_starburst.png" M3DT_SHOT_T=2.4 ./dist/Modern3DText.exe /s
setcfg "$BASE;streaks_mode 2;streaks_intensity 0.8;streaks_length 0.6"
MSYS_NO_PATHCONV=1 M3DT_LOG_APPEND=1 M3DT_SELFTEST=1 M3DT_SHOT="$S/s_anamorphic.png" M3DT_SHOT_T=2.4 ./dist/Modern3DText.exe /s
powershell -Command "Remove-Item 'HKCU:\Software\Modern3DText' -Recurse -Force"
```
  **Conferir os 3 PNGs:**
  - `s_off`: só bloom (baseline da 4a).
  - `s_starburst`: estrela de 6 pontas saindo dos realces; raios nas 3 direções
    (0°/60°/120°); não estoura (ACES segura); some suave.
  - `s_anamorphic`: faixa horizontal longa azulada nos realces; sem raios
    verticais.
  Sem `glError` / `FBO ... incompleto`. Testar também `M3DT_FORCE_TIER=reduced`
  → **sem streaks** no PNG (mesmo com `streaks_mode 1`).

- [ ] **Step 5: regressão + estabilidade.**
```sh
mingw32-make -f build/Makefile test          # all tests passed
for i in $(seq 1 10); do MSYS_NO_PATHCONV=1 M3DT_SELFTEST=1 ./dist/Modern3DText.exe /s >/dev/null 2>&1 || echo FAIL $i; done
for i in $(seq 1 10); do MSYS_NO_PATHCONV=1 M3DT_SELFTEST=1 ./dist/Modern3DText.exe /c >/dev/null 2>&1 || echo FAIL $i; done
# auto-qualidade: com streaks on, o 1o degrau deve ser "streaks off"
powershell -Command "New-Item 'HKCU:\Software\Modern3DText' -Force|Out-Null; Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name streaks_mode -Value '1' -Type String; Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name auto_quality -Value '1' -Type String"
MSYS_NO_PATHCONV=1 M3DT_AQ_FORCE_MS=30 M3DT_AQ_FAST=1 M3DT_LOG_APPEND=1 M3DT_SELFTEST=1 ./dist/Modern3DText.exe /s
grep "autoQuality: step" "$LOCALAPPDATA/Modern3DText/log.txt"   # step 1 = streaks off, step 2 = bloom off, ...
powershell -Command "Remove-Item 'HKCU:\Software\Modern3DText' -Recurse -Force"
```

- [ ] **Step 6: Commit**
```sh
git add src/render_tiers.h src/render_tiers.c src/gl_window.c build/tests/test_render_tiers.c
git commit -m "$(printf 'feat: wire streaks into gl_window; autoQuality ladder gains a streaks-off step\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 5: aba Efeitos — controles de streak

**Files:** `src/resource.h`, `res/screensaver.rc`, `src/config_dialog.c`

**IDs novos (`resource.h`, seção aba Efeitos):**
```c
#define IDC_STREAKMODE   1510
#define IDC_SINT         1511
#define IDC_SINT_VAL     1512
#define IDC_SLEN         1513
#define IDC_SLEN_VAL     1514
```

- [ ] **Step 1: `res/screensaver.rc` — `IDD_TAB_EFFECTS` reorganizado**
  (compactar a seção de bloom, tirar o parágrafo de ajuda longo, adicionar a
  seção de streaks; template continua `240, 200`):
```
IDD_TAB_EFFECTS DIALOGEX 0, 0, 240, 200
STYLE DS_SETFONT | DS_CONTROL | WS_CHILD
FONT 9, "Segoe UI", 400, 0, 0x1
BEGIN
    AUTOCHECKBOX "Bloom (glare)", IDC_BLOOM, 8, 8, 120, 12
    LTEXT      "Limiar:", -1, 8, 24, 80, 9
    LTEXT      "", IDC_BTHRESH_VAL, 182, 24, 50, 9
    CONTROL    "", IDC_BTHRESH, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 33, 224, 14
    LTEXT      "Intensidade:", -1, 8, 49, 80, 9
    LTEXT      "", IDC_BINT_VAL, 182, 49, 50, 9
    CONTROL    "", IDC_BINT, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 58, 224, 14
    LTEXT      "Espalhamento:", -1, 8, 74, 80, 9
    LTEXT      "", IDC_BRAD_VAL, 182, 74, 50, 9
    CONTROL    "", IDC_BRAD, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 83, 224, 14

    LTEXT      "Streaks (difracao):", -1, 8, 106, 90, 9
    COMBOBOX   IDC_STREAKMODE, 100, 104, 100, 80, CBS_DROPDOWNLIST | WS_TABSTOP
    LTEXT      "Intensidade:", -1, 8, 124, 80, 9
    LTEXT      "", IDC_SINT_VAL, 182, 124, 50, 9
    CONTROL    "", IDC_SINT, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 133, 224, 14
    LTEXT      "Comprimento:", -1, 8, 149, 80, 9
    LTEXT      "", IDC_SLEN_VAL, 182, 149, 50, 9
    CONTROL    "", IDC_SLEN, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 158, 224, 14
    LTEXT      "Bloom e streaks agem no render HDR, antes do tonemap.",
               -1, 8, 178, 224, 9
END
```

- [ ] **Step 2: `src/config_dialog.c` — aba Efeitos.**
  - `effects_labels(HWND h)` (+):
```c
    swprintf(b, 32, L"%.2f", (double)g_work.streaks_intensity); SetDlgItemTextW(h, IDC_SINT_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.streaks_length);    SetDlgItemTextW(h, IDC_SLEN_VAL, b);
```
  - `effects_enable(HWND h)` (+):
```c
    BOOL st = g_work.streaks_mode != 0;
    EnableWindow(GetDlgItem(h, IDC_SINT), st);
    EnableWindow(GetDlgItem(h, IDC_SLEN), st);
```
  - `WM_INITDIALOG` (+):
```c
    static const wchar_t *sm[] = { L"Desligado", L"Starburst", L"Anamorfico" };
    for (int i = 0; i < 3; ++i)
        SendDlgItemMessageW(h, IDC_STREAKMODE, CB_ADDSTRING, 0, (LPARAM)sm[i]);
    SendDlgItemMessageW(h, IDC_STREAKMODE, CB_SETCURSEL, g_work.streaks_mode, 0);
    set_slider(h, IDC_SINT, 0, 200, (int)(g_work.streaks_intensity * 100.0f + 0.5f));
    set_slider(h, IDC_SLEN, 0, 100, (int)(g_work.streaks_length * 100.0f + 0.5f));
```
  - `WM_HSCROLL` (+):
```c
    g_work.streaks_intensity = (float)SendDlgItemMessageW(h, IDC_SINT, TBM_GETPOS, 0, 0) / 100.0f;
    g_work.streaks_length    = (float)SendDlgItemMessageW(h, IDC_SLEN, TBM_GETPOS, 0, 0) / 100.0f;
```
  - `WM_COMMAND` — trocar o `if (LOWORD(w) == IDC_BLOOM)` por um `switch`:
```c
        case WM_COMMAND:
            if (LOWORD(w) == IDC_BLOOM) {
                g_work.bloom_on = (IsDlgButtonChecked(h, IDC_BLOOM) == BST_CHECKED);
                effects_enable(h);
                preview_dirty(h);
            } else if (LOWORD(w) == IDC_STREAKMODE && HIWORD(w) == CBN_SELCHANGE) {
                g_work.streaks_mode =
                    (int)SendDlgItemMessageW(h, IDC_STREAKMODE, CB_GETCURSEL, 0, 0);
                effects_enable(h);
                preview_dirty(h);
            }
            return TRUE;
```

- [ ] **Step 3: build + selftest + captura.**
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile && cp -f dist/Modern3DText.scr dist/Modern3DText.exe
for t in 0 1 2 3 4 5; do MSYS_NO_PATHCONV=1 M3DT_SELFTEST=1 M3DT_TAB=$t ./dist/Modern3DText.exe /c >/dev/null 2>&1 || echo "FAIL tab $t"; done
# aba Efeitos (4) com streaks
powershell -Command "New-Item 'HKCU:\Software\Modern3DText' -Force|Out-Null; Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name streaks_mode -Value '1' -Type String"
powershell -ExecutionPolicy Bypass -File "$S/shot_dialog.ps1" -Tab 4 -Out "$S/p4c_effects.png"
powershell -Command "Remove-Item 'HKCU:\Software\Modern3DText' -Recurse -Force"
```
  Conferir `p4c_effects.png`: bloom + streaks numa aba só, combo com "Starburst"
  selecionado, sliders de intensidade/comprimento **habilitados** (ficam
  desabilitados quando o combo está "Desligado"). Nada cortado.

- [ ] **Step 4: `make test`** → `all tests passed`.

- [ ] **Step 5: Commit**
```sh
git add src/resource.h res/screensaver.rc src/config_dialog.c
git commit -m "$(printf 'feat: config dialog Effects tab - streak mode combo + intensity/length sliders\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 6: integração, QA da Fase 4c, tag

**Files:** `docs/qa-checklist-phase-4c.md` (novo), `README.md`,
`res/screensaver.rc` (versão → 0.4.2), `docs/img/phase4c-*.png`

- [ ] **Step 1:** `make` (release) + `make debug` (clean) + `make test` verdes,
  **sem warnings**; `.scr` < 3 MB (esperado ~408 KB).

- [ ] **Step 2: capturas `docs/img/`** (metálico + env de teste):
  - `phase4c-streaks-off.png` / `phase4c-starburst.png` / `phase4c-anamorphic.png`
    — mesma cena, os 3 modos.
  - `phase4c-effects-tab.png` — a aba Efeitos com a seção de streaks.
  Conferir os 4.

- [ ] **Step 3: `docs/qa-checklist-phase-4c.md`** — no formato dos anteriores:
  - **Headless (dev):** `make`/`debug`/`test` verdes sem warning; `.scr` ~408 KB.
    `test_config` (streaks round-trip + clamps). `test_render_tiers` (ladder de 8
    passos; `streaks` gate; passo 1 = streaks off). Capturas: starburst = estrela
    de 6 pontas, anamórfico = faixa horizontal azulada, off = só bloom; ACES
    segura o brilho. `M3DT_FORCE_TIER=reduced` → sem streaks. `M3DT_AQ_FORCE_MS=30
    M3DT_AQ_FAST=1` com `streaks_mode=1` → log: `autoQuality: step 1` já com
    `streaks` desligado. Sem `glError`/FBO incompleto. 10× `/s`, 10× `/c`.
  - **Interativo (olho humano):** `/c` aba Efeitos — trocar Starburst/Anamórfico/
    Desligado → mini-preview muda na hora; sliders de intensidade/comprimento
    respondem; combo "Desligado" desabilita os 2 sliders. `OK` grava (`regedit`,
    REG_SZ). `/p` sem streaks. Multi-monitor. 100/150/200% DPI: aba Efeitos (a
    mais cheia agora) sem corte. 5 min `/s` com streaks — sem vazar VRAM (4 FBOs
    de 1/4 res por janela).
  - **Notas:** anamórfico usa tinta azulada saturada (aplicada em toda iteração)
    — é o look pretendido. Streaks vêm de um bright-pass em 1/4 de resolução
    (borrados, então 1/4 basta). O nº de pontas do starburst é fixo em 6 (3
    eixos) — o controle de pontas do spec §10.1 não foi exposto (decisão do
    usuário). CA / vinheta / FXAA = Fase 4d.

- [ ] **Step 4: `README.md`** — bloco **Status** → Fase 4c (streaks starburst +
  anamórfico na aba Efeitos; degrau "streaks off" no ladder). Manter a imagem do
  topo ou trocar por `docs/img/phase4c-starburst.png`.

- [ ] **Step 5: versão** — `res/screensaver.rc`: `FILEVERSION 0,4,2,0`,
  `PRODUCTVERSION 0,4,2,0`, strings `"0.4.2.0"`. Rebuild, `cp` pro `.exe`,
  conferir `(Get-Item dist\Modern3DText.scr).VersionInfo`.

- [ ] **Step 6: rodar o checklist interativo** (o executor faz as partes
  headless e marca; as interativas ficam pro Alan).

- [ ] **Step 7: Commit + tag**
```sh
git add docs/qa-checklist-phase-4c.md docs/img/phase4c-*.png README.md res/screensaver.rc
git commit -m "$(printf 'feat: phase 4c - diffraction streaks (starburst + anamorphic)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
git tag -a v0.4.2-phase4c -m "Fase 4c: streaks de difracao (starburst + anamorfico)"
```

- [ ] **Step 8: finishing-a-development-branch** — `make test` no resultado do
  merge, apresentar as 3 opções (merge local / PR / manter), executar a escolha.
  Base = `main`. (Alan normalmente pede merge local + `git push`.)

---

## Self-Review

**1. Cobertura (spec §8.1 passo 5, §10.1 `effects.streaks`):**
- Streaks a partir do bright buffer, N direções a 0/45/90/... → **starburst** com
  3 eixos (0°/60°/120° → 6 pontas) no Task 3. O spec cita 4/6/8 pontas
  configuráveis; o usuário optou por **fixo** (modo + intensidade + comprimento
  só) — documentado. ✓
- 2–3 iterações de blur de passo crescente por direção → 3 iterações (passos 1,
  4, 16) no Task 3 Step 5. ✓
- Composição `+ streaks * effects.streaks.intensity` → `post_composite.frag`
  (Task 2) + Task 3 Step 6. ✓
- Modo **anamórfico** (não está no spec; adição pedida pelo usuário) → 1 eixo
  horizontal + tinta azulada + passo base maior (Task 3 Step 5). ✓
- Controles no diálogo (§10.3) → Task 5. ✓
- Modo preview / reduzido sem streaks (§8.1) → gate `RenderQuality.streaks` +
  `!g->preview` (Task 4). ✓
- Auto-qualidade: "streaks off" é o 1º degrau do ladder (§11.4) → Task 4 (ladder
  7 → 8). ✓
- **Fora da 4c:** CA, vinheta, FXAA + reestruturação da cadeia → Fase 4d. Anotado
  na Global Constraints. ✓

**2. Placeholders:** sem "TODO"/"TBD". Shaders e código C vêm completos e
inline; o loop de acumulação de eixos (Task 3 Step 5) tem uma decisão única
(último blur escreve direto no `streaks_acc` aditivo). Verificações produzem PNG
comparável (off / starburst / anamórfico) + asserts de `config`/`render_tiers`.

**3. Consistência de tipos:**
- `Config` (+`streaks_mode` int, `streaks_intensity`/`streaks_length` float) —
  Task 1; lido por `gl_window_set_config` → `PostParams` (Task 4), escrito por
  `effects_proc` (Task 5). Faixas dos controles (Task 5: combo 0..2, sliders
  0..200 / 0..100) batem com os clamps (Task 1). ✓
- `PostParams` (+ os 3 campos) — `post.h` (Task 3), montado em `render_into_post`
  (Task 4), consumido em `post_present` (Task 3). ✓
- `RenderQuality` (+`streaks` int) — `render_tiers.h` (Task 4), `render_quality_
  for_step` preenche, `gl_window_frame` lê `g->quality.streaks`. `ladder_len`
  FULL 7 → 8 em `render_quality_ladder_len` + `aq_decide` (via `AqInput.ladder_
  len`, que já vem de `render_quality_ladder_len`). ✓
- `EMBED_post_streak_frag` — gerado pelo `embed` (Task 2), usado em `post_create`
  (Task 3). Nome = basename com não-alfanum → `_`. ✓
- `prog_streak` / `streak_src` / `streak_a` / `streak_b` / `streaks_acc` — campos
  de `struct Post` (Task 3), criados em `ensure_size`, liberados em
  `post_destroy`. ✓
- IDs `IDC_STREAKMODE`..`IDC_SLEN_VAL` 1510+ — não colidem com bloom (1500–1506)
  nem perf (1600+). `IDD_TAB_EFFECTS` continua 114. ✓

Sem inconsistências.

---

## Execution Handoff

**Plano completo e salvo em
`docs/superpowers/plans/2026-09-10-modern-3d-text-phase-4c-streaks.md`. Duas
opções de execução:**

**1. Subagent-Driven (recomendado)** — um subagente novo por task, revisão entre
tasks.

**2. Inline Execution** — executa as tasks nesta sessão via `executing-plans`,
com checkpoints.

**Qual abordagem?**
