# Modern 3D Text — Fase 3b: Bevel e casca oca — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** As quinas do texto/forma deixam de ser vivas. Três modos de bevel: **sombreado** (padrão — inclina a normal perto da borda usando um SDF da forma, nunca quebra), **geométrico** (opt-in — offset real do contorno, pode falhar em cantos difíceis, o usuário compara no preview), e **desligado**. Mais um **micro-bevel** geométrico fixo (só no modo sombreado, pra suavizar a silhueta) e a opção de **casca oca** (forma vazada por dentro). Controles numa aba **Geometria** no diálogo.

**Architecture:** `sdf.c` gera, no build da malha, um campo de distância assinado 2D da forma (rasterização scanline por winding + 8SSEDT de Danielsson pra distância euclidiana, gradiente por diferenças centrais) — em unidades de em, resolução conforme `quality`. `contour_mesh` ganha os parâmetros de bevel: no modo `shading` produz tampa + paredes retas + micro-bevel de 1 anel e devolve a textura SDF junto; no modo `geometry` produz a faixa de bevel multi-segmento (offset por bissetriz, auto-interseção clampada); `shell` faz **um** offset pra dentro e fecha só o anel. O `model.vert` passa a coordenada local XY; o `model.frag`, nos fragmentos de tampa (`surfaceId` 0/1), amostra o SDF e — se `abs(dist) < bevelSize` — mistura a normal da face com a direção da borda (gradiente do SDF), sintetizando o chanfro na iluminação sem mexer na geometria. Cache: a malha (e o SDF) só reconstroem quando um parâmetro que os afeta muda.

**Tech Stack:** C11 · w64devkit · OpenGL 3.3 / GLSL 330 · `libtess2` (já vendorizado) · reuso de `config`/`scene`/`material`/`gl_core`/`contour_mesh`.

## Global Constraints

Do spec (§5.5, §5.6, §16, §17 item 3) e do estado pós-Fase 3a. Todo task herda esta seção.

- **Linguagem:** C11. Flags `-std=c11 -municode -Wall -Wextra` (+ `-O2 -DNDEBUG` release). **Sem `-ffast-math`.** Build **sem warnings**.
- **Toolchain:** só w64devkit. Sem downloads novos (tudo já vendorizado).
- **Registro:** só `HKCU\Software\Modern3DText` (testes: `_test`). Sem escrita fora disso e de `%LOCALAPPDATA%\Modern3DText\`.
- **`.scr` ≤ 3 MB.** i18n ainda não (Fase 8) — strings do diálogo em PT no `.rc`.
- **Padrão do bevel = `shading`** (spec §5.5). O modo `geometry` é opt-in; auto-interseção é **clampada** (não crasha), com log em nível debug. Silhueta continua reta no modo `shading` — aceito (spec §16 risco #2), micro-bevel suaviza.
- **Commits frequentes.** **TDD**: `sdf` (distância + gradiente de formas conhecidas), casos novos de `contour_mesh` (contagens/estanqueidade por modo). Shader/GL: PNG conferível (um por modo de bevel + shell on/off).
- **Atribuição:** todo commit termina com `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>`.

---

## File Structure

| Arquivo | Responsabilidade |
|---|---|
| `src/config.h` / `.c` | (+) `bevel_mode` (0 shading / 1 geometry / 2 off), `bevel_size`, `bevel_depth`, `bevel_segments` (2..8), `shell` (bool), `wall_thickness`, `quality` (0 baixa / 1 media / 2 alta) |
| `src/geometry/sdf.h` / `.c` | `Sdf` (dist + grad + mapeamento local↔texel); `sdf_build(const ContourSet*, int res, Sdf*)`; `sdf_free` |
| `src/geometry/contour_mesh.h` / `.c` | `MeshParams` cresce (bevel + shell + quality); `MeshData` carrega o `Sdf`; caps + paredes + micro-bevel (shading) / faixa multi-segmento (geometry) / anel (shell) |
| `src/gl_core.h` / `.c` | (+) `gl_texture_2d_rgba32f(w, h, const float *rgba)` — pro SDF (dist em R, grad em GB) |
| `shaders/model.vert` | (+) `out vec2 vLocalXY = aPos.xy` |
| `shaders/model.frag` | (+) amostra `uSdf` nas tampas; inclina a normal na faixa `abs(dist) < uBevelSize`; junção normal↔parede |
| `src/material.h` / `.c` | (+) uniforms `uSdf`/`uSdfMin`/`uSdfSize`/`uBevelMode`/`uBevelSize`; `material_set_bevel(...)` |
| `src/scene.c` | passa params de bevel ao `contour_mesh`; sobe/troca a textura SDF; alimenta os uniforms de bevel; cache inclui os params novos |
| `res/screensaver.rc`, `src/resource.h` | (+) `IDD_TAB_GEOMETRY` |
| `src/config_dialog.c` | (+) aba **Geometria** |
| `build/tests/test_sdf.c`, `test_contour_mesh.c`, `test_config.c` | testes |
| `build/Makefile` | `src/geometry/sdf.c` em `SRC_C` e nos testes |

---

## Task 1: Config — campos de geometria/bevel (TDD)

**Files:** `src/config.h`, `src/config.c`, `build/tests/test_config.c`

**Interfaces:** `Config` ganha
```c
int   bevel_mode;      /* 0 shading, 1 geometry, 2 off */
float bevel_size;      /* 0.0 .. 0.2 (em) */
float bevel_depth;     /* 0.0 .. 0.2 (em) */
int   bevel_segments;  /* 2 .. 8 (so no modo geometry) */
int   shell;           /* 0/1 */
float wall_thickness;  /* 0.01 .. 0.2 (em) */
int   quality;         /* 0 baixa, 1 media, 2 alta */
```
Padrões: `bevel_mode = 0`, `bevel_size = 0.05`, `bevel_depth = 0.04`, `bevel_segments = 4`, `shell = 0`, `wall_thickness = 0.06`, `quality = 1`. Chaves REG_SZ homônimas.

- [ ] **Step 1: `build/tests/test_config.c`** — no round-trip, setar/conferir os 7 campos; no bloco de clamp: `bevel_mode` "9" → 0; `bevel_segments` "1" → 2, "99" → 8; `quality` "5" → clamp 0..2; `bevel_size` "9" → ≤ 0.2.
- [ ] **Step 2: `src/config.h`** — adicionar os 7 campos após `env_path`.
- [ ] **Step 3: `src/config.c`**
  - `config_defaults`: os valores acima.
  - `config_load_from`: `reg_get_i` pra `bevel_mode`/`bevel_segments`/`shell`/`quality`; `reg_get_f` pra `bevel_size`/`bevel_depth`/`wall_thickness`. Clamps: `bevel_mode` 0..2, `bevel_segments` 2..8, `quality` 0..2, `shell` 0/1, floats via `clampf(_, 0.0f, 0.2f)` (o `wall_thickness` com piso 0.01).
  - `config_save_to`: `set_f` pros 7 (ints como float).
- [ ] **Step 4: Rodar — falha, depois `all tests passed`.**
- [ ] **Step 5: Commit**
```sh
git add src/config.h src/config.c build/tests/test_config.c
git commit -m "$(printf 'feat: config geometry fields (bevel mode/size/depth/segments, shell, quality)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 2: `sdf.c` — campo de distância assinado (8SSEDT) — TDD

**Files:** Create `src/geometry/sdf.h`, `src/geometry/sdf.c`, `build/tests/test_sdf.c`; modify `build/tests/test_main.c`, `build/Makefile`.

**Interfaces:**
```c
typedef struct {
    float *dist;   /* res*res, distancia assinada em unidades de em (< 0 dentro) */
    float *gx;     /* res*res, gradiente x normalizado (aponta pra fora) */
    float *gy;     /* res*res */
    int    res;
    float  min_x, min_y;   /* coord local (em) do centro do texel (0,0) */
    float  size_x, size_y; /* extensao local (em) coberta pela grade */
} Sdf;

int  sdf_build(const ContourSet *cs, int res, Sdf *out);   /* 0 = falha */
void sdf_free(Sdf *out);

/* auxiliar exposto pro teste: amostra bilinear em coord local (em) */
float sdf_sample(const Sdf *s, float lx, float ly);
```

**Algoritmo (`sdf_build`):**
1. Bbox da união dos contornos + margem = `2 * (size/res)` de cada lado. `size_x/size_y` = bbox+margem; `min_x/min_y` = canto − meio texel.
2. **Dentro/fora** (`unsigned char inside[res*res]`): scanline por linha — para cada `y` de texel, achar interseções das arestas de todos os contornos com a reta horizontal, ordenar, preencher spans pela regra `fillRule` do `ContourSet` (nonzero por padrão; usar winding acumulado).
3. **Seeds do 8SSEDT** (`float vx[res*res], vy[res*res]` = vetor até o pixel-feature mais próximo, ou `INF`): para cada texel, se algum dos 4 vizinhos tem `inside` diferente → é fronteira → `vx=vy=0`. Senão `INF`.
4. **8SSEDT** (Danielsson): passe pra frente (varre y crescente; por linha, x crescente depois x decrescente) e passe pra trás (y decrescente; idem), relaxando `v[p]` contra `v[vizinho] + offset` pelo menor `|v|²`. Máscara de 8 offsets padrão.
5. `d[p] = sqrt(vx²+vy²) * em_per_texel`. `dist[p] = inside[p] ? -d[p] : d[p]`.
6. **Gradiente**: diferenças centrais em `dist` → `(gx, gy)`, normalizado (fallback `(0,0)` se magnitude ~0).

**Testes (`test_sdf.c`):** contorno = **círculo** de raio R (polígono de 64 lados) centrado na origem.
- `sdf_sample(s, 0, 0)` ≈ `-R` (tolerância ~2 texels em em).
- `sdf_sample(s, R, 0)` ≈ 0.
- `sdf_sample(s, R*1.5f, 0)` ≈ `0.5*R` (fora).
- gradiente em `(R*0.9, 0)` aponta ≈ `(+1, 0)` (radial pra fora).
- **quadrado** com furo (2 contornos): ponto no meio do furo → `dist > 0` (o furo é "fora").
- `sdf_build` com contorno degenerado (2 pontos) → retorna 0, sem crash.

- [ ] **Step 1: `build/tests/test_sdf.c`** (falha primeiro) — conforme acima.
- [ ] **Step 2: `test_main.c`** — declarar e chamar `run_sdf_tests();`.
- [ ] **Step 3: `build/Makefile`** — `TEST_SRC += build/tests/test_sdf.c`; `TEST_UNITS += src/geometry/sdf.c`.
- [ ] **Step 4: Rodar — falha de link.**
- [ ] **Step 5: `src/geometry/sdf.h` + `sdf.c`** — implementar. `sdf.c` inclui só `geom_types.h`, `<stdlib.h>`, `<string.h>`, `<math.h>` (sem GL, sem windows.h → unidade pura testável).
- [ ] **Step 6: Rodar — `all tests passed`.**
- [ ] **Step 7: Commit**
```sh
git add src/geometry/sdf.h src/geometry/sdf.c build/tests/test_sdf.c build/tests/test_main.c build/Makefile
git commit -m "$(printf 'feat: sdf - signed distance field of a contour set via 8SSEDT\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 3: `contour_mesh` — modo `shading` (micro-bevel + SDF junto)

**Files:** `src/geometry/contour_mesh.h`, `src/geometry/contour_mesh.c`, `build/tests/test_contour_mesh.c`.

**Interfaces:**
```c
typedef struct {
    float depth;
    int   bevel_mode;      /* 0 shading, 1 geometry, 2 off */
    float bevel_size;
    float bevel_depth;
    int   bevel_segments;
    int   shell;
    float wall_thickness;
    int   quality;          /* 0/1/2 -> tolerancia de curva + res do SDF */
} MeshParams;

typedef struct {
    MeshVertex *verts; int nverts;
    unsigned   *idx;   int nidx;
    float minx, miny, minz, maxx, maxy, maxz;
    Sdf   sdf;         /* preenchido quando bevel_mode == 0; sdf.res == 0 caso contrario */
    int   has_sdf;
} MeshData;
```
`mesh_data_free` também chama `sdf_free(&m->sdf)`.

**Modo `shading` (bevel_mode == 0), esta task:**
- Tampa frente/verso + paredes retas (como hoje) — **sem** recuo por `bevel_depth` na tampa (o chanfro é só sombreado).
- **Micro-bevel geométrico**: 1 anel de quads entre a borda da tampa e o topo da parede, recuo fixo `mb = fminf(bevel_size, params.depth * 0.03f)` em Z e no plano (offset por bissetriz do contorno; `mb` é pequeno o bastante pra não auto-intersectar — se ainda assim intersectar num vértice, usa o ponto original). `surfaceId = 3`.
- `contour_mesh_build` chama `sdf_build(cs, sdf_res(quality), &out->sdf)` (res 384/640/1024) e seta `out->has_sdf = 1`.

**Testes:** re-rodar os casos existentes (quadrado / furo / L / degenerado) com `MeshParams{ .depth=0.3, .bevel_mode=0, .bevel_size=0.05, .quality=1 }`:
- estanqueidade das tampas/paredes mantida; micro-bevel adiciona ~`4*count` triângulos por contorno.
- `md.has_sdf == 1`, `md.sdf.res > 0`, `sdf_sample(&md.sdf, 0, 0) < 0` (dentro do quadrado).
- degenerado → `contour_mesh_build` retorna 0 e `md.has_sdf == 0`.

- [ ] **Step 1: `contour_mesh.h`** — novos campos de `MeshParams`/`MeshData`.
- [ ] **Step 2: `build/tests/test_contour_mesh.c`** — atualizar as chamadas (todas passam `MeshParams` completo) + as asserções de SDF. Rodar → falha de compilação (campos novos).
- [ ] **Step 3: `contour_mesh.c`** — `sdf_res(quality)`; adicionar o micro-bevel; chamar `sdf_build` no modo 0; `mesh_data_free` libera o SDF. `bevel_mode != 0` por ora: comportamento atual (sem micro-bevel, sem SDF) — os modos 1/2 vêm nas Tasks 4/5.
- [ ] **Step 4: Rodar — `all tests passed`.**
- [ ] **Step 5: `src/scene.c`** — `MeshParams` montado da `Config`; `rebuild_mesh` guarda `md.sdf` (move pra `SceneRenderer`, não liberar já) + sobe como textura via `gl_texture_2d_rgba32f` (RGBA: R=dist, G=gx, B=gy); `mesh_dirty` inclui `bevel_*`/`shell`/`wall_thickness`/`quality`. `scene_destroy` libera a textura + `sdf_free`.
- [ ] **Step 6: `gl_core`** — `gl_texture_2d_rgba32f(int w, int h, const float *rgba)` (sem mipmap, `GL_LINEAR`, `GL_CLAMP_TO_EDGE`). `sdf.c` produz 3 planos separados → `scene` entrelaça em `float[res*res*4]` antes de subir (ou `sdf.c` já entrelaça — decidir na implementação, entrelaçado é mais simples pro upload).
- [ ] **Step 7: Build + captura** — `bevel_mode=0`: a malha ainda renderiza (SDF ainda não usado no shader — Task 6). Sem `glError`.
- [ ] **Step 8: Commit**
```sh
git add src/geometry/contour_mesh.h src/geometry/contour_mesh.c build/tests/test_contour_mesh.c src/scene.c src/gl_core.h src/gl_core.c
git commit -m "$(printf 'feat: contour_mesh shading-bevel path - micro-bevel ring + SDF texture\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 4: `contour_mesh` — modo `geometry` (faixa de bevel multi-segmento)

**Files:** `src/geometry/contour_mesh.c`, `build/tests/test_contour_mesh.c`.

**Modo `geometry` (bevel_mode == 1):**
- Tampa recuada: frente em `z = +depth/2 - bevel_depth`, verso em `z = -depth/2 + bevel_depth`.
- **Faixa de bevel** por contorno: offset do contorno pra dentro por `bevel_size` (offset por bissetriz de ângulo por vértice: `p' = p + n_bis * bevel_size / sin(theta/2)`, com `n_bis` a bissetriz das normais das 2 arestas). `bevel_segments` anéis interpolando (posição e Z) da borda da tampa (`z` recuado, contorno original) até a borda offsetada (`z = ±depth/2`), perfil quarto-de-círculo (`cos`), normais suaves ao longo da faixa. Frente e verso. `surfaceId = 3`.
- **Auto-interseção**: se o vértice offsetado cruzar uma aresta vizinha (teste de orientação com os vizinhos) → usar o ponto de interseção (colapso local). `log` em debug: `"bevel geom: N vertices clampados"`.
- Sem SDF nesse modo (`has_sdf = 0`).

**Testes:** `MeshParams{ .bevel_mode=1, .bevel_size=0.15, .bevel_segments=4, .depth=0.4 }`:
- quadrado: contagem de triângulos = tampas + paredes + `2 * count * bevel_segments` da faixa.
- "L" côncavo com `bevel_size` grande: **não crasha**, produz malha (mesmo clampada), `nidx % 3 == 0`.
- `bevel_segments` fora de 2..8 → clampado dentro de `contour_mesh` (defensivo, além do clamp do config).

- [ ] **Step 1: teste (falha)** → **Step 2: implementação** → **Step 3: `all tests passed`.**
- [ ] **Step 4: Build + captura** `bevel_mode=1` num texto — o chanfro geométrico aparece (mesmo sem shader novo, a geometria já reflete a luz nas quinas).
- [ ] **Step 5: Commit**
```sh
git add src/geometry/contour_mesh.c build/tests/test_contour_mesh.c
git commit -m "$(printf 'feat: contour_mesh geometry-bevel mode (multi-segment offset band)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 5: `contour_mesh` — casca oca (shell)

**Files:** `src/geometry/contour_mesh.c`, `build/tests/test_contour_mesh.c`.

**`shell` (params.shell == 1), qualquer modo de bevel:**
- Em vez da tampa cheia, **um** offset de todos os contornos pra dentro por `wall_thickness` → contornos internos.
- Tampa = anel: tesselar (contornos externos + internos como furos) → só a faixa entre externo e interno.
- Paredes internas: quads ligando o anel interno da frente ao do verso, normal **pra dentro**, `surfaceId = 2`.
- Interior aberto nas pontas (visível em ângulo raso / profundidade curta) — é o efeito pretendido (spec §5.5).
- Se o offset interno auto-intersectar numa peça (traço fino) → **pular o shell nessa peça** (tampa cheia normal) + `log` debug.

**Testes:** `MeshParams{ .shell=1, .wall_thickness=0.3, .depth=0.4 }` num quadrado de lado 4:
- produz malha, `nidx % 3 == 0`, `nverts > 0`.
- quadrado de lado 0.4 com `wall_thickness=0.3` (offset > meia-largura) → shell pulado, tampa cheia, sem crash.
- "L" côncavo com shell → não crasha.

- [ ] **Step 1: teste (falha)** → **Step 2: implementação** → **Step 3: `all tests passed`.**
- [ ] **Step 4: Build + captura** `shell=1` — texto vazado.
- [ ] **Step 5: Commit**
```sh
git add src/geometry/contour_mesh.c build/tests/test_contour_mesh.c
git commit -m "$(printf 'feat: contour_mesh hollow shell (single inset, open ends)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 6: Shaders — bevel sombreado por SDF

**Files:** `shaders/model.vert`, `shaders/model.frag`, `src/material.h`, `src/material.c`, `src/scene.c`.

- [ ] **Step 1: `shaders/model.vert`** — adicionar:
```glsl
out vec2 vLocalXY;
...
vLocalXY = aPos.xy;
```

- [ ] **Step 2: `shaders/model.frag`** — novos uniforms + lógica no início de `main` (antes das ramificações de material):
```glsl
uniform sampler2D uSdf;      // R=dist(em), G=gx, B=gy
uniform vec2  uSdfMin;
uniform vec2  uSdfSize;
uniform int   uBevelMode;    // 0 shading, 1 geometry, 2 off
uniform float uBevelSize;
in vec2 vLocalXY;
...
    vec3 N = normalize(vNrm);
    if (!gl_FrontFacing) N = -N;

    // bevel sombreado: so nas tampas (surf 0/1), so quando ha SDF
    if (uBevelMode == 0 && uBevelSize > 1e-4 && (vSurf < 0.5 || (vSurf > 0.5 && vSurf < 1.5))) {
        vec2 uv = (vLocalXY - uSdfMin) / uSdfSize;
        vec3 s = texture(uSdf, uv).rgb;
        float dist = s.r;                       // < 0 dentro
        float band = clamp(-dist / uBevelSize, 0.0, 1.0);   // 0 na borda, 1 fundo
        if (band < 1.0) {
            vec2 g = s.gb;
            float gl = length(g);
            if (gl > 1e-4) {
                g /= gl;
                float zside = (vSurf < 0.5) ? 1.0 : -1.0;   // frente +Z, verso -Z
                vec3 edgeN = normalize(vec3(g, zside));       // ~45 graus pra fora
                float t = 1.0 - band;
                t = t * t * (3.0 - 2.0 * t);                  // smoothstep
                N = normalize(mix(vec3(0.0, 0.0, zside), edgeN, t));
                N = mat3(uModel) * N;                          // pra espaco de mundo? ver nota
            }
        }
    }
```
> **Nota (espaço da normal):** `vNrm` já vem em espaço de mundo (o `.vert` faz `mat3(uModel)*aNrm`). A normal de bevel é construída em espaço **local** (o SDF é local) → precisa de `mat3(uModel)` também. Como `uModel` é só rotação+escala uniforme aqui, `mat3(uModel)` serve. Passar `uModel` ao `.frag` (já é uniform no `.vert`; declarar no `.frag` também) OU passar a normal local como varying e transformar sempre no `.frag`. **Decisão:** o `.vert` passa `vNrmLocal` (sem transform) e `vNrmWorld` (com); o `.frag` escolhe e aplica `mat3(uModel)` uma vez no fim. Ajustar `model.vert` de acordo.

- Nas 4 ramificações de material, usar `N` (já possivelmente ajustado). A junção normal↔parede: quando `surfaceId == 2` e o fragmento está perto do topo da parede — **fora do escopo desta task** (o micro-bevel geométrico já cobre a transição visível); anotar como refino possível.

- [ ] **Step 3: `material.h` / `.c`** — `uSdf`(unidade 1), `uSdfMin`, `uSdfSize`, `uBevelMode`, `uBevelSize`, `uModel` no frag. `material_set_bevel(const Material*, int mode, float size, v2 sdf_min, v2 sdf_size, unsigned sdf_tex)`; bind da textura na unidade 1 (`GL_TEXTURE1`), `glUniform1i(uSdf, 1)` no init.

- [ ] **Step 4: `src/scene.c`** — `scene_render`: `material_set_bevel(&s->mat, s->bevel_mode, s->bevel_size, (v2){s->sdf.min_x, s->sdf.min_y}, (v2){s->sdf.size_x, s->sdf.size_y}, s->sdf_tex);` (só relevante quando `has_sdf`; quando não, passar `sdf_tex = 0` e o shader ignora por `uBevelMode`/`uBevelSize`).

- [ ] **Step 5: Build + captura comparativa** — mesmo texto, `bevel_mode` 0 / 1 / 2:
```sh
for B in 0 1 2; do ... set bevel_mode $B ... M3DT_SHOT=mat_b$B.png ... ; done
```
Abrir: **2** quinas vivas; **0** realce rolando na quina (chanfro sombreado), silhueta ainda reta; **1** chanfro geométrico real (silhueta arredondada). Sem `glError`.

- [ ] **Step 6: Testes** (regressão) + **Commit**
```sh
git add shaders/model.vert shaders/model.frag src/material.h src/material.c src/scene.c
git commit -m "$(printf 'feat: SDF-normal shading bevel in the model shader\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 7: Aba Geometria no diálogo

**Files:** `src/resource.h`, `res/screensaver.rc`, `src/config_dialog.c`.

**Aba Geometria (índice 3):** dropdown **Bevel** (Sombreado / Geometria / Desligado), sliders **Tamanho** (`bevel_size`) e **Profundidade do chanfro** (`bevel_depth`), slider **Segmentos** (`bevel_segments`, habilitado só no modo Geometria), checkbox **Casca oca** + slider **Espessura da parede** (`wall_thickness`, habilitado só com o checkbox), dropdown **Qualidade** (Baixa/Média/Alta). Mexer → `g_work` + `WM_APP+1`.

- [ ] **Step 1: `resource.h`** — `IDD_TAB_GEOMETRY 113` + `IDC_BEVELMODE/BSIZE/BSIZE_VAL/BDEPTH/BDEPTH_VAL/BSEG/BSEG_VAL/SHELL/WALL/WALL_VAL/QUALITY` (1400+).
- [ ] **Step 2: `res/screensaver.rc`** — `IDD_TAB_GEOMETRY DIALOGEX` com esses controles.
- [ ] **Step 3: `src/config_dialog.c`** — `geometry_proc` (mesmo padrão de `material_proc`): combos populados, sliders mapeados (`bevel_size`/`bevel_depth`/`wall_thickness` ×1000 em faixa 0..200; `bevel_segments` 2..8; `quality` combo), `EnableWindow` no slider de segmentos conforme o modo e no de espessura conforme o checkbox. Registrar `g_geometry`, criar no `WM_INITDIALOG`, `TabCtrl_InsertItem` índice 3, alternar no `TCN_SELCHANGE` (4 abas agora).
- [ ] **Step 4: Build + selftest headless** (`/c` abre com 4 abas, fecha, `exit=0`, sem `glError`).
- [ ] **Step 5: Verificação manual** — `/c` aba Geometria: trocar o modo → preview muda; sliders → preview responde; casca oca → texto vaza; conferir em `regedit`.
- [ ] **Step 6: Testes** + **Commit**
```sh
git add src/resource.h res/screensaver.rc src/config_dialog.c
git commit -m "$(printf 'feat: config dialog Geometry tab (bevel mode/size/segments, shell, quality)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 8: Integração, QA da Fase 3b, tag

**Files:** `docs/qa-checklist-phase-3b.md`, `README.md`, `res/screensaver.rc` (versão → 0.3.1).

- [ ] **Step 1:** `make` / `make debug` / `make test` verdes; `.scr` < 3 MB. Medir o tempo de `scene_create` no log (o SDF adiciona custo no load — deve ficar < ~100 ms na qualidade Média).
- [ ] **Step 2:** Capturas pra `docs/img/`: `phase3b-bevel-{shading,geometry,off}.png` e `phase3b-shell.png` (texto "ProArt"). Conferir cada uma.
- [ ] **Step 3: `docs/qa-checklist-phase-3b.md`** — headless (build/test, sdf, contour_mesh por modo, 4 capturas, tempo de load) + interativo (aba Geometria: cada controle → preview; comparar bevel sombreado × geométrico no *seu* texto pra ver qual quebra menos; casca oca em tela cheia; DPI).
- [ ] **Step 4:** `README.md` status → "Fase 3b — bevel (sombreado por SDF / geométrico / desligado) + micro-bevel + casca oca; aba Geometria".
- [ ] **Step 5: Rodar o checklist interativo.**
- [ ] **Step 6: Commit + tag**
```sh
git add docs/qa-checklist-phase-3b.md docs/img/phase3b-*.png README.md res/screensaver.rc
git commit -m "$(printf 'feat: phase 3b - bevel and hollow shell\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
git tag -a v0.3.1-phase3b -m "Fase 3b: bevel (SDF/geometria/off) + micro-bevel + casca oca"
```

---

## Self-Review

**1. Cobertura (spec §17 item 3 + §5.5):**
- `sdf` (8SSEDT) → Task 2. ✓
- bevel `shading` (SDF-normal) + micro-bevel → Tasks 3 (geometria/SDF) + 6 (shader). ✓
- bevel `geometry` (offset multi-segmento, clamp de auto-interseção) → Task 4. ✓
- bevel `off` → suportado desde a Fase 2a (tampa+paredes), exposto no config (Task 1) e diálogo (Task 7). ✓
- casca oca (`shell`) → Task 5. ✓
- cache de geometria (§5.6) → Task 3 Step 5 (o `mesh_dirty` do `scene_set_config` ganha os params de bevel/shell/quality; SDF só reconstrói junto com a malha). ✓
- `quality` → tolerância de curva já existe (`SC_FLATTEN` vira função de quality) + resolução do SDF (Task 3). ✓
- Controles no diálogo (§10.3, aba de geometria) → Task 7. ✓
- Silhueta reta no modo `shading` + micro-bevel (§16 risco #2) → Tasks 3/6, anotado. ✓
- Auto-interseção clampada no modo `geometry` (§16 risco #1) → Task 4, com log. ✓
- **Fora da 3b:** junção fina normal↔parede no shader (refino, o micro-bevel cobre o visível); pós-processamento; SVG/mesh; i18n; presets.

**2. Placeholders:** sem "TODO"/"TBD". O modo do shader que ainda não existe (SDF no `.frag`) é criado no Task 6 — a Task 3 sobe a textura mas o shader só usa depois, e isso está explícito nos dois tasks. A "Nota (espaço da normal)" no Task 6 Step 2 resolve a ambiguidade inline (decisão: `.vert` passa normal local e mundial). Testes produzem PNGs conferíveis (4).

**3. Consistência de tipos:**
- `Config` campos novos (`bevel_mode` int, `bevel_size`/`bevel_depth`/`wall_thickness` float, `bevel_segments`/`shell`/`quality` int) — Task 1; lidos por `scene`/`contour_mesh` (Tasks 3–5), escritos por `geometry_proc` (Task 7). Faixas do config batem com os clamps defensivos de `contour_mesh`. ✓
- `Sdf` — `sdf.h` (Task 2), embutido em `MeshData` (Task 3), copiado pra `SceneRenderer` e subido como textura (Task 3 Step 5), amostrado no shader via `uSdfMin`/`uSdfSize` (Task 6). `sdf_free` chamado por `mesh_data_free` e `scene_destroy`. ✓
- `MeshParams` cresce no Task 3 (7 campos novos); **todas** as chamadas de `contour_mesh_build` (scene.c + os 4 blocos de teste) passam a montar o struct completo — Task 3 Step 2 atualiza os testes, Task 3 Step 5 atualiza `scene.c`. ✓
- `MeshData.sdf` / `.has_sdf` — Task 3; `scene.c` checa `has_sdf` antes de subir a textura. ✓
- `gl_texture_2d_rgba32f(int,int,const float*)` — `gl_core.h` (Task 3 Step 6), usado por `scene.c` (Task 3 Step 5). Complementa o `gl_texture_2d_rgb8` da Fase 3a. ✓
- `material_set_bevel(const Material*, int, float, v2, v2, unsigned)` — `material.h` (Task 6), chamado em `scene_render` (Task 6 Step 4). Uniforms batem com `model.frag` (Task 6 Step 2). `v2` é o tipo de `mathx.h` (já existe). ✓
- IDs `IDD_TAB_GEOMETRY`/`IDC_BEVEL*` — `resource.h` (Task 7), `.rc` + `config_dialog.c` (Task 7). 4ª aba, índice 3; `g_content`/`g_motion`/`g_material`/`g_geometry` alternados no `TCN_SELCHANGE`. ✓
- `vLocalXY` / `vNrmLocal` — `model.vert` out, `model.frag` in (Task 6). ✓

Sem inconsistências.

---

## Execution Handoff

**Plano completo e salvo em `docs/superpowers/plans/2026-09-10-modern-3d-text-phase-3b-bevel-shell.md`. Duas opções de execução:**

**1. Subagent-Driven (recomendado)** — subagente novo por task, revisão entre tasks.
**2. Inline Execution** — nesta sessão, com checkpoints.

**Qual abordagem?**

> Como nas fases anteriores: build/headless aqui, com PNGs comparativos (bevel sombreado × geométrico × desligado, e casca oca) pra eu conferir. O QA interativo da aba Geometria — e principalmente **comparar os dois modos de bevel no seu texto real** pra ver qual quebra menos — fica pra você. Sem downloads novos.
