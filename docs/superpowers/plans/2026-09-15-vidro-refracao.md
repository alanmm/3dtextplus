# Vidro — Refração Real (Distorção) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fazer o material Vidro mostrar o fundo real (distorcido pela normal da superfície) em vez de uma cor fixa, controlado por um novo slider "Distorção" (0 = desligado, sem custo).

**Architecture:** `scene.c` captura (via `glBlitFramebuffer`, lendo o que já está no framebuffer atualmente ligado) o fundo desenhado, logo antes de desenhar o texto, só quando Vidro está ativo e a Distorção > 0, numa textura HDR própria (`GlFbo grab`, campo de instância, nunca `static`). O fragment shader do Vidro desloca a amostragem dessa textura com base na normal da superfície (mais desvio nas bordas, menos de frente) e usa mipmaps + LOD ligado à Rugosidade pra borrar em vidro áspero.

**Tech Stack:** C11 (mingw-w64/GCC), OpenGL 3.3 core (glad), GLSL 330, Win32 (registro, dialog resources).

## Global Constraints

- Toolchain: `C:\Users\alanm\w64devkit`; `mingw32-make -f build/Makefile {debug,release,test,clean}`.
- **Rodar `mingw32-make -f build/Makefile clean` depois de editar qualquer `.h`** (Makefile não rastreia dependência de headers).
- Depois de um build `release` real, sempre `cp -f dist/Modern3DText.scr dist/Modern3DText.exe`.
- Textura de captura (`GlFbo grab`) é **campo de `SceneRenderer`, nunca `static`** — lição do bug de VAO compartilhado entre monitores (cada janela/monitor tem seu próprio contexto GL).
- `refraction == 0.0` deve pular a captura inteira (sem blit, sem bind) — não só "sem efeito visual".
- Todo campo novo de `Config` precisa dos 4 pontos de toque de sempre: `config_defaults`, `config_load_from`, sanitize (`clampf`), `config_save_to` — e também `preset_scope_copy`/`preset_dump_fields` em `presets.c`.
- Especificação completa: `docs/superpowers/specs/2026-09-15-vidro-refracao-design.md`.

---

### Task 1: Campo `Config.refraction` (TDD)

**Files:**
- Modify: `src/config.h:22-24`
- Modify: `src/config.c` (defaults ~linha 31, load ~linha 188, sanitize ~linha 285, save ~linha 393)
- Test: `build/tests/test_config.c` (defaults ~linha 32, roundtrip setup ~linha 105, roundtrip assert ~linha 173)

**Interfaces:**
- Produces: `Config.refraction` (float, 0..1, default `0.4f`) — usado pelas Tasks 2, 3, 4, 6.

- [ ] **Step 1: Escrever os testes que falham**

Em `build/tests/test_config.c`, localizar a linha:

```c
    EXPECT(nearf(d.emissive_amount, 0.0f));
```

e trocar por (adiciona a checagem de default logo depois):

```c
    EXPECT(nearf(d.emissive_amount, 0.0f));
    EXPECT(nearf(d.refraction, 0.4f));
```

Localizar o bloco de setup do teste de roundtrip:

```c
    a.emissive_amount = 0.6f;
    a.env_mode = 1;
```

e trocar por:

```c
    a.emissive_amount = 0.6f;
    a.refraction = 0.72f;
    a.env_mode = 1;
```

Localizar o bloco de assert do roundtrip:

```c
    EXPECT(nearf(b.emissive_amount, 0.6f));
    EXPECT(b.env_mode == 1);
```

e trocar por:

```c
    EXPECT(nearf(b.emissive_amount, 0.6f));
    EXPECT(nearf(b.refraction, 0.72f));
    EXPECT(b.env_mode == 1);
```

- [ ] **Step 2: Rodar os testes e confirmar que falham (erro de compilação)**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```

Esperado: **falha de compilação** em `test_config.c` (`Config` não tem membro `refraction`) — é o "vermelho" esperado nesse projeto, já que `Config` é um struct C, não há como referenciar um campo inexistente e rodar de verdade.

- [ ] **Step 3: Implementar o campo**

Em `src/config.h`, localizar:

```c
    float        emissive_amount;      /* 0..1 - 0 = desligado */
    int          env_mode;             /* 0 embutida, 1 personalizada, 2 nenhuma */
```

Trocar por:

```c
    float        emissive_amount;      /* 0..1 - 0 = desligado */
    float        refraction;           /* 0..1 - so' vidro. 0 = desliga a captura/distorcao (sem custo) */
    int          env_mode;             /* 0 embutida, 1 personalizada, 2 nenhuma */
```

Em `src/config.c`, `config_defaults`, localizar:

```c
    c->emissive_amount = 0.0f;
    c->env_mode = 0;
```

Trocar por:

```c
    c->emissive_amount = 0.0f;
    c->refraction = 0.4f;
    c->env_mode = 0;
```

Em `config_load_from`, localizar:

```c
    if (reg_get_f(k, L"emissive_amount", &f)) c->emissive_amount = f;
    if (reg_get_f(k, L"bevel_size", &f))     c->bevel_size = f;
```

Trocar por:

```c
    if (reg_get_f(k, L"emissive_amount", &f)) c->emissive_amount = f;
    if (reg_get_f(k, L"refraction", &f))      c->refraction = f;
    if (reg_get_f(k, L"bevel_size", &f))     c->bevel_size = f;
```

Na função de sanitize (mesmo arquivo), localizar:

```c
    c->emissive_amount = clampf(c->emissive_amount, 0.0f, 1.0f);
    if (c->env_mode < 0 || c->env_mode > 2) c->env_mode = 0;
```

Trocar por:

```c
    c->emissive_amount = clampf(c->emissive_amount, 0.0f, 1.0f);
    c->refraction = clampf(c->refraction, 0.0f, 1.0f);
    if (c->env_mode < 0 || c->env_mode > 2) c->env_mode = 0;
```

Em `config_save_to`, localizar:

```c
    set_f(k, L"emissive_amount", c->emissive_amount);
    set_f(k, L"env_mode", (float)c->env_mode);
```

Trocar por:

```c
    set_f(k, L"emissive_amount", c->emissive_amount);
    set_f(k, L"refraction", c->refraction);
    set_f(k, L"env_mode", (float)c->env_mode);
```

- [ ] **Step 4: Rodar os testes e confirmar que passam**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```

Esperado: `all tests passed`.

- [ ] **Step 5: Commit**

```bash
git add src/config.h src/config.c build/tests/test_config.c
git commit -m "feat: add Config.refraction field for Vidro distortion (0..1, default 0.4)"
```

---

### Task 2: Presets — roundtrip de `refraction`

**Files:**
- Modify: `src/presets.c` (scope_copy ~linha 14, dump_fields ~linha 256)

**Interfaces:**
- Consumes: `Config.refraction` (Task 1).
- Produces: `refraction` presente em `preset_scope_copy` (aplicado ao trocar de preset) e em `preset_dump_fields` (usado por backup/restauração em lote).

- [ ] **Step 1: Adicionar aos pontos de cópia**

Em `src/presets.c`, `preset_scope_copy`, localizar:

```c
    dst->emissive_amount = src->emissive_amount;
    dst->env_mode = src->env_mode;
```

Trocar por:

```c
    dst->emissive_amount = src->emissive_amount;
    dst->refraction = src->refraction;
    dst->env_mode = src->env_mode;
```

Em `preset_dump_fields`, localizar:

```c
    fwprintf(f, L"emissive_amount=%.5f\r\n", (double)from->emissive_amount);
    fwprintf(f, L"env_mode=%d\r\n", from->env_mode);
```

Trocar por:

```c
    fwprintf(f, L"emissive_amount=%.5f\r\n", (double)from->emissive_amount);
    fwprintf(f, L"refraction=%.5f\r\n", (double)from->refraction);
    fwprintf(f, L"env_mode=%d\r\n", from->env_mode);
```

- [ ] **Step 2: Build + rodar a suíte completa**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```

Esperado: `all tests passed` (não há teste dedicado de presets pra campos individuais nesse projeto — mesmo padrão já usado pros 6 campos de material da fase anterior; a suíte só precisa continuar compilando e passando).

- [ ] **Step 3: Commit**

```bash
git add src/presets.c
git commit -m "feat: round-trip Config.refraction through presets scope-copy and backup"
```

---

### Task 3: Shader (`model.frag`) + `material.h`/`.c`

**Files:**
- Modify: `shaders/model.frag`
- Modify: `src/material.h`
- Modify: `src/material.c`

**Interfaces:**
- Consumes: nada de tasks anteriores diretamente (uniforms são setados pela Task 4).
- Produces: uniforms GLSL `uGrabTex` (sampler2D, unidade de textura **1**), `uRefraction` (float), `uScreenSize` (vec2); `Material.uGrabTex/uRefraction/uScreenSize` (int, localização dos uniforms); `material_begin(m, view, proj, campos, base, fb_w, fb_h)` (nova assinatura, 2 parâmetros a mais no final); `material_set_style(m, mode, metalness, roughness, env_tex, emissive_color, emissive_amount, refraction)` (novo parâmetro `float refraction` no final) — **Task 4 depende dessas duas assinaturas exatas**.

- [ ] **Step 1: Novos uniforms em `model.frag`**

Localizar:

```glsl
uniform vec3  uEmissiveColor;
uniform float uEmissiveAmount;  // 0..1 - classico, vidro e fosco (nao metalico)
uniform sampler2D uEnvTex;
uniform int   uHasEnv;       // 0/1
```

Trocar por:

```glsl
uniform vec3  uEmissiveColor;
uniform float uEmissiveAmount;  // 0..1 - classico, vidro e fosco (nao metalico)
uniform sampler2D uEnvTex;
uniform int   uHasEnv;       // 0/1
uniform sampler2D uGrabTex;     // fundo capturado antes do vidro ser desenhado (so' usado se uRefraction > 0)
uniform float uRefraction;      // 0..1 - so' vidro. 0 = desliga (uGrabTex nem e' amostrado)
uniform vec2  uScreenSize;      // resolucao em pixels, pra converter gl_FragCoord em UV 0..1
```

- [ ] **Step 2: Ramo `uMode == 2` (Vidro) - distorção**

Localizar:

```glsl
    if (uMode == 2) {                         // vidro (passe transparente)
        vec3 env  = sample_env(R, uRoughness * 0.5);
        vec3 refr = base * 0.6;
        // vidro liso (aspereza baixa) deveria parecer bem mais espelhado/
        // polido mesmo olhando de frente, nao so nas bordas (fresnel);
        // vidro aspero fica mais opaco/fosco mesmo de frente.
        float m   = clamp(fres + 0.15 + (1.0 - uRoughness) * 0.35, 0.0, 1.0);
        vec3 col  = mix(refr, env, m);
```

Trocar por:

```glsl
    if (uMode == 2) {                         // vidro (passe transparente)
        vec3 env  = sample_env(R, uRoughness * 0.5);
        vec3 refr = base * 0.6;
        if (uRefraction > 0.0) {
            // desloca a amostra do fundo capturado com base na normal -
            // faces de frente pra camera (N.xy pequeno) desviam pouco,
            // bordas/chanfros (N mais inclinado) desviam mais - da o
            // "efeito lupa" nas bordas sem stripe fisicamente correto.
            // LOD escala com a aspereza: vidro liso fica nitido, vidro
            // aspero borra (mipmaps gerados no momento da captura).
            vec2 screenUV = gl_FragCoord.xy / uScreenSize;
            vec2 duv = clamp(screenUV + N.xy * uRefraction * 0.12, 0.002, 0.998);
            vec3 behind = textureLod(uGrabTex, duv, uRoughness * 5.0).rgb;
            refr = mix(refr, behind * mix(vec3(1.0), base, 0.4), 0.85);
        }
        // vidro liso (aspereza baixa) deveria parecer bem mais espelhado/
        // polido mesmo olhando de frente, nao so nas bordas (fresnel);
        // vidro aspero fica mais opaco/fosco mesmo de frente.
        float m   = clamp(fres + 0.15 + (1.0 - uRoughness) * 0.35, 0.0, 1.0);
        vec3 col  = mix(refr, env, m);
```

- [ ] **Step 3: `Material` struct em `material.h`**

Localizar:

```c
typedef struct {
    unsigned prog;
    int uModel, uView, uProj, uCamPos, uBaseColor;
    int uMode, uMetalness, uRoughness, uEnvTex, uHasEnv;
    int uEmissiveColor, uEmissiveAmount;
} Material;

int  material_init(Material *m);                                   /* 0 = falha */
void material_begin(const Material *m, m4 view, m4 proj, v3 campos, v3 base);
void material_set_style(const Material *m, int mode, float metalness, float roughness,
                        unsigned env_tex /* 0 = nenhuma */,
                        v3 emissive_color, float emissive_amount);
```

Trocar por:

```c
typedef struct {
    unsigned prog;
    int uModel, uView, uProj, uCamPos, uBaseColor;
    int uMode, uMetalness, uRoughness, uEnvTex, uHasEnv;
    int uEmissiveColor, uEmissiveAmount;
    int uGrabTex, uRefraction, uScreenSize;
} Material;

int  material_init(Material *m);                                   /* 0 = falha */
void material_begin(const Material *m, m4 view, m4 proj, v3 campos, v3 base, int fb_w, int fb_h);
void material_set_style(const Material *m, int mode, float metalness, float roughness,
                        unsigned env_tex /* 0 = nenhuma */,
                        v3 emissive_color, float emissive_amount,
                        float refraction);
```

- [ ] **Step 4: `material.c` — init/begin/set_style**

Em `material_init`, localizar:

```c
    m->uEmissiveColor  = glGetUniformLocation(m->prog, "uEmissiveColor");
    m->uEmissiveAmount = glGetUniformLocation(m->prog, "uEmissiveAmount");
    glUseProgram(m->prog);
    glUniform1i(m->uEnvTex, 0);   /* unidade de textura 0 */
    return 1;
```

Trocar por:

```c
    m->uEmissiveColor  = glGetUniformLocation(m->prog, "uEmissiveColor");
    m->uEmissiveAmount = glGetUniformLocation(m->prog, "uEmissiveAmount");
    m->uGrabTex        = glGetUniformLocation(m->prog, "uGrabTex");
    m->uRefraction     = glGetUniformLocation(m->prog, "uRefraction");
    m->uScreenSize     = glGetUniformLocation(m->prog, "uScreenSize");
    glUseProgram(m->prog);
    glUniform1i(m->uEnvTex, 0);   /* unidade de textura 0 */
    glUniform1i(m->uGrabTex, 1);  /* unidade de textura 1 */
    return 1;
```

Localizar:

```c
void material_begin(const Material *m, m4 view, m4 proj, v3 campos, v3 base)
{
    glUseProgram(m->prog);
    glUniformMatrix4fv(m->uView, 1, GL_FALSE, view.m);
    glUniformMatrix4fv(m->uProj, 1, GL_FALSE, proj.m);
    glUniform3f(m->uCamPos, campos.x, campos.y, campos.z);
    glUniform3f(m->uBaseColor, base.x, base.y, base.z);
}
```

Trocar por:

```c
void material_begin(const Material *m, m4 view, m4 proj, v3 campos, v3 base, int fb_w, int fb_h)
{
    glUseProgram(m->prog);
    glUniformMatrix4fv(m->uView, 1, GL_FALSE, view.m);
    glUniformMatrix4fv(m->uProj, 1, GL_FALSE, proj.m);
    glUniform3f(m->uCamPos, campos.x, campos.y, campos.z);
    glUniform3f(m->uBaseColor, base.x, base.y, base.z);
    glUniform2f(m->uScreenSize, (float)fb_w, (float)fb_h);
}
```

Localizar:

```c
void material_set_style(const Material *m, int mode, float metalness, float roughness,
                        unsigned env_tex,
                        v3 emissive_color, float emissive_amount)
{
    glUseProgram(m->prog);
    glUniform1i(m->uMode, mode);
    glUniform1f(m->uMetalness, metalness);
    glUniform1f(m->uRoughness, roughness);
    glUniform1i(m->uHasEnv, env_tex ? 1 : 0);
    glUniform3f(m->uEmissiveColor, emissive_color.x, emissive_color.y, emissive_color.z);
    glUniform1f(m->uEmissiveAmount, emissive_amount);
    if (env_tex) {
```

Trocar por:

```c
void material_set_style(const Material *m, int mode, float metalness, float roughness,
                        unsigned env_tex,
                        v3 emissive_color, float emissive_amount,
                        float refraction)
{
    glUseProgram(m->prog);
    glUniform1i(m->uMode, mode);
    glUniform1f(m->uMetalness, metalness);
    glUniform1f(m->uRoughness, roughness);
    glUniform1i(m->uHasEnv, env_tex ? 1 : 0);
    glUniform3f(m->uEmissiveColor, emissive_color.x, emissive_color.y, emissive_color.z);
    glUniform1f(m->uEmissiveAmount, emissive_amount);
    glUniform1f(m->uRefraction, refraction);
    if (env_tex) {
```

- [ ] **Step 5: Build (só compila - GLSL só é validado em runtime, ver Task 7)**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```

Esperado: `all tests passed` (nenhum teste existente chama `material_begin`/`material_set_style` diretamente - a suíte de testes não sobe um contexto GL; isso só confirma que o resto do projeto ainda compila com as novas assinaturas, já que `scene.c` só será atualizado na Task 4).

**Este passo VAI FALHAR a compilação nesta task isoladamente**, porque `scene.c` ainda chama `material_begin`/`material_set_style` com a assinatura antiga - **é esperado**: Tasks 3 e 4 são acopladas pela mudança de assinatura (mesma situação documentada na Fase 4d deste projeto, "shader rename teve que entrar no mesmo commit"). Se estiver executando as tasks em sessões/commits separados, funda as Tasks 3 e 4 num único commit, ou aplique a Task 4 em seguida antes de rodar o build.

- [ ] **Step 6: Commit (junto com a Task 4 — ver nota acima)**

Não commitar isoladamente; commit combinado ao final da Task 4.

---

### Task 4: `scene.c` — captura do fundo + wiring

**Files:**
- Modify: `src/scene.c`

**Interfaces:**
- Consumes: `Config.refraction` (Task 1); `material_begin(m, view, proj, campos, base, fb_w, fb_h)` e `material_set_style(..., float refraction)` (Task 3, assinaturas exatas).
- Produces: `SceneRenderer.grab` (GlFbo, campo de instância); `SceneRenderer.refraction` (float, sincronizado de `Config` em `scene_set_config`).

- [ ] **Step 1: Novos campos em `SceneRenderer`**

Localizar:

```c
    v3       emissive_color;
    float    emissive_amount;
    wchar_t  env_path[512];
    unsigned env_tex;
    int      env_mode;
    int      env_loaded;   /* forca 1a carga mesmo quando env_mode==0 bate com o calloc inicial */
```

Trocar por:

```c
    v3       emissive_color;
    float    emissive_amount;
    float    refraction;
    GlFbo    grab;          /* fundo capturado p/ refracao do Vidro - so' criado quando usado */
    wchar_t  env_path[512];
    unsigned env_tex;
    int      env_mode;
    int      env_loaded;   /* forca 1a carga mesmo quando env_mode==0 bate com o calloc inicial */
```

- [ ] **Step 2: Sincronizar de `Config` em `scene_set_config`**

Localizar:

```c
    s->emissive_color = (v3){ cfg->emissive_r, cfg->emissive_g, cfg->emissive_b };
    s->emissive_amount = cfg->emissive_amount;
```

Trocar por:

```c
    s->emissive_color = (v3){ cfg->emissive_r, cfg->emissive_g, cfg->emissive_b };
    s->emissive_amount = cfg->emissive_amount;
    s->refraction = cfg->refraction;
```

- [ ] **Step 3: Helper `ensure_grab_fbo` (static, novo)**

Adicionar logo **antes** de `void scene_render(...)` (a função existente que começa em `void scene_render(SceneRenderer *s, double t, int fb_w, int fb_h, int particles_active)`):

```c
/* cria (ou recria, se o tamanho mudou) a textura de captura do fundo
   pro Vidro - RGBA16F sem depth (so' recebe um blit de copia, nunca e'
   destino de desenho real), com mipmaps habilitados manualmente pra
   textureLod() funcionar (gl_fbo_color16f nao habilita mipmap por
   padrao, e' usado tambem por post.c pra alvos que nunca precisam). */
static void ensure_grab_fbo(SceneRenderer *s, int w, int h)
{
    if (s->grab.fbo && s->grab.w == w && s->grab.h == h) return;
    if (s->grab.fbo) gl_fbo_free(&s->grab);
    s->grab = gl_fbo_color16f(w, h, 0);
    glBindTexture(GL_TEXTURE_2D, s->grab.color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}
```

- [ ] **Step 4: Capturar o fundo em `scene_render`, antes de `material_begin`**

Localizar:

```c
    material_begin(&s->mat, view, proj, eye, s->base_color);
    material_set_style(&s->mat, s->material_mode, s->metalness, s->roughness, s->env_tex,
                        s->emissive_color, s->emissive_amount);
    material_set_model(&s->mat, model);
```

Trocar por:

```c
    if (s->material_mode == 2 && s->refraction > 0.0f) {
        ensure_grab_fbo(s, fb_w, fb_h);
        GLint prev_fbo = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_fbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s->grab.fbo);
        glBlitFramebuffer(0, 0, fb_w, fb_h, 0, 0, fb_w, fb_h,
                           GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
        glBindTexture(GL_TEXTURE_2D, s->grab.color);
        glGenerateMipmap(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, s->grab.color);
        glActiveTexture(GL_TEXTURE0);
    }

    material_begin(&s->mat, view, proj, eye, s->base_color, fb_w, fb_h);
    material_set_style(&s->mat, s->material_mode, s->metalness, s->roughness, s->env_tex,
                        s->emissive_color, s->emissive_amount, s->refraction);
    material_set_model(&s->mat, model);
```

- [ ] **Step 5: Liberar a FBO em `scene_destroy`**

Localizar:

```c
    env_free(s->env_tex);
    material_destroy(&s->mat);
    free(s);
```

Trocar por:

```c
    env_free(s->env_tex);
    gl_fbo_free(&s->grab);
    material_destroy(&s->mat);
    free(s);
```

- [ ] **Step 6: Build + rodar a suíte completa (Tasks 3+4 combinadas)**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
mingw32-make -f build/Makefile release
```

Esperado: `all tests passed`, `Built dist/Modern3DText.scr (... bytes)` sem erro. Isso confirma o lado C — a validade do GLSL em si só é confirmada rodando o binário de verdade (Task 7).

- [ ] **Step 7: Commit (Tasks 3 + 4 juntas)**

```bash
git add shaders/model.frag src/material.h src/material.c src/scene.c
git commit -m "feat: real screen-space refraction for Vidro (grab-pass + normal-based UV offset)"
```

---

### Task 5: Resource IDs, `.rc`, i18n

**Files:**
- Modify: `src/resource.h`
- Modify: `res/screensaver.rc`
- Modify: `src/i18n.h`
- Modify: `src/i18n.c`
- Modify: `res/lang/pt.txt`
- Modify: `res/lang/en.txt`

**Interfaces:**
- Produces: `IDC_REFRACT_LABEL`, `IDC_REFRACT_VAL`, `IDC_REFRACT` (resource IDs); `STR_MATERIAL_REFRACTION_LABEL` (i18n key) — **Task 6 depende desses 4 nomes exatos**.

- [ ] **Step 1: IDs em `resource.h`**

Localizar:

```c
#define IDC_EMISSIVE_COLOR 1318
```

Trocar por:

```c
#define IDC_EMISSIVE_COLOR 1318
#define IDC_REFRACT_LABEL  1319
#define IDC_REFRACT_VAL    1320
#define IDC_REFRACT         1321
```

- [ ] **Step 2: Controles no `.rc`**

Em `res/screensaver.rc`, dentro de `IDD_TAB_MATERIAL`, localizar:

```
    LTEXT      "Material:", IDC_MATERIAL_LABEL, 8, 10, 60, 9
    COMBOBOX   IDC_MATMODE, 8, 22, 150, 80, CBS_DROPDOWNLIST | WS_TABSTOP
    LTEXT      "Metalizacao:", IDC_METAL_LABEL, 8, 46, 90, 9
    LTEXT      "", IDC_METAL_VAL, 182, 46, 50, 9
    CONTROL    "", IDC_METAL, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 56, 224, 18
    LTEXT      "Rugosidade:", IDC_ROUGH_LABEL, 8, 80, 90, 9
```

Trocar por (adiciona o bloco de Distorção **na mesma posição física** que Metalização — os dois nunca aparecem juntos, já que Metalização só existe em Metálico e Distorção só em Vidro; o sistema de reflow dinâmico já trata esse caso, é a mesma técnica que hoje esconde Metalização fora do modo Metálico):

```
    LTEXT      "Material:", IDC_MATERIAL_LABEL, 8, 10, 60, 9
    COMBOBOX   IDC_MATMODE, 8, 22, 150, 80, CBS_DROPDOWNLIST | WS_TABSTOP
    LTEXT      "Metalizacao:", IDC_METAL_LABEL, 8, 46, 90, 9
    LTEXT      "", IDC_METAL_VAL, 182, 46, 50, 9
    CONTROL    "", IDC_METAL, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 56, 224, 18
    LTEXT      "Distorcao:", IDC_REFRACT_LABEL, 8, 46, 90, 9
    LTEXT      "", IDC_REFRACT_VAL, 182, 46, 50, 9
    CONTROL    "", IDC_REFRACT, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 56, 224, 18
    LTEXT      "Rugosidade:", IDC_ROUGH_LABEL, 8, 80, 90, 9
```

- [ ] **Step 3: Chave i18n em `i18n.h`**

Localizar:

```c
    STR_MATERIAL_EMISSIVE_LABEL, STR_MATERIAL_EMISSIVE_COLOR_BTN,
    STR_MATERIAL_ENV_LABEL, STR_PLACEHOLDER_PROCEDURAL,
```

Trocar por:

```c
    STR_MATERIAL_EMISSIVE_LABEL, STR_MATERIAL_EMISSIVE_COLOR_BTN,
    STR_MATERIAL_REFRACTION_LABEL,
    STR_MATERIAL_ENV_LABEL, STR_PLACEHOLDER_PROCEDURAL,
```

- [ ] **Step 4: Chave i18n em `i18n.c` (mesma ordem exata do enum)**

Localizar:

```c
    "material.emissive_label", "material.emissive_color_btn",
    "material.env_label", "placeholder.procedural",
```

Trocar por:

```c
    "material.emissive_label", "material.emissive_color_btn",
    "material.refraction_label",
    "material.env_label", "placeholder.procedural",
```

- [ ] **Step 5: Textos em `pt.txt`/`en.txt`**

Em `res/lang/pt.txt`, localizar:

```
material.emissive_color_btn=Cor...
material.env_label=Imagem de ambiente:
```

Trocar por:

```
material.emissive_color_btn=Cor...
material.refraction_label=Distorção:
material.env_label=Imagem de ambiente:
```

Em `res/lang/en.txt`, localizar:

```
material.emissive_color_btn=Color...
material.env_label=Environment image:
```

Trocar por:

```
material.emissive_color_btn=Color...
material.refraction_label=Distortion:
material.env_label=Environment image:
```

- [ ] **Step 6: Build (confirma que o `.rc` compila e o `embed` gera os `.txt` certos)**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile release
```

Esperado: `Built dist/Modern3DText.scr (... bytes)` sem erro do `windres`.

- [ ] **Step 7: Commit**

```bash
git add src/resource.h res/screensaver.rc src/i18n.h src/i18n.c res/lang/pt.txt res/lang/en.txt
git commit -m "feat: add Distortion slider resources, .rc layout and i18n strings for Vidro"
```

---

### Task 6: `config_dialog.c` — bloco dinâmico + handlers

**Files:**
- Modify: `src/config_dialog.c`

**Interfaces:**
- Consumes: `IDC_REFRACT_LABEL/VAL/REFRACT`, `STR_MATERIAL_REFRACTION_LABEL` (Task 5); `Config.refraction`/`g_work.refraction` (Task 1, via `g_work` que já é um `Config`).

- [ ] **Step 1: Valor exibido em `material_labels`**

Localizar:

```c
    swprintf(b, 32, L"%.2f", (double)g_work.emissive_amount); SetDlgItemTextW(h, IDC_EMISSIVE_VAL, b);
    SetDlgItemTextW(h, IDC_ENVPATH, g_work.env_path[0] ? g_work.env_path : i18n_str(STR_PLACEHOLDER_PROCEDURAL));
```

Trocar por:

```c
    swprintf(b, 32, L"%.2f", (double)g_work.emissive_amount); SetDlgItemTextW(h, IDC_EMISSIVE_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.refraction); SetDlgItemTextW(h, IDC_REFRACT_VAL, b);
    SetDlgItemTextW(h, IDC_ENVPATH, g_work.env_path[0] ? g_work.env_path : i18n_str(STR_PLACEHOLDER_PROCEDURAL));
```

- [ ] **Step 2: Label i18n em `material_apply_i18n`**

Localizar:

```c
    SetDlgItemTextW(h, IDC_EMISSIVE_COLOR, i18n_str(STR_MATERIAL_EMISSIVE_COLOR_BTN));
    SetDlgItemTextW(h, IDC_ENV_LABEL, i18n_str(STR_MATERIAL_ENV_LABEL));
```

Trocar por:

```c
    SetDlgItemTextW(h, IDC_EMISSIVE_COLOR, i18n_str(STR_MATERIAL_EMISSIVE_COLOR_BTN));
    SetDlgItemTextW(h, IDC_REFRACT_LABEL, i18n_str(STR_MATERIAL_REFRACTION_LABEL));
    SetDlgItemTextW(h, IDC_ENV_LABEL, i18n_str(STR_MATERIAL_ENV_LABEL));
```

- [ ] **Step 3: Bloco novo no reflow dinâmico**

Localizar (comentário + array + contagem):

```c
/* blocos da aba Material, na ordem em que ja aparecem no .rc - cada
   propriedade so' faz sentido (e so' tem efeito real no
   shaders/model.frag) em alguns modos:
   Metalizacao: so' Metalico.
   Rugosidade: Classico, Metalico e Vidro (Fosco ja' e' "todo rugoso").
   Emissivo: Classico, Vidro e Fosco (Metalico ja' reflete o ambiente,
   brilho proprio por cima ficaria estranho).
   Ambiente: Metalico e Vidro (unico jeito de refletir alguma coisa).
   (Verniz e Anisotropia existiram brevemente mas foram removidos -
   feedback do usuario apos testar: efeito pouco distintivo pra
   justificar mais 2 controles na UI.) */
typedef struct { int id; int x, rel_y; } MatCtrl;

#define MAT_BLOCKS 5
static MatCtrl g_mat_blocks[MAT_BLOCKS][4] = {
    { { IDC_METAL_LABEL, 0, 0 }, { IDC_METAL_VAL, 0, 0 }, { IDC_METAL, 0, 0 } },
    { { IDC_ROUGH_LABEL, 0, 0 }, { IDC_ROUGH_VAL, 0, 0 }, { IDC_ROUGH, 0, 0 } },
    { { IDC_EMISSIVE_LABEL, 0, 0 }, { IDC_EMISSIVE_COLOR, 0, 0 }, { IDC_EMISSIVE_VAL, 0, 0 }, { IDC_EMISSIVE, 0, 0 } },
    { { IDC_ENV_LABEL, 0, 0 }, { IDC_ENVMODE_EMBED, 0, 0 }, { IDC_ENVMODE_CUSTOM, 0, 0 }, { IDC_ENVMODE_NONE, 0, 0 } },
    { { IDC_ENVPATH, 0, 0 }, { IDC_ENVPICK, 0, 0 }, { IDC_ENVCLEAR, 0, 0 } },
};
static const int MAT_BLOCK_N[MAT_BLOCKS] = { 3, 3, 4, 4, 3 };
```

Trocar por:

```c
/* blocos da aba Material, na ordem em que ja aparecem no .rc - cada
   propriedade so' faz sentido (e so' tem efeito real no
   shaders/model.frag) em alguns modos:
   Metalizacao: so' Metalico.
   Distorcao: so' Vidro (ocupa a MESMA posicao fisica de Metalizacao
   no .rc - os dois nunca aparecem juntos, entao podem compartilhar
   as coordenadas de layout sem conflito real).
   Rugosidade: Classico, Metalico e Vidro (Fosco ja' e' "todo rugoso").
   Emissivo: Classico, Vidro e Fosco (Metalico ja' reflete o ambiente,
   brilho proprio por cima ficaria estranho).
   Ambiente: Metalico e Vidro (unico jeito de refletir alguma coisa).
   (Verniz e Anisotropia existiram brevemente mas foram removidos -
   feedback do usuario apos testar: efeito pouco distintivo pra
   justificar mais 2 controles na UI.) */
typedef struct { int id; int x, rel_y; } MatCtrl;

#define MAT_BLOCKS 6
static MatCtrl g_mat_blocks[MAT_BLOCKS][4] = {
    { { IDC_METAL_LABEL, 0, 0 }, { IDC_METAL_VAL, 0, 0 }, { IDC_METAL, 0, 0 } },
    { { IDC_REFRACT_LABEL, 0, 0 }, { IDC_REFRACT_VAL, 0, 0 }, { IDC_REFRACT, 0, 0 } },
    { { IDC_ROUGH_LABEL, 0, 0 }, { IDC_ROUGH_VAL, 0, 0 }, { IDC_ROUGH, 0, 0 } },
    { { IDC_EMISSIVE_LABEL, 0, 0 }, { IDC_EMISSIVE_COLOR, 0, 0 }, { IDC_EMISSIVE_VAL, 0, 0 }, { IDC_EMISSIVE, 0, 0 } },
    { { IDC_ENV_LABEL, 0, 0 }, { IDC_ENVMODE_EMBED, 0, 0 }, { IDC_ENVMODE_CUSTOM, 0, 0 }, { IDC_ENVMODE_NONE, 0, 0 } },
    { { IDC_ENVPATH, 0, 0 }, { IDC_ENVPICK, 0, 0 }, { IDC_ENVCLEAR, 0, 0 } },
};
static const int MAT_BLOCK_N[MAT_BLOCKS] = { 3, 3, 3, 4, 4, 3 };
```

- [ ] **Step 4: Visibilidade em `material_layout_apply`**

Localizar:

```c
    int mode = g_work.material_mode;
    int vis_metal      = (mode == 1);
    int vis_rough      = (mode == 0 || mode == 1 || mode == 2);
    int vis_emissive   = (mode == 0 || mode == 2 || mode == 3);
    int vis_env_hdr    = (mode == 1 || mode == 2);
    int vis_env_pick   = vis_env_hdr && (g_work.env_mode == 1);
    int visible[MAT_BLOCKS] = { vis_metal, vis_rough, vis_emissive,
                                 vis_env_hdr, vis_env_pick };
```

Trocar por:

```c
    int mode = g_work.material_mode;
    int vis_metal      = (mode == 1);
    int vis_refract    = (mode == 2);
    int vis_rough      = (mode == 0 || mode == 1 || mode == 2);
    int vis_emissive   = (mode == 0 || mode == 2 || mode == 3);
    int vis_env_hdr    = (mode == 1 || mode == 2);
    int vis_env_pick   = vis_env_hdr && (g_work.env_mode == 1);
    int visible[MAT_BLOCKS] = { vis_metal, vis_refract, vis_rough, vis_emissive,
                                 vis_env_hdr, vis_env_pick };
```

- [ ] **Step 5: Slider - init e mudança**

Localizar:

```c
            set_slider(h, IDC_EMISSIVE, 0, 100, (int)(g_work.emissive_amount * 100.0f + 0.5f));
            material_apply_i18n(h);
```

Trocar por:

```c
            set_slider(h, IDC_EMISSIVE, 0, 100, (int)(g_work.emissive_amount * 100.0f + 0.5f));
            set_slider(h, IDC_REFRACT, 0, 100, (int)(g_work.refraction * 100.0f + 0.5f));
            material_apply_i18n(h);
```

Localizar:

```c
            g_work.emissive_amount = (float)SendDlgItemMessageW(h, IDC_EMISSIVE, TBM_GETPOS, 0, 0) / 100.0f;
            material_labels(h);
```

Trocar por:

```c
            g_work.emissive_amount = (float)SendDlgItemMessageW(h, IDC_EMISSIVE, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.refraction = (float)SendDlgItemMessageW(h, IDC_REFRACT, TBM_GETPOS, 0, 0) / 100.0f;
            material_labels(h);
```

- [ ] **Step 6: Build + rodar a suíte completa**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
mingw32-make -f build/Makefile release
```

Esperado: `all tests passed`, `.scr` construído sem erro.

- [ ] **Step 7: Commit**

```bash
git add src/config_dialog.c
git commit -m "feat: wire Distortion slider into the Material tab's dynamic layout"
```

---

### Task 7: Verificação visual real + entrega

**Files:** nenhum (só verificação e comunicação).

**Interfaces:**
- Consumes: tudo das Tasks 1-6 (feature completa).

- [ ] **Step 1: Build release limpo e atualizar o `.exe`**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
mingw32-make -f build/Makefile release
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

- [ ] **Step 2: Confirmar que o shader realmente compilou/linkou**

Rodar o binário de verdade uma vez (ex.: `M3DT_TAB=2` no modo de configuração, ou `/p` de preview) e checar o log (`log_init` trunca por execução) por qualquer linha `shader`/`link` de erro - `model.frag` com um erro de sintaxe compilaria o projeto C normalmente (é só um array de bytes embutido) mas falharia silenciosamente em runtime (`gl_program` loga erro e devolve 0, `material_init` retorna falha). Esse é o único jeito de validar o GLSL nesse projeto (sem infraestrutura de validação de shader offline).

- [ ] **Step 3: Verificação visual (não automatizável - efeito puramente de aparência)**

Abrir a aba Material, selecionar Vidro, e conferir:
- `refraction = 0`: aparência idêntica a antes desta fase (cor tingida + reflexo, sem imagem real através).
- `refraction` baixo (~0.1): distorção sutil, fundo real visível levemente bendo.
- `refraction` alto (~0.9): efeito lupa bem pronunciado, principalmente nas bordas/chanfros.
- Aspereza baixa vs. alta com `refraction` fixo: nítido vs. borrado.
- Redimensionar a janela / trocar de aba e voltar pra Vidro: sem crash (a textura de captura precisa redimensionar corretamente via `ensure_grab_fbo`).
- Trocar pra outro material e voltar pro Vidro: bloco de Distorção aparece/some corretamente, sem sobrepor Rugosidade/Emissivo.

- [ ] **Step 4: Atualizar a memória do projeto**

Anexar ao arquivo de memória (`project-modern-3d-text.md`, fora deste repo) um resumo do que foi implementado, decisões tomadas (refração real via grab-pass, distorção baseada na normal, aspereza controlando blur via LOD, default 0.4, semântica "0 = desligado") e o resultado da verificação visual.

- [ ] **Step 5: Enviar o `.exe` atualizado ao usuário**

Igual sempre feito nesta sessão após um build real.
