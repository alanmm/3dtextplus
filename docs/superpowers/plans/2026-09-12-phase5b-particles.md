# Fase 5b — Partículas — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Adicionar o sistema de partículas do spec principal (§6.4) — 4
tipos (`dust`, `bokeh`, `sparks`, `stars`), simulação em CPU, renderizado
como point sprites aditivos dentro do alvo HDR — fechando a Fase 5 por
completo.

**Architecture:** Módulo dedicado `src/particles.c`/`particles.h` (um
`ParticleSystem` por `SceneRenderer`, nunca compartilhado entre monitores).
`dust`/`bokeh`/`stars` vivem numa caixa de mundo fixa na origem
(independente da rotação do texto); `sparks` nascem em vértices da parede
extrudada do texto e se desprendem para o espaço de mundo. Renderizadas em
`scene_render()`, depois do modelo 3D, dentro do HDR (participam do bloom).
Entram na escada de auto-qualidade como o novo degrau mais caro (primeiro a
cair sob carga).

**Tech Stack:** C11, OpenGL 3.3 core (`GL_POINTS` + `gl_PointSize` +
`gl_PointCoord`, sem compute shader), Win32 dialog.

## Global Constraints

- Schema do `Config` continua v2 (campos ausentes usam o default).
- `ParticleSystem` nunca é `static`/compartilhado entre contextos GL (regra
  reforçada pelo bug de VAO da Fase 5a).
- Simulação em **CPU** (decisão confirmada na conversa de design — ver
  `docs/superpowers/specs/2026-09-12-phase5b-particles-design.md` §3).
- Nova aba "Partículas", 9ª, inserida no final (índice 8). `M3DT_TAB` passa
  a aceitar `0..8`.
- Build: `mingw32-make -f build/Makefile debug` (ou `release`) a partir da
  raiz, com w64devkit no PATH
  (`export PATH="/c/Users/alanm/w64devkit/bin:$PATH"`). Testes:
  `mingw32-make -f build/Makefile test`.

---

### Task 1: Config schema + persistência

**Files:**
- Modify: `src/config.h` (struct `Config`, logo após `bg_neb_color2_b`)
- Modify: `src/config.c` (`config_defaults`, `config_load_from`,
  `config_save_to`)
- Test: `build/tests/test_config.c`

**Interfaces:**
- Produces: `particles_on` (int), `particles_kind` (int, 0..3),
  `particles_density`/`particles_speed`/`particles_size_scale` (float).
  Consumidos pelas Tasks 4-6 e 8.

- [ ] **Step 1: Campos em `Config`**

Em `src/config.h`, logo após `float bg_neb_color2_r, bg_neb_color2_g, bg_neb_color2_b;`
e antes de `} Config;`:

```c
    int   particles_on;          /* 0/1 */
    int   particles_kind;        /* 0 dust, 1 bokeh, 2 sparks, 3 stars */
    float particles_density;     /* 0..1 */
    float particles_speed;       /* 0..2 */
    float particles_size_scale;  /* 0..2 */
```

- [ ] **Step 2: Defaults em `config_defaults`**

Em `src/config.c`, logo após o bloco `bg_neb_color2_*`:

```c
    c->particles_on = 0;
    c->particles_kind = 0;
    c->particles_density = 0.5f;
    c->particles_speed = 1.0f;
    c->particles_size_scale = 1.0f;
```

(desligado por padrão, mesmo padrão de `streaks_mode`/`chroma_on`/`vignette_on`.)

- [ ] **Step 3: Carregar em `config_load_from`**

Logo antes de `RegCloseKey(k);`:

```c
    reg_get_i(k, L"particles_on", &c->particles_on);
    reg_get_i(k, L"particles_kind", &c->particles_kind);
    if (reg_get_f(k, L"particles_density", &f))    c->particles_density = f;
    if (reg_get_f(k, L"particles_speed", &f))      c->particles_speed = f;
    if (reg_get_f(k, L"particles_size_scale", &f)) c->particles_size_scale = f;
```

E no bloco de saneamento (antes de `if (c->text[0] == 0) ...`):

```c
    c->particles_on = c->particles_on ? 1 : 0;
    if (c->particles_kind < 0 || c->particles_kind > 3) c->particles_kind = 0;
    c->particles_density    = clampf(c->particles_density, 0.0f, 1.0f);
    c->particles_speed      = clampf(c->particles_speed, 0.0f, 2.0f);
    c->particles_size_scale = clampf(c->particles_size_scale, 0.0f, 2.0f);
```

- [ ] **Step 4: Salvar em `config_save_to`**

Logo após o último `set_f(k, L"bg_neb_color2_b", ...)`:

```c
    set_f(k, L"particles_on", (float)c->particles_on);
    set_f(k, L"particles_kind", (float)c->particles_kind);
    set_f(k, L"particles_density", c->particles_density);
    set_f(k, L"particles_speed", c->particles_speed);
    set_f(k, L"particles_size_scale", c->particles_size_scale);
```

- [ ] **Step 5: Estender `build/tests/test_config.c`**

No bloco de defaults:

```c
    EXPECT(d.particles_on == 0);
    EXPECT(nearf(d.particles_density, 0.5f));
```

No bloco de round-trip, junto das outras atribuições de `a`:

```c
    a.particles_on = 1;
    a.particles_kind = 2;
    a.particles_density = 0.75f;
    a.particles_speed = 1.6f;
    a.particles_size_scale = 0.4f;
```

E nas verificações de `b`:

```c
    EXPECT(b.particles_on == 1);
    EXPECT(b.particles_kind == 2);
    EXPECT(nearf(b.particles_density, 0.75f));
    EXPECT(nearf(b.particles_speed, 1.6f));
    EXPECT(nearf(b.particles_size_scale, 0.4f));
```

No bloco de valores fora de faixa (array `kv[]`, hoje com 16 entradas —
trocar o `for` para `20` e adicionar):

```c
            { L"particles_kind", L"9" }, { L"particles_density", L"-1" },
            { L"particles_speed", L"9" }, { L"particles_size_scale", L"-1" },
```

E nas verificações de `e`:

```c
    EXPECT(e.particles_kind == 0);                /* 9 -> fora de 0..3 -> 0 */
    EXPECT(e.particles_density >= 0.0f);           /* -1 -> clamp */
    EXPECT(e.particles_speed <= 2.0f);             /* 9 -> clamp */
    EXPECT(e.particles_size_scale >= 0.0f);        /* -1 -> clamp */
```

- [ ] **Step 6: Rodar os testes**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```

Esperado: build sem erro, todos os `EXPECT` passam.

- [ ] **Step 7: Commit**

```bash
git add src/config.h src/config.c build/tests/test_config.c
git commit -m "feat: add particle system config fields"
```

---

### Task 2: Escada de auto-qualidade — partículas como novo primeiro degrau

**Files:**
- Modify: `src/render_tiers.h` (struct `RenderQuality`)
- Modify: `src/render_tiers.c` (`render_quality_ladder_len`,
  `render_quality_for_step`)
- Test: `build/tests/test_render_tiers.c`

**Interfaces:**
- Produces: `RenderQuality.particles` (int, gate 0/1). Consumido pela
  Task 6 (`gl_window.c`).

- [ ] **Step 1: Novo campo em `RenderQuality`**

Em `src/render_tiers.h`, dentro do `typedef struct { ... } RenderQuality;`,
logo antes de `int streaks;`:

```c
    int   particles;     /* 0/1: gate de qualidade (o gl_window faz AND com cfg.particles_on) */
```

- [ ] **Step 2: `render_quality_ladder_len`**

Em `src/render_tiers.c`:

```c
int render_quality_ladder_len(M3dtTier tier)
{
    return tier == M3DT_TIER_REDUCED ? 1 : 9;
}
```

- [ ] **Step 3: `render_quality_for_step`**

Substituir o corpo por (adiciona `particles` no topo do ladder, renumerando
os degraus existentes):

```c
RenderQuality render_quality_for_step(const Config *cfg, M3dtTier tier, int step)
{
    int len = render_quality_ladder_len(tier);
    if (step < 0) step = 0;
    if (step > len - 1) step = len - 1;

    RenderQuality q;
    q.step = step;

    if (tier == M3DT_TIER_REDUCED) {
        q.particles = 0;
        q.streaks = 0;
        q.bloom = 0;
        q.msaa = mini(cfg->msaa, 2);
        q.render_scale = minf(cfg->render_scale, 0.75f);
        return q;
    }

    /* FULL: ladder de 9 passos.
       0: topo  1: particulas off  2: streaks off  3: bloom off
       4: msaa<=4  5: msaa<=2  6: msaa 0  7: scale<=0.75  8: scale 0.5 */
    q.particles = (step >= 1) ? 0 : 1;
    q.streaks   = (step >= 2) ? 0 : 1;
    q.bloom     = (step >= 3) ? 0 : 1;
    q.msaa = cfg->msaa;
    if (step >= 4) q.msaa = mini(q.msaa, 4);
    if (step >= 5) q.msaa = mini(q.msaa, 2);
    if (step >= 6) q.msaa = 0;
    q.render_scale = cfg->render_scale;
    if (step >= 7) q.render_scale = minf(q.render_scale, 0.75f);
    if (step >= 8) q.render_scale = 0.5f;
    return q;
}
```

- [ ] **Step 4: Atualizar `build/tests/test_render_tiers.c`**

Substituir o bloco `/* --- RenderQuality: ladder --- */` inteiro por:

```c
    RenderQuality q0 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 0);
    EXPECT(q0.particles == 1 && q0.streaks == 1 && q0.bloom == 1 && q0.msaa == 4 && q0.render_scale > 0.99f);
    RenderQuality q1 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 1);
    EXPECT(q1.particles == 0 && q1.streaks == 1);
    RenderQuality q2 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 2);
    EXPECT(q2.streaks == 0 && q2.bloom == 1);
    RenderQuality q3 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 3);
    EXPECT(q3.bloom == 0 && q3.msaa == 4);
    RenderQuality q5 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 5);
    EXPECT(q5.msaa == 2);
    RenderQuality q6 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 6);
    EXPECT(q6.msaa == 0);
    RenderQuality q7 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 7);
    EXPECT(q7.render_scale <= 0.75f + 1e-4f);
    RenderQuality q8 = render_quality_for_step(&cfg, M3DT_TIER_FULL, 8);
    EXPECT(q8.render_scale <= 0.5f + 1e-4f);
    RenderQuality qhi = render_quality_for_step(&cfg, M3DT_TIER_FULL, 99);   /* clamp */
    EXPECT(qhi.step == 8);
    EXPECT(render_quality_ladder_len(M3DT_TIER_FULL) == 9);

    RenderQuality qr = render_quality_for_step(&cfg, M3DT_TIER_REDUCED, 0);
    EXPECT(qr.particles == 0 && qr.streaks == 0 && qr.bloom == 0 && qr.msaa <= 2 && qr.render_scale <= 0.75f + 1e-4f);
    EXPECT(render_quality_ladder_len(M3DT_TIER_REDUCED) == 1);
```

(o bloco de `aq_decide` logo abaixo não muda — testa a decisão pura, que é
agnóstica ao comprimento real do ladder de produção.)

- [ ] **Step 5: Rodar os testes**

```bash
mingw32-make -f build/Makefile test
```

- [ ] **Step 6: Commit**

```bash
git add src/render_tiers.h src/render_tiers.c build/tests/test_render_tiers.c
git commit -m "feat: add particles as the new first auto-quality degrade step"
```

---

### Task 3: Shaders de partícula + embed

**Files:**
- Create: `shaders/particle.vert`
- Create: `shaders/particle.frag`
- Modify: `build/Makefile:48-52` (`EMBED_INPUTS`)

**Interfaces:**
- Produces: símbolos `EMBED_particle_vert`/`EMBED_particle_frag` em
  `generated/embedded.h`. Consumidos pela Task 4.

- [ ] **Step 1: `shaders/particle.vert`**

```glsl
#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aSize;
layout(location = 2) in vec4  aColor;

uniform mat4 uView;
uniform mat4 uProj;

out vec4 vColor;

void main()
{
    vec4 viewPos = uView * vec4(aPos, 1.0);
    float dist = max(-viewPos.z, 0.1);
    gl_PointSize = clamp(aSize * 220.0 / dist, 1.0, 128.0);
    gl_Position = uProj * viewPos;
    vColor = aColor;
}
```

- [ ] **Step 2: `shaders/particle.frag`**

```glsl
#version 330 core
in vec4 vColor;
out vec4 fragColor;

uniform int uRing;   /* 0 disco cheio (dust/sparks/stars), 1 anel suave (bokeh) */

void main()
{
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float d = length(uv);
    float alpha;
    if (uRing != 0) {
        alpha = smoothstep(1.0, 0.7, d) - smoothstep(0.55, 0.25, d) * 0.6;
    } else {
        alpha = smoothstep(1.0, 0.0, d);
    }
    if (alpha <= 0.001) discard;
    fragColor = vec4(vColor.rgb, vColor.a * alpha);
}
```

- [ ] **Step 3: Adicionar ao `EMBED_INPUTS`**

Em `build/Makefile`:

```makefile
EMBED_INPUTS := shaders/model.vert shaders/model.frag shaders/fullscreen.vert \
                shaders/post_bright.frag shaders/post_down.frag shaders/post_up.frag \
                shaders/post_combine.frag shaders/post_streak.frag \
                shaders/post_finish.frag shaders/post_fxaa.frag shaders/background.frag \
                shaders/particle.vert shaders/particle.frag
```

- [ ] **Step 4: Verificar o embed isoladamente**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
rm -f generated/embedded.h
mingw32-make -f build/Makefile generated/embedded.h
grep -c "EMBED_particle_vert\[\]" generated/embedded.h
grep -c "EMBED_particle_frag\[\]" generated/embedded.h
```

Esperado: `1` para cada.

- [ ] **Step 5: Commit**

```bash
git add shaders/particle.vert shaders/particle.frag build/Makefile
git commit -m "feat: add particle.vert/frag shaders (point sprites, disc/ring)"
```

---

### Task 4: Módulo `src/particles.c`

**Files:**
- Create: `src/particles.h`
- Create: `src/particles.c`
- Modify: `build/Makefile:35-40` (`SRC_C`)

**Interfaces:**
- Consumes: `Config` (Task 1: `particles_kind/density/speed/size_scale`),
  `v3`/`m4` (`util/mathx.h`), `EMBED_particle_vert`/`EMBED_particle_frag`
  (Task 3), `gl_program` (`gl_core.h`).
- Produces: `ParticleSystem` (opaco) + `particles_create`/
  `particles_set_config`/`particles_update`/`particles_render`/
  `particles_destroy`. Consumidos pela Task 5 (`scene.c`).

- [ ] **Step 1: `src/particles.h`**

```c
#ifndef M3DT_PARTICLES_H
#define M3DT_PARTICLES_H
#include "config.h"
#include "util/mathx.h"

typedef struct ParticleSystem ParticleSystem;

/* Requer contexto GL corrente. NULL em falha. */
ParticleSystem *particles_create(void);

/* hx/hy/hz = meia-extensao atual do texto (mesma unidade de scene.c).
   wall_pos/wall_n = vertices com surfaceId==2 (parede) do MeshData atual,
   em espaco local da malha - usados como pontos de emissao de sparks.
   Pode ser chamado com wall_count == 0 (ainda sem malha). */
void particles_set_config(ParticleSystem *p, const Config *cfg,
                          float hx, float hy, float hz,
                          const v3 *wall_pos, const v3 *wall_n, int wall_count);

/* model = matriz atual do texto (usada so' para nascer sparks em espaco de
   mundo). dt em segundos, ja com clamp feito pelo chamador. */
void particles_update(ParticleSystem *p, float dt, m4 model);

/* view/proj identicas as usadas pelo modelo 3D no mesmo frame. */
void particles_render(ParticleSystem *p, m4 view, m4 proj);

void particles_destroy(ParticleSystem *p);

#endif
```

- [ ] **Step 2: `src/particles.c`**

```c
#include "particles.h"
#include "gl_core.h"
#include "util/log.h"
#include "embedded.h"

#include <glad/gl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PARTICLES_MAX   4000
#define WALL_SAMPLE_MAX 512

typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float age, life;   /* sparks: idade/vida real; stars: age reaproveitado como fase de cintilacao */
    float size;
    float r, g, b;
} Particle;

typedef struct { float x, y, z, size, r, g, b, a; } ParticleVertex;

struct ParticleSystem {
    unsigned prog, vao, vbo;

    Particle       buf[PARTICLES_MAX];
    ParticleVertex gpu_buf[PARTICLES_MAX];
    int            count;

    int   kind;      /* -1 = ainda nao inicializado */
    float density, speed, size_scale;
    float box_hx, box_hy, box_hz;

    float time;
    float star_angle;
    float emit_accum;
    unsigned seed;

    v3  wall_pos[WALL_SAMPLE_MAX];
    v3  wall_n[WALL_SAMPLE_MAX];
    int wall_count;
};

/* ---------------- ruido em CPU (mesmo estilo do fBm de shaders/background.frag,
   reimplementado aqui porque simulacao roda em C, nao em GLSL) ---------------- */

static float hashf(unsigned *seed)
{
    *seed = *seed * 1664525u + 1013904223u;
    return (float)(*seed >> 8) / (float)(1u << 24);
}

static float hash21(float x, float y)
{
    float h = sinf(x * 127.1f + y * 311.7f) * 43758.5453f;
    return h - floorf(h);
}

static float value_noise2(float x, float y)
{
    float ix = floorf(x), iy = floorf(y);
    float fx = x - ix, fy = y - iy;
    float a = hash21(ix, iy),          b = hash21(ix + 1.0f, iy);
    float c = hash21(ix, iy + 1.0f),   d = hash21(ix + 1.0f, iy + 1.0f);
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);
    return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
}

static void curl2d(float x, float y, float *ox, float *oy)
{
    const float e = 0.06f;
    float n1 = value_noise2(x, y + e), n2 = value_noise2(x, y - e);
    float n3 = value_noise2(x + e, y), n4 = value_noise2(x - e, y);
    *ox =  (n1 - n2) / (2.0f * e);
    *oy = -(n3 - n4) / (2.0f * e);
}

/* ---------------- ciclo de vida ---------------- */

ParticleSystem *particles_create(void)
{
    ParticleSystem *p = (ParticleSystem *)calloc(1, sizeof *p);
    if (!p) return NULL;

    p->prog = gl_program((const char *)EMBED_particle_vert, (const char *)EMBED_particle_frag);
    if (!p->prog) { free(p); return NULL; }

    glGenVertexArrays(1, &p->vao);
    glGenBuffers(1, &p->vbo);
    glBindVertexArray(p->vao);
    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(PARTICLES_MAX * (int)sizeof(ParticleVertex)), NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void *)(4 * sizeof(float)));
    glBindVertexArray(0);

    p->kind = -1;
    p->seed = 0x9E3779B9u;
    glEnable(GL_PROGRAM_POINT_SIZE);
    return p;
}

static void spawn_ambient(ParticleSystem *p)
{
    int target = 0;
    float size = 0.05f, r = 1.0f, g = 1.0f, b = 1.0f;
    switch (p->kind) {
        case 0: target = (int)(p->density * 800.0f); size = 0.035f; r = g = b = 1.0f; break;               /* dust */
        case 1: target = (int)(p->density * 60.0f);  size = 0.16f;  r = 1.0f; g = 0.95f; b = 0.85f; break;  /* bokeh */
        case 3: target = (int)(p->density * 300.0f); size = 0.03f;  r = 0.85f; g = 0.9f; b = 1.0f; break;   /* stars */
        default: target = 0; break;
    }
    if (target > PARTICLES_MAX) target = PARTICLES_MAX;
    p->count = target;
    for (int i = 0; i < target; ++i) {
        Particle *pt = &p->buf[i];
        pt->x = (hashf(&p->seed) * 2.0f - 1.0f) * p->box_hx;
        pt->y = (hashf(&p->seed) * 2.0f - 1.0f) * p->box_hy;
        pt->z = (hashf(&p->seed) * 2.0f - 1.0f) * p->box_hz;
        pt->vx = pt->vy = pt->vz = 0.0f;
        pt->age = hashf(&p->seed) * 6.2831853f;   /* fase de cintilacao - so' stars usam */
        pt->life = 0.0f;
        pt->size = size;
        pt->r = r; pt->g = g; pt->b = b;
    }
}

void particles_set_config(ParticleSystem *p, const Config *cfg,
                          float hx, float hy, float hz,
                          const v3 *wall_pos, const v3 *wall_n, int wall_count)
{
    int kind_changed = (p->kind != cfg->particles_kind);

    p->density    = cfg->particles_density;
    p->speed      = cfg->particles_speed;
    p->size_scale = cfg->particles_size_scale;
    p->box_hx = hx * 3.0f + 1.0f;
    p->box_hy = hy * 3.0f + 1.0f;
    p->box_hz = hz * 6.0f + 2.0f;

    int n = wall_count;
    if (n > WALL_SAMPLE_MAX) n = WALL_SAMPLE_MAX;
    p->wall_count = n;
    if (n > 0) {
        int step = wall_count / n; if (step < 1) step = 1;
        for (int i = 0; i < n; ++i) {
            int src = (i * step) % wall_count;
            p->wall_pos[i] = wall_pos[src];
            p->wall_n[i]   = wall_n[src];
        }
    }

    if (kind_changed) {
        p->kind = cfg->particles_kind;
        p->count = 0;
        p->emit_accum = 0.0f;
        p->star_angle = 0.0f;
        if (p->kind != 2) spawn_ambient(p);   /* sparks (kind 2) comecam vazias, emitem aos poucos */
    } else if (p->kind == 0 || p->kind == 1 || p->kind == 3) {
        spawn_ambient(p);   /* densidade pode ter mudado sem trocar de tipo - re-semeia */
    }
}

static void update_ambient_drift(ParticleSystem *p, float dt)
{
    for (int i = 0; i < p->count; ++i) {
        Particle *pt = &p->buf[i];
        float cx, cy;
        curl2d(pt->x * 0.25f + p->time * 0.05f, pt->z * 0.25f, &cx, &cy);
        pt->vx = cx * 0.5f;
        pt->vz = cy * 0.5f;
        pt->vy = -0.03f;
        pt->x += pt->vx * dt * p->speed;
        pt->y += pt->vy * dt * p->speed;
        pt->z += pt->vz * dt * p->speed;
        if (pt->x >  p->box_hx) pt->x -= 2.0f * p->box_hx; else if (pt->x < -p->box_hx) pt->x += 2.0f * p->box_hx;
        if (pt->y >  p->box_hy) pt->y -= 2.0f * p->box_hy; else if (pt->y < -p->box_hy) pt->y += 2.0f * p->box_hy;
        if (pt->z >  p->box_hz) pt->z -= 2.0f * p->box_hz; else if (pt->z < -p->box_hz) pt->z += 2.0f * p->box_hz;
    }
}

static void update_stars(ParticleSystem *p, float dt)
{
    const float STAR_ANGULAR_SPEED = 1.5f;   /* graus/seg, em speed=1 */
    p->star_angle += STAR_ANGULAR_SPEED * p->speed * dt;
}

#define SPARK_RATE_MAX  50.0f   /* particulas/seg em density=1 */
#define SPARK_GRAVITY    1.2f

static void update_sparks(ParticleSystem *p, float dt, m4 model)
{
    for (int i = 0; i < p->count; ) {
        Particle *pt = &p->buf[i];
        pt->age += dt;
        if (pt->age >= pt->life) {
            *pt = p->buf[p->count - 1];
            p->count--;
            continue;
        }
        pt->vy -= SPARK_GRAVITY * dt;
        pt->x += pt->vx * dt;
        pt->y += pt->vy * dt;
        pt->z += pt->vz * dt;
        ++i;
    }

    if (p->wall_count <= 0) return;
    p->emit_accum += dt * p->density * SPARK_RATE_MAX;
    while (p->emit_accum >= 1.0f && p->count < PARTICLES_MAX) {
        p->emit_accum -= 1.0f;
        int idx = (int)(hashf(&p->seed) * (float)p->wall_count);
        if (idx >= p->wall_count) idx = p->wall_count - 1;

        /* model e' rotacao pura (sem translacao/escala) em scene.c, entao
           tambem serve para transformar a normal - sem precisar de uma
           inversa-transposta separada. */
        v3 wp = m4_mul_point(model, p->wall_pos[idx]);
        v3 wn = m4_mul_point(model, p->wall_n[idx]);

        Particle *pt = &p->buf[p->count++];
        float sp = 1.2f * p->speed;
        pt->x = wp.x; pt->y = wp.y; pt->z = wp.z;
        pt->vx = wn.x * sp;
        pt->vy = wn.y * sp + 0.6f * sp;
        pt->vz = wn.z * sp;
        pt->age = 0.0f;
        pt->life = 0.5f + hashf(&p->seed) * 1.0f;
        pt->size = 0.02f;
        pt->r = 1.0f; pt->g = 0.55f; pt->b = 0.12f;
    }
}

void particles_update(ParticleSystem *p, float dt, m4 model)
{
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    p->time += dt;

    switch (p->kind) {
        case 0: case 1: update_ambient_drift(p, dt); break;
        case 2: update_sparks(p, dt, model); break;
        case 3: update_stars(p, dt); break;
        default: break;
    }
}

void particles_render(ParticleSystem *p, m4 view, m4 proj)
{
    if (p->count <= 0 || !p->prog) return;

    m4 star_rot = m4_rotate_y(m3dt_radians(p->star_angle));
    for (int i = 0; i < p->count; ++i) {
        const Particle *pt = &p->buf[i];
        ParticleVertex *gv = &p->gpu_buf[i];
        v3 pos = { pt->x, pt->y, pt->z };
        if (p->kind == 3) pos = m4_mul_point(star_rot, pos);

        gv->x = pos.x; gv->y = pos.y; gv->z = pos.z;
        gv->size = pt->size * p->size_scale;

        float alpha = 1.0f;
        if (p->kind == 2) {
            float u = pt->life > 0.0f ? pt->age / pt->life : 1.0f;
            alpha = 1.0f - u;
            gv->size *= (1.0f - 0.5f * u);
        } else if (p->kind == 3) {
            alpha = 0.4f + 0.6f * (0.5f + 0.5f * sinf(p->time * 2.0f + pt->age));
        }
        gv->r = pt->r; gv->g = pt->g; gv->b = pt->b; gv->a = alpha;
    }

    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(p->count * (int)sizeof(ParticleVertex)), p->gpu_buf);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    glUseProgram(p->prog);
    glUniformMatrix4fv(glGetUniformLocation(p->prog, "uView"), 1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(p->prog, "uProj"), 1, GL_FALSE, proj.m);
    glUniform1i(glGetUniformLocation(p->prog, "uRing"), p->kind == 1 ? 1 : 0);

    glBindVertexArray(p->vao);
    glDrawArrays(GL_POINTS, 0, p->count);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void particles_destroy(ParticleSystem *p)
{
    if (!p) return;
    if (p->vbo) glDeleteBuffers(1, &p->vbo);
    if (p->vao) glDeleteVertexArrays(1, &p->vao);
    if (p->prog) glDeleteProgram(p->prog);
    free(p);
}
```

- [ ] **Step 3: Adicionar ao `SRC_C` do Makefile**

Em `build/Makefile`, no bloco `SRC_C :=`, logo após `src/post.c \`:

```makefile
SRC_C := src/main.c src/cmdline.c src/util/log.c src/util/mathx.c src/gl_core.c \
         src/gl_window.c src/host_win32.c src/config_dialog.c src/config.c \
         src/material.c src/scene.c src/env.c src/post.c src/particles.c \
         src/render_tiers.c src/render_tiers_gl.c \
         src/geometry/font_outline.c src/geometry/stb_impl.c src/geometry/contour_mesh.c \
         src/geometry/sdf.c
```

- [ ] **Step 4: Build de depuração (só compila `particles.c` isolado ainda
  não é chamado de lugar nenhum, mas precisa compilar e linkar limpo)**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile debug
```

Esperado: build sem erro (com aviso esperado de "particles_create definida
mas não usada" é impossível aqui pois é uma função pública declarada em
`.h`, não deve gerar warning; se algum warning `-Wall`/`-Wextra` aparecer,
investigar antes de prosseguir).

- [ ] **Step 5: Commit**

```bash
git add src/particles.h src/particles.c build/Makefile
git commit -m "feat: add particles.c - CPU particle simulation (dust/bokeh/sparks/stars)"
```

---

### Task 5: Integração em `scene.c`

**Files:**
- Modify: `src/scene.c`
- Modify: `src/scene.h`

**Interfaces:**
- Consumes: `particles_create/set_config/update/render/destroy`
  (`particles.h`, Task 4).
- Produces: `scene_render` ganha um novo parâmetro `int particles_active`
  no fim da assinatura. Consumido pela Task 6 (`gl_window.c`, único
  chamador externo).

- [ ] **Step 1: Include**

Em `src/scene.c:1-16`, adicionar `#include "particles.h"` junto dos
outros includes de projeto (depois de `#include "env.h"`).

- [ ] **Step 2: Novos campos em `struct SceneRenderer`**

Logo após os campos de fundo (`bg_neb_color1, bg_neb_color2;`):

```c
    /* particulas */
    ParticleSystem *particles;
    v3     wall_pos_cache[512];
    v3     wall_n_cache[512];
    int    wall_cache_count;
    double last_t;
    int    have_last_t;
```

- [ ] **Step 3: Criar em `scene_create`**

Logo após o bloco que cria `s->bg_prog` (e antes de
`glEnable(GL_DEPTH_TEST);`):

```c
    s->particles = particles_create();
    if (!s->particles) {
        glDeleteProgram(s->bg_prog);
        material_destroy(&s->mat);
        free(s);
        return NULL;
    }
```

- [ ] **Step 4: Extrair vértices da parede em `rebuild_mesh`**

Em `rebuild_mesh`, logo após `if (s->hy < 1e-3f) s->hy = 1.0f;` e antes de
`upload_sdf(s, md.has_sdf ? &md.sdf : NULL);`:

```c
    {
        int wc = 0;
        for (int i = 0; i < md.nverts && wc < 512; ++i) {
            if (md.verts[i].surf == 2.0f) {
                s->wall_pos_cache[wc] = (v3){ md.verts[i].px, md.verts[i].py, md.verts[i].pz };
                s->wall_n_cache[wc]   = (v3){ md.verts[i].nx, md.verts[i].ny, md.verts[i].nz };
                ++wc;
            }
        }
        s->wall_cache_count = wc;
    }
```

(amostra por ordem de índice até o limite de 512 — suficiente para um
efeito decorativo; se sobrarem candidatos fora do limite, simplesmente não
entram na amostra.)

- [ ] **Step 5: Repassar config em `scene_set_config`**

No fim de `scene_set_config` (depois do bloco `if (mesh_dirty) { ... }`):

```c
    particles_set_config(s->particles, cfg, s->hx, s->hy, s->hz,
                         s->wall_pos_cache, s->wall_n_cache, s->wall_cache_count);
```

(chamado sempre, mesmo quando a malha não foi reconstruída — assim mudar
só densidade/velocidade/tipo funciona sem precisar mexer em texto/fonte.)

- [ ] **Step 6: Atualizar assinatura de `scene_render`**

Em `src/scene.h`, trocar:

```c
void scene_render(SceneRenderer *s, double t, int fb_w, int fb_h);
```

por:

```c
void scene_render(SceneRenderer *s, double t, int fb_w, int fb_h, int particles_active);
```

Em `src/scene.c`, atualizar a assinatura da definição para bater.

- [ ] **Step 7: Calcular `dt` e desenhar as partículas em `scene_render`**

Logo no início do corpo de `scene_render` (depois do clamp de `fb_w`/`fb_h`,
antes do `glViewport`):

```c
    float dt = 0.0f;
    if (s->have_last_t) {
        dt = (float)(t - s->last_t);
        if (dt < 0.0f) dt = 0.0f;
        if (dt > 0.1f) dt = 0.1f;
    }
    s->last_t = t;
    s->have_last_t = 1;
```

E no fim da função, logo após o bloco `if (glass) { glDepthMask(GL_TRUE); glDisable(GL_BLEND); }`
(antes do `}` que fecha `scene_render`):

```c
    if (particles_active) {
        particles_update(s->particles, dt, model);
        particles_render(s->particles, view, proj);
    }
```

- [ ] **Step 8: Liberar em `scene_destroy`**

Logo após `if (s->sdf_tex) glDeleteTextures(1, &s->sdf_tex);`:

```c
    particles_destroy(s->particles);
```

- [ ] **Step 9: Commit**

(feito junto da Task 6, já que o único chamador de `scene_render` fica em
`gl_window.c` — sem esse ajuste o projeto não compila. Ver Step final da
Task 6.)

---

### Task 6: Wiring em `gl_window.c`

**Files:**
- Modify: `src/gl_window.c`

**Interfaces:**
- Consumes: `scene_render(..., int particles_active)` (Task 5),
  `RenderQuality.particles` (Task 2), `Config.particles_on` (Task 1).

- [ ] **Step 1: Novo campo em `struct GlWindow`**

Em `src/gl_window.c:42-55`, logo após `int preview;`:

```c
    int   particles_on;
```

- [ ] **Step 2: Gate de qualidade**

Logo após a função `frame_bloom`:

```c
static int frame_particles(const GlWindow *g)
{
    if (g->preview) return 0;
    return (g->quality.particles && g->particles_on) ? 1 : 0;
}
```

- [ ] **Step 3: Passar o gate para `scene_render`**

Em `render_into_post`, trocar:

```c
    if (g->scene) scene_render(g->scene, t, sw, sh);
```

por:

```c
    if (g->scene) scene_render(g->scene, t, sw, sh, frame_particles(g));
```

- [ ] **Step 4: Guardar `particles_on` em `gl_window_set_config`**

Logo após `g->post_params.fxaa_on = cfg->fxaa_on;`:

```c
    g->particles_on = cfg->particles_on;
```

- [ ] **Step 5: Build de depuração completo**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile debug
```

Esperado: `Built dist/Modern3DText.scr (N bytes)`, sem erros.

- [ ] **Step 6: Rodar a suite de testes**

```bash
mingw32-make -f build/Makefile test
```

- [ ] **Step 7: Commit (Tasks 5 + 6 juntas, já que uma não compila sem a outra)**

```bash
git add src/scene.c src/scene.h src/gl_window.c
git commit -m "feat: render particles in scene.c, gated by config + auto-quality"
```

---

### Task 7: IDs de recurso + template da aba "Partículas"

**Files:**
- Modify: `src/resource.h`
- Modify: `res/screensaver.rc`

**Interfaces:**
- Produces: `IDD_TAB_PARTICLES` (118); `IDC_PARTON`, `IDC_PARTKIND`,
  `IDC_PARTDENS`, `IDC_PARTDENS_VAL`, `IDC_PARTSPEED`,
  `IDC_PARTSPEED_VAL`, `IDC_PARTSIZE`, `IDC_PARTSIZE_VAL` (1900-1907).
  Consumidos pela Task 8.

- [ ] **Step 1: Novos IDs em `src/resource.h`**

Logo após `#define IDD_TAB_BG        117`:

```c
#define IDD_TAB_PARTICLES 118
```

E no final do arquivo, logo antes do `#endif` (depois do bloco "aba Fundo"):

```c
/* aba Particulas */
#define IDC_PARTON        1900
#define IDC_PARTKIND      1901
#define IDC_PARTDENS      1902
#define IDC_PARTDENS_VAL  1903
#define IDC_PARTSPEED     1904
#define IDC_PARTSPEED_VAL 1905
#define IDC_PARTSIZE      1906
#define IDC_PARTSIZE_VAL  1907
```

- [ ] **Step 2: Template `IDD_TAB_PARTICLES` em `res/screensaver.rc`**

Logo após o `END` de `IDD_TAB_BG`, antes de `VS_VERSION_INFO VERSIONINFO`:

```rc
IDD_TAB_PARTICLES DIALOGEX 0, 0, 240, 200
STYLE DS_SETFONT | DS_CONTROL | WS_CHILD
FONT 9, "Segoe UI", 400, 0, 0x1
BEGIN
    AUTOCHECKBOX "Ativar particulas", IDC_PARTON, 8, 8, 140, 12

    LTEXT      "Tipo:", -1, 8, 28, 40, 9
    COMBOBOX   IDC_PARTKIND, 60, 26, 140, 80, CBS_DROPDOWNLIST | WS_TABSTOP

    LTEXT      "Densidade:", -1, 8, 48, 80, 9
    LTEXT      "", IDC_PARTDENS_VAL, 182, 48, 50, 9
    CONTROL    "", IDC_PARTDENS, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 58, 224, 16

    LTEXT      "Velocidade:", -1, 8, 78, 80, 9
    LTEXT      "", IDC_PARTSPEED_VAL, 182, 78, 50, 9
    CONTROL    "", IDC_PARTSPEED, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 88, 224, 16

    LTEXT      "Tamanho:", -1, 8, 108, 80, 9
    LTEXT      "", IDC_PARTSIZE_VAL, 182, 108, 50, 9
    CONTROL    "", IDC_PARTSIZE, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 118, 224, 16
END
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
git commit -m "feat: add IDD_TAB_PARTICLES dialog template and resource IDs"
```

---

### Task 8: Wiring em `config_dialog.c`

**Files:**
- Modify: `src/config_dialog.c`

**Interfaces:**
- Consumes: `IDD_TAB_PARTICLES` e IDs `IDC_PART*` (Task 7); campos
  `particles_*` em `Config` (Task 1); `preview_dirty`/`set_slider` (já
  existentes no arquivo).

- [ ] **Step 1: Global `g_particles`**

Logo após `static HWND g_bg;`:

```c
static HWND       g_particles;   /* sub-dialogo da aba Particulas */
```

- [ ] **Step 2: `particles_labels`, `particles_enable`, `particles_proc`**

Logo após o fim de `bg_proc` (antes do comentário
`/* ---------------- dialogo principal ---------------- */`):

```c
/* ---------------- aba Particulas ---------------- */

static void particles_labels(HWND h)
{
    wchar_t b[32];
    swprintf(b, 32, L"%.2f", (double)g_work.particles_density);    SetDlgItemTextW(h, IDC_PARTDENS_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.particles_speed);      SetDlgItemTextW(h, IDC_PARTSPEED_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.particles_size_scale); SetDlgItemTextW(h, IDC_PARTSIZE_VAL, b);
}

static void particles_enable(HWND h)
{
    BOOL on = g_work.particles_on ? TRUE : FALSE;
    EnableWindow(GetDlgItem(h, IDC_PARTKIND), on);
    EnableWindow(GetDlgItem(h, IDC_PARTDENS), on);
    EnableWindow(GetDlgItem(h, IDC_PARTSPEED), on);
    EnableWindow(GetDlgItem(h, IDC_PARTSIZE), on);
}

static INT_PTR CALLBACK particles_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            static const wchar_t *kinds[] = { L"Poeira", L"Bokeh", L"Faiscas", L"Estrelas" };
            for (int i = 0; i < 4; ++i)
                SendDlgItemMessageW(h, IDC_PARTKIND, CB_ADDSTRING, 0, (LPARAM)kinds[i]);
            SendDlgItemMessageW(h, IDC_PARTKIND, CB_SETCURSEL, g_work.particles_kind, 0);
            CheckDlgButton(h, IDC_PARTON, g_work.particles_on ? BST_CHECKED : BST_UNCHECKED);
            set_slider(h, IDC_PARTDENS, 0, 100, (int)(g_work.particles_density * 100.0f + 0.5f));
            set_slider(h, IDC_PARTSPEED, 0, 200, (int)(g_work.particles_speed * 100.0f + 0.5f));
            set_slider(h, IDC_PARTSIZE, 0, 200, (int)(g_work.particles_size_scale * 100.0f + 0.5f));
            particles_labels(h);
            particles_enable(h);
            return TRUE;
        }
        case WM_HSCROLL:
            g_work.particles_density    = (float)SendDlgItemMessageW(h, IDC_PARTDENS, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.particles_speed      = (float)SendDlgItemMessageW(h, IDC_PARTSPEED, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.particles_size_scale = (float)SendDlgItemMessageW(h, IDC_PARTSIZE, TBM_GETPOS, 0, 0) / 100.0f;
            particles_labels(h);
            preview_dirty(h);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDC_PARTON:
                    g_work.particles_on = (IsDlgButtonChecked(h, IDC_PARTON) == BST_CHECKED);
                    particles_enable(h);
                    preview_dirty(h);
                    break;
                case IDC_PARTKIND:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.particles_kind = (int)SendDlgItemMessageW(h, IDC_PARTKIND, CB_GETCURSEL, 0, 0);
                        preview_dirty(h);
                    }
                    break;
            }
            return TRUE;
    }
    return FALSE;
}
```

- [ ] **Step 3: Registrar a aba em `select_tab`**

Logo após `ShowWindow(g_bg, sel == 7 ? SW_SHOW : SW_HIDE);`:

```c
    ShowWindow(g_particles, sel == 8 ? SW_SHOW : SW_HIDE);
```

- [ ] **Step 4: Criar/posicionar em `dlg_proc` (`WM_INITDIALOG`)**

Logo após `TabCtrl_InsertItem(tabs, 7, &ti);`:

```c
            ti.pszText = L"Particulas";
            TabCtrl_InsertItem(tabs, 8, &ti);
```

Logo após `g_bg = CreateDialogW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_TAB_BG), h, bg_proc);`:

```c
            g_particles = CreateDialogW(GetModuleHandleW(NULL),
                                        MAKEINTRESOURCEW(IDD_TAB_PARTICLES), h, particles_proc);
```

Logo após `place_tab_child(h, tabs, g_bg);`:

```c
            place_tab_child(h, tabs, g_particles);
```

- [ ] **Step 5: Bump do clamp de `M3DT_TAB`**

Trocar `if (sel > 7) sel = 7;` por:

```c
                    if (sel > 8) sel = 8;
```

- [ ] **Step 6: Build de depuração + checagem de abertura da aba**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile debug
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

```powershell
$env:M3DT_SELFTEST = "1"
$env:M3DT_TAB = "8"
$env:M3DT_HOLD_MS = "4000"
$p = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p | Wait-Process -Timeout 8
Write-Output "exitcode: $($p.ExitCode)"
```

Esperado: `exitcode: 0`, sem crash.

- [ ] **Step 7: Commit**

```bash
git add src/config_dialog.c
git commit -m "feat: wire 'Particulas' tab into config dialog (9th tab, index 8)"
```

---

### Task 9: Verificação final

**Files:** nenhum (só execução/validação).

- [ ] **Step 1: Suite de testes completa**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```

- [ ] **Step 2: Capturar os 4 tipos via `M3DT_SHOT`**

```powershell
function Set-PartConfig($kind, $extra) {
    $key = "HKCU:\Software\Modern3DText"
    New-Item -Path $key -Force | Out-Null
    Set-ItemProperty -Path $key -Name "particles_on" -Value "1"
    Set-ItemProperty -Path $key -Name "particles_kind" -Value "$kind"
    Set-ItemProperty -Path $key -Name "particles_density" -Value "0.8"
    foreach ($k in $extra.Keys) { Set-ItemProperty -Path $key -Name $k -Value $extra[$k] }
}
function Shoot($outPath, $shotT) {
    $env:M3DT_SELFTEST = "1"
    $env:M3DT_SHOT = $outPath
    $env:M3DT_SHOT_T = "$shotT"
    $env:M3DT_HOLD_MS = "4000"
    $p = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
    $p | Wait-Process -Timeout 10
    Write-Output "$outPath exists: $(Test-Path $outPath)"
}

Set-PartConfig 0 @{}
Shoot "$env:TEMP\m3dt_part_dust.png" "3.0"

Set-PartConfig 1 @{}
Shoot "$env:TEMP\m3dt_part_bokeh.png" "3.0"

Set-PartConfig 2 @{ particles_speed = "1.5" }
Shoot "$env:TEMP\m3dt_part_sparks.png" "3.0"

Set-PartConfig 3 @{}
Shoot "$env:TEMP\m3dt_part_stars.png" "3.0"
```

Depois de cada captura, usar a ferramenta `Read` para abrir o PNG e
confirmar visualmente: poeira mostra pontos pequenos espalhados ao redor
do texto; bokeh mostra discos grandes com anel mais claro na borda;
faíscas mostram pontos quentes (laranja) saindo da superfície do texto;
estrelas mostram um campo mais denso e uniforme de pontos pequenos e
frios (azulados/brancos).

- [ ] **Step 3: Checagem real em 2 monitores (regressão do padrão de posse
  por-`SceneRenderer`)**

```powershell
Remove-Item Env:\M3DT_SELFTEST -ErrorAction SilentlyContinue
$env:M3DT_SHOT = "$env:TEMP\m3dt_part_mon1.png"
$env:M3DT_SHOT2 = "$env:TEMP\m3dt_part_mon2.png"
$env:M3DT_SHOT_T = "3.0"
$p = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/s" -PassThru
$p | Wait-Process -Timeout 10
Write-Output "mon1 exists: $(Test-Path $env:TEMP\m3dt_part_mon1.png)"
Write-Output "mon2 exists: $(Test-Path $env:TEMP\m3dt_part_mon2.png)"
```

(repetir até 3x se algum dos dois arquivos não existir — flakiness já
documentada de `WM_MOUSEMOVE` espúrio em runs reais não-selftest.)

- [ ] **Step 4: Restaurar o registro para os defaults**

```powershell
Remove-Item -Path "HKCU:\Software\Modern3DText" -Recurse -Force -ErrorAction SilentlyContinue
```

- [ ] **Step 5: Build release final**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile release
```

Esperado: `Built dist/Modern3DText.scr (N bytes)`, abaixo do limite de 3 MB
(atenção: `debug`/`release` compartilham os mesmos caminhos de `.o` neste
Makefile — sempre `clean` antes do build final de release, mesma lição da
Fase 5a).

- [ ] **Step 6: Commit final (rede de segurança, só se sobrou algo)**

```bash
git add -A
git status
```

---

## Self-Review (executado antes de apresentar o plano)

1. **Cobertura do spec/design**: §2 (dois grupos de comportamento) →
   Task 4/5 (spawn em espaço de mundo vs. espaço local+model); §3
   (arquitetura, CPU) → Tasks 3-6; §4 (config) → Task 1; §5 (os 4 tipos)
   → Task 4; §6 (interface) → Tasks 7-8; §7 (performance/ladder) → Task 2
   + Task 6; §8 (testes) → Task 9. §9 (fora de escopo) não gera tarefas,
   como esperado.
2. **Placeholders**: nenhum "TBD"/"depois" — todo trecho de código cola
   direto no arquivo indicado.
3. **Consistência de tipos**: `particles_kind`/`particles_on` como `int`
   batem entre `Config` (Task 1), `particles.c` (Task 4) e
   `config_dialog.c` (Task 8); `RenderQuality.particles` (Task 2) e
   `GlWindow.particles_on` (Task 6) são combinados exatamente como
   `bloom`/`streaks` já são (`frame_bloom`/`frame_particles` no mesmo
   padrão); a nova assinatura de `scene_render` (Task 5) é atualizada no
   único chamador externo (Task 6) na mesma tarefa lógica (commit
   combinado, já que uma não compila sem a outra); `m4_mul_point` (já
   existente em `util/mathx.h`) é reaproveitada tanto para posição quanto
   para normal em `update_sparks`, com o comentário explicando por que
   isso é seguro (o `model` de `scene.c` é rotação pura, sem
   translação/escala).

---

**Plano completo e salvo em `docs/superpowers/plans/2026-09-12-phase5b-particles.md`.**

Duas opções de execução:

1. **Subagent-Driven (recomendado)** — dispatco um subagente novo por
   task, com revisão entre elas e iteração rápida.
2. **Inline Execution** — executo as tasks nesta sessão via
   `executing-plans`, em lote com checkpoints para revisão.

Qual prefere?
