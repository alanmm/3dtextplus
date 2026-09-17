# Material Wireframe Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a 4th material mode ("Wireframe") that draws only the mesh's real edges (silhouette + creases) as constant-pixel-width 3D ribbons, with configurable color (reused `Cor:`), emissive, line thickness, and an X-ray toggle (occluded lines at 60% opacity).

**Architecture:** A pure geometry-analysis module (`feature_edges.c`) filters the final triangulated mesh down to "real" edges via a dihedral-angle threshold, independent of content mode. `scene.c` uploads that filtered edge list as a `GL_LINES` VBO and renders it with a new geometry-shader program that expands each line into a screen-space-constant-width quad. Occlusion reuses the main HDR target's own depth buffer (no extra FBO); X-ray additionally renders a single-sample depth-texture copy of the solid mesh so the line fragment shader can sample (not just test) depth to decide per-fragment opacity.

**Tech Stack:** C11, OpenGL 3.3 core (first use of a geometry shader stage in this project), Win32 dialog UI.

## Global Constraints

- OpenGL 3.3 core floor (project-wide) - geometry shaders are core since GL 3.2, so this fits.
- `GL_CULL_FACE` stays globally disabled (`scene_create`) - never re-enable it for this feature.
- Every Material-tab UI block gets its own unique `.rc` position - never share coordinates between two blocks (real bug from earlier this session, see comment above `g_mat_blocks` in `src/config_dialog.c`).
- Every new `Config` field needs all 6 touch points: `config_defaults`, `config_load_from`, sanitize block, `config_save_to`, `preset_scope_copy`, `preset_dump_fields`.
- After editing any `.h` file, run `mingw32-make -f build/Makefile clean` before rebuilding (no header-dependency tracking in this Makefile).
- Spec: `docs/superpowers/specs/2026-09-17-wireframe-material-design.md`.

---

### Task 1: Feature-edge detection (`feature_edges.c`/`.h`)

**Files:**
- Create: `src/geometry/feature_edges.h`
- Create: `src/geometry/feature_edges.c`
- Test: `build/tests/test_feature_edges.c`
- Modify: `build/tests/test_main.c`
- Modify: `build/Makefile`

**Interfaces:**
- Produces: `typedef struct { v3 a, b; } WireEdge;` and
  `int feature_edges_build(const MeshData *md, float crease_deg, WireEdge **out, int *out_count);`
  - Returns 0 (and leaves `*out`/`*out_count` untouched) if `md` has no triangles or no edge passed the filter.
  - On success, `*out` is `malloc`'d (caller `free()`s it) and holds `*out_count` edges as pairs of world-space positions (whatever space `md`'s positions are already in - this function does no transforms).

- [ ] **Step 1: Write the failing test**

Create `build/tests/test_feature_edges.c`:

```c
#include "test.h"
#include "geometry/feature_edges.h"
#include <stdlib.h>
#include <string.h>

/* cubo unitario: 8 vertices, 6 faces * 2 triangulos = 12 triangulos.
   Cada face e' um quad plano dividido em (q0,q1,q2)+(q0,q2,q3) - as 2
   metades de uma MESMA face compartilham normal identica (0 graus,
   nao deveria virar aresta "de verdade"); faces vizinhas fazem 90
   graus entre si (arestas reais do cubo). */
static const float CUBE_POS[8][3] = {
    {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},
    {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1},
};
static const unsigned CUBE_QUADS[6][4] = {
    {0,1,2,3}, {4,5,6,7}, {0,1,5,4}, {2,3,7,6}, {1,2,6,5}, {0,3,7,4},
};

static void build_cube(MeshData *md)
{
    memset(md, 0, sizeof *md);
    md->nverts = 8;
    md->verts = (MeshVertex *)calloc(8, sizeof(MeshVertex));
    for (int i = 0; i < 8; ++i) {
        md->verts[i].px = CUBE_POS[i][0];
        md->verts[i].py = CUBE_POS[i][1];
        md->verts[i].pz = CUBE_POS[i][2];
    }
    md->nidx = 6 * 2 * 3;
    md->idx = (unsigned *)malloc((size_t)md->nidx * sizeof(unsigned));
    int w = 0;
    for (int f = 0; f < 6; ++f) {
        unsigned q0 = CUBE_QUADS[f][0], q1 = CUBE_QUADS[f][1];
        unsigned q2 = CUBE_QUADS[f][2], q3 = CUBE_QUADS[f][3];
        md->idx[w++] = q0; md->idx[w++] = q1; md->idx[w++] = q2;
        md->idx[w++] = q0; md->idx[w++] = q2; md->idx[w++] = q3;
    }
    md->minx = md->miny = md->minz = -1;
    md->maxx = md->maxy = md->maxz = 1;
}

void run_feature_edges_tests(void)
{
    MeshData cube;
    build_cube(&cube);

    WireEdge *edges = NULL;
    int count = 0;
    int ok = feature_edges_build(&cube, 35.0f, &edges, &count);
    EXPECT(ok);
    /* as 12 arestas reais do cubo entram; as 6 diagonais internas
       (0 grau entre as 2 metades de cada face) NAO entram */
    EXPECT_EQ_INT(count, 12);
    free(edges);
    free(cube.verts);
    free(cube.idx);

    /* um unico triangulo solto: as 3 arestas sao bordas abertas (1 so
       vizinho cada) - todas devem entrar, mesmo sem nenhum vinco de
       verdade pra medir */
    MeshData tri;
    memset(&tri, 0, sizeof tri);
    tri.nverts = 3;
    tri.verts = (MeshVertex *)calloc(3, sizeof(MeshVertex));
    tri.verts[0].px = 0; tri.verts[0].py = 0; tri.verts[0].pz = 0;
    tri.verts[1].px = 1; tri.verts[1].py = 0; tri.verts[1].pz = 0;
    tri.verts[2].px = 0; tri.verts[2].py = 1; tri.verts[2].pz = 0;
    tri.nidx = 3;
    tri.idx = (unsigned *)malloc(3 * sizeof(unsigned));
    tri.idx[0] = 0; tri.idx[1] = 1; tri.idx[2] = 2;

    WireEdge *tedges = NULL;
    int tcount = 0;
    ok = feature_edges_build(&tri, 35.0f, &tedges, &tcount);
    EXPECT(ok);
    EXPECT_EQ_INT(tcount, 3);
    free(tedges);
    free(tri.verts);
    free(tri.idx);

    /* malha vazia (0 triangulos) - retorna 0, nao crasha */
    MeshData empty;
    memset(&empty, 0, sizeof empty);
    WireEdge *eedges = NULL;
    int ecount = -1;
    ok = feature_edges_build(&empty, 35.0f, &eedges, &ecount);
    EXPECT(!ok);
}
```

Add to `build/tests/test_main.c`: a `void run_feature_edges_tests(void);` declaration (alphabetically near `run_font_outline_tests`) and a `run_feature_edges_tests();` call in `main()` (same relative position).

Add to `build/Makefile`:
- `TEST_SRC` (around line 104-106): append `build/tests/test_feature_edges.c`.
- `TEST_UNITS` (around line 107-110): append `src/geometry/feature_edges.c`.
- `SRC_C` (around line 39): append `src/geometry/feature_edges.c` right after `src/geometry/contour_mesh.c`.

- [ ] **Step 2: Run test to verify it fails**

```bash
mingw32-make -f build/Makefile test
```
Expected: FAIL to compile (`feature_edges.h`/`.c` don't exist yet).

- [ ] **Step 3: Write minimal implementation**

Create `src/geometry/feature_edges.h`:

```c
#ifndef M3DT_FEATURE_EDGES_H
#define M3DT_FEATURE_EDGES_H
#include "geometry/contour_mesh.h"
#include "util/mathx.h"

typedef struct { v3 a, b; } WireEdge;

/* Extrai as arestas "reais" de md: bordas abertas da malha (1
   triangulo vizinho) e vincos (2 triangulos vizinhos cujas normais
   geometricas planas diferem mais que crease_deg). Aloca *out via
   malloc - quem chama libera com free(). Retorna 0 se md nao tiver
   triangulos, ou se nenhuma aresta passar no filtro (nesse caso
   *out/*out_count ficam intocados). */
int feature_edges_build(const MeshData *md, float crease_deg,
                         WireEdge **out, int *out_count);

#endif
```

Create `src/geometry/feature_edges.c`:

```c
#include "geometry/feature_edges.h"
#include <stdlib.h>
#include <math.h>

typedef struct {
    unsigned lo, hi;       /* indices do par, lo < hi; neighbor_count==0 = slot livre */
    v3  n1;                /* normal do 1o triangulo que tocou essa aresta */
    int neighbor_count;
    int is_feature;
} EdgeEntry;

typedef struct { EdgeEntry *slots; int cap; } EdgeTable;

static unsigned hash_pair(unsigned lo, unsigned hi)
{
    return lo * 2654435761u ^ hi * 40503u;
}

static v3 v3_sub(v3 a, v3 b) { return (v3){ a.x - b.x, a.y - b.y, a.z - b.z }; }
static v3 v3_cross(v3 a, v3 b)
{
    return (v3){ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
static float v3_len2(v3 a) { return a.x * a.x + a.y * a.y + a.z * a.z; }
static v3 v3_norm(v3 a)
{
    float l = sqrtf(v3_len2(a));
    if (l < 1e-9f) return (v3){ 0, 0, 0 };
    return (v3){ a.x / l, a.y / l, a.z / l };
}
static float v3_dot(v3 a, v3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

/* acha o slot da aresta (i0,i1) na tabela, criando um vazio se for a
   1a vez que essa aresta aparece. Enderecamento aberto, sondagem
   linear - a capacidade (2x o maximo teorico de arestas distintas)
   mantem a tabela esparsa o bastante pra nunca precisar de rehash. */
static EdgeEntry *edge_find_or_insert(EdgeTable *t, unsigned i0, unsigned i1)
{
    unsigned lo = i0 < i1 ? i0 : i1;
    unsigned hi = i0 < i1 ? i1 : i0;
    unsigned idx = hash_pair(lo, hi) % (unsigned)t->cap;
    for (int probe = 0; probe < t->cap; ++probe) {
        EdgeEntry *e = &t->slots[idx];
        if (e->neighbor_count == 0) { e->lo = lo; e->hi = hi; return e; }
        if (e->lo == lo && e->hi == hi) return e;
        idx = (idx + 1) % (unsigned)t->cap;
    }
    return NULL;   /* tabela cheia - nao deveria acontecer com a capacidade abaixo */
}

int feature_edges_build(const MeshData *md, float crease_deg,
                         WireEdge **out, int *out_count)
{
    int ntri = md->nidx / 3;
    if (ntri <= 0) return 0;

    EdgeTable table;
    table.cap = ntri * 6 + 17;   /* ate' 3*ntri arestas distintas - folga generosa */
    table.slots = (EdgeEntry *)calloc((size_t)table.cap, sizeof(EdgeEntry));
    if (!table.slots) return 0;

    float crease_cos = cosf(crease_deg * (3.14159265f / 180.0f));

    for (int t = 0; t < ntri; ++t) {
        unsigned i0 = md->idx[t * 3 + 0];
        unsigned i1 = md->idx[t * 3 + 1];
        unsigned i2 = md->idx[t * 3 + 2];
        v3 p0 = { md->verts[i0].px, md->verts[i0].py, md->verts[i0].pz };
        v3 p1 = { md->verts[i1].px, md->verts[i1].py, md->verts[i1].pz };
        v3 p2 = { md->verts[i2].px, md->verts[i2].py, md->verts[i2].pz };
        v3 faceN = v3_norm(v3_cross(v3_sub(p1, p0), v3_sub(p2, p0)));

        unsigned edges[3][2] = { { i0, i1 }, { i1, i2 }, { i2, i0 } };
        for (int e = 0; e < 3; ++e) {
            EdgeEntry *entry = edge_find_or_insert(&table, edges[e][0], edges[e][1]);
            if (!entry) continue;   /* tabela cheia (nao deveria acontecer) */
            if (entry->neighbor_count == 0) {
                entry->n1 = faceN;
                entry->neighbor_count = 1;
                entry->is_feature = 1;   /* borda aberta ate' aparecer um 2o vizinho */
            } else if (entry->neighbor_count == 1) {
                entry->is_feature = (v3_dot(entry->n1, faceN) < crease_cos);
                entry->neighbor_count = 2;
            } else {
                /* 3o+ triangulo na mesma aresta - malha nao-manifold,
                   caso degenerado. Mais seguro mostrar do que esconder. */
                entry->is_feature = 1;
                entry->neighbor_count++;
            }
        }
    }

    int count = 0;
    for (int i = 0; i < table.cap; ++i)
        if (table.slots[i].neighbor_count > 0 && table.slots[i].is_feature) count++;

    if (count == 0) { free(table.slots); return 0; }

    WireEdge *edges_out = (WireEdge *)malloc((size_t)count * sizeof(WireEdge));
    if (!edges_out) { free(table.slots); return 0; }

    int w = 0;
    for (int i = 0; i < table.cap; ++i) {
        EdgeEntry *e = &table.slots[i];
        if (e->neighbor_count == 0 || !e->is_feature) continue;
        const MeshVertex *va = &md->verts[e->lo];
        const MeshVertex *vb = &md->verts[e->hi];
        edges_out[w].a = (v3){ va->px, va->py, va->pz };
        edges_out[w].b = (v3){ vb->px, vb->py, vb->pz };
        w++;
    }

    free(table.slots);
    *out = edges_out;
    *out_count = count;
    return 1;
}
```

- [ ] **Step 4: Run test to verify it passes**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```
Expected: `all tests passed`.

- [ ] **Step 5: Commit**

```bash
git add src/geometry/feature_edges.h src/geometry/feature_edges.c \
        build/tests/test_feature_edges.c build/tests/test_main.c build/Makefile
git commit -m "feat: add feature-edge detection for the Wireframe material"
```

---

### Task 2: Geometry-shader-capable program compilation (`gl_core.c`/`.h`)

**Files:**
- Modify: `src/gl_core.h`
- Modify: `src/gl_core.c`

**Interfaces:**
- Consumes: nothing new.
- Produces: `unsigned gl_program_gs(const char *vs_src, const char *gs_src, const char *fs_src);`
  (`gs_src` may be `NULL` for a vertex+fragment-only program). `gl_program()`'s existing signature and
  behavior are unchanged - it becomes a thin wrapper.

This module has no automated test (matches every other `gl_core.c` function - GL-context-dependent,
can't run in the plain-C test binary). Verified by Task 4's build.

- [ ] **Step 1: Extend `compile()` to name geometry-shader errors correctly**

In `src/gl_core.c`, replace the `compile()` function:

```c
static unsigned compile(GLenum type, const char *src)
{
    unsigned s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    int ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[2048];
        glGetShaderInfoLog(s, sizeof buf, NULL, buf);
        const char *kind = type == GL_VERTEX_SHADER ? "vert" :
                            type == GL_GEOMETRY_SHADER ? "geom" : "frag";
        log_errorf("shader %s: %s", kind, buf);
        glDeleteShader(s);
        return 0;
    }
    return s;
}
```

- [ ] **Step 2: Add `gl_program_gs()`, make `gl_program()` delegate to it**

Replace `gl_program()` in `src/gl_core.c` with:

```c
unsigned gl_program(const char *vs_src, const char *fs_src)
{
    return gl_program_gs(vs_src, NULL, fs_src);
}

unsigned gl_program_gs(const char *vs_src, const char *gs_src, const char *fs_src)
{
    unsigned vs = compile(GL_VERTEX_SHADER, vs_src);
    unsigned gs = gs_src ? compile(GL_GEOMETRY_SHADER, gs_src) : 0;
    unsigned fs = compile(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || (gs_src && !gs) || !fs) {
        if (vs) glDeleteShader(vs);
        if (gs) glDeleteShader(gs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    unsigned p = glCreateProgram();
    glAttachShader(p, vs);
    if (gs) glAttachShader(p, gs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    if (gs) glDeleteShader(gs);
    glDeleteShader(fs);

    int ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[2048];
        glGetProgramInfoLog(p, sizeof buf, NULL, buf);
        log_errorf("link: %s", buf);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}
```

In `src/gl_core.h`, add right after the existing `unsigned gl_program(const char *vs_src, const char *fs_src);` line:

```c
unsigned gl_program_gs(const char *vs_src, const char *gs_src, const char *fs_src);
```

- [ ] **Step 3: Build to verify it compiles**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```
Expected: `all tests passed` (this module isn't exercised by tests, but must not break the existing build - `gl_core.c` isn't in `TEST_UNITS`, so `test` just needs to still pass unaffected; run `mingw32-make -f build/Makefile release` too to confirm the full binary still links).

- [ ] **Step 4: Commit**

```bash
git add src/gl_core.h src/gl_core.c
git commit -m "feat: add gl_program_gs() for geometry-shader-capable programs"
```

---

### Task 3: `Config` schema - `wireframe_thickness`/`wireframe_xray`

**Files:**
- Modify: `src/config.h`
- Modify: `src/config.c`
- Modify: `src/presets.c`
- Modify: `build/tests/test_config.c`

**Interfaces:**
- Produces: `Config.wireframe_thickness` (float, px, 1..6, default 2.0) and
  `Config.wireframe_xray` (int, 0/1, default 0), fully round-tripped through
  registry load/save and presets.

- [ ] **Step 1: Write the failing test**

In `build/tests/test_config.c`, add to the defaults test (near the existing `EXPECT(nearf(d.edge_bias, 0.5f));` around line 33):

```c
EXPECT(nearf(d.wireframe_thickness, 2.0f));
EXPECT(d.wireframe_xray == 0);
```

Add to the roundtrip test's setup (near `a.edge_bias = 0.72f;` around line 107):

```c
a.wireframe_thickness = 4.5f;
a.wireframe_xray = 1;
```

Add to the roundtrip test's assertions (near `EXPECT(nearf(b.edge_bias, 0.72f));` around line 176):

```c
EXPECT(nearf(b.wireframe_thickness, 4.5f));
EXPECT(b.wireframe_xray == 1);
```

- [ ] **Step 2: Run test to verify it fails**

```bash
mingw32-make -f build/Makefile test
```
Expected: FAIL to compile (`Config` has no `wireframe_thickness`/`wireframe_xray` fields yet).

- [ ] **Step 3: Add the fields and wire all 6 touch points**

In `src/config.h`, add right after the `edge_bias` field:

```c
    float        wireframe_thickness; /* 1..6 (px de tela) - so' wireframe */
    int          wireframe_xray;      /* 0/1 - so' wireframe */
```

In `src/config.c`:

`config_defaults()` - add right after `c->edge_bias = 0.5f;`:
```c
    c->wireframe_thickness = 2.0f;
    c->wireframe_xray = 0;
```

`config_load_from()` - add right after the `if (reg_get_f(k, L"edge_bias", &f)) c->edge_bias = f;` line:
```c
    if (reg_get_f(k, L"wireframe_thickness", &f)) c->wireframe_thickness = f;
    reg_get_i(k, L"wireframe_xray", &c->wireframe_xray);
```

Sanitize block - add right after `c->edge_bias = clampf(c->edge_bias, 0.0f, 1.0f);`:
```c
    c->wireframe_thickness = clampf(c->wireframe_thickness, 1.0f, 6.0f);
    if (c->wireframe_xray != 0 && c->wireframe_xray != 1) c->wireframe_xray = 0;
```

`config_save_to()` - add right after `set_f(k, L"edge_bias", c->edge_bias);`:
```c
    set_f(k, L"wireframe_thickness", c->wireframe_thickness);
    set_f(k, L"wireframe_xray", (float)c->wireframe_xray);
```

In `src/presets.c`:

`preset_scope_copy()` - add right after `dst->edge_bias = src->edge_bias;`:
```c
    dst->wireframe_thickness = src->wireframe_thickness;
    dst->wireframe_xray = src->wireframe_xray;
```

`preset_dump_fields()` - add right after `fwprintf(f, L"edge_bias=%.5f\r\n", (double)from->edge_bias);`:
```c
    fwprintf(f, L"wireframe_thickness=%.5f\r\n", (double)from->wireframe_thickness);
    fwprintf(f, L"wireframe_xray=%d\r\n", from->wireframe_xray);
```

- [ ] **Step 4: Run test to verify it passes**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```
Expected: `all tests passed`.

- [ ] **Step 5: Commit**

```bash
git add src/config.h src/config.c src/presets.c build/tests/test_config.c
git commit -m "feat: add wireframe_thickness/wireframe_xray to Config"
```

---

### Task 4: Wireframe shaders

**Files:**
- Create: `shaders/wireframe.vert`
- Create: `shaders/wireframe.geom`
- Create: `shaders/wireframe.frag`
- Modify: `build/Makefile`

**Interfaces:**
- Produces: three embeddable shader sources, giving `EMBED_wireframe_vert`,
  `EMBED_wireframe_geom`, `EMBED_wireframe_frag` (and their `_len` companions) in
  `generated/embedded.h` once built. Consumed by Task 5's `gl_program_gs()` call.

No test possible for shader source text itself beyond "the build compiles/links it" (Task 5/6 cover
that). This task's own verification is just that `embed.exe` picks the files up without error.

- [ ] **Step 1: Write `shaders/wireframe.vert`**

```glsl
#version 330 core
layout(location = 0) in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

void main()
{
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
}
```

- [ ] **Step 2: Write `shaders/wireframe.geom`**

```glsl
#version 330 core
layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

uniform vec2  uViewportSize;   // pixels
uniform float uThicknessPx;

void main()
{
    vec4 p0 = gl_in[0].gl_Position;
    vec4 p1 = gl_in[1].gl_Position;

    vec2 ndc0 = p0.xy / p0.w;
    vec2 ndc1 = p1.xy / p1.w;

    // trabalha em espaco de pixel de tela pra garantir espessura
    // uniforme mesmo com viewport nao-quadrado (um "perpendicular" em
    // NDC puro sai distorcido pela razao de aspecto).
    vec2 halfViewport = uViewportSize * 0.5;
    vec2 screen0 = ndc0 * halfViewport;
    vec2 screen1 = ndc1 * halfViewport;

    vec2 dir = screen1 - screen0;
    if (length(dir) < 1e-6) return;   // segmento sem extensao em tela
    dir = normalize(dir);
    vec2 normalPx = vec2(-dir.y, dir.x) * (uThicknessPx * 0.5);
    vec2 offsetNdc = normalPx / halfViewport;

    gl_Position = vec4((ndc0 + offsetNdc) * p0.w, p0.z, p0.w); EmitVertex();
    gl_Position = vec4((ndc0 - offsetNdc) * p0.w, p0.z, p0.w); EmitVertex();
    gl_Position = vec4((ndc1 + offsetNdc) * p1.w, p1.z, p1.w); EmitVertex();
    gl_Position = vec4((ndc1 - offsetNdc) * p1.w, p1.z, p1.w); EmitVertex();
    EndPrimitive();
}
```

- [ ] **Step 3: Write `shaders/wireframe.frag`**

```glsl
#version 330 core
out vec4 fragColor;

uniform vec3  uLineColor;
uniform vec3  uEmissiveColor;
uniform float uEmissiveAmount;
uniform int   uXray;
uniform sampler2D uSolidDepth;   // so' amostrado quando uXray == 1
uniform vec2  uViewportSize;

void main()
{
    vec3 col = uLineColor + uEmissiveColor * uEmissiveAmount;

    if (uXray == 1) {
        vec2 uv = gl_FragCoord.xy / uViewportSize;
        float solidDepth = texture(uSolidDepth, uv).r;
        // gl_FragCoord.z e' a profundidade desta linha na mesma
        // convencao [0,1] de uma textura GL_DEPTH_COMPONENT - se ela
        // esta' na frente/na superficie da malha solida (ou a malha
        // solida nao escreveu nada ali, profundidade==1 = "infinito"),
        // fica opaca; senao (atras, oculta) fica em 60% - efeito
        // "fantasma" do Blender pedido pelo usuario.
        float alpha = (gl_FragCoord.z <= solidDepth + 1e-5) ? 1.0 : 0.6;
        fragColor = vec4(col, alpha);
    } else {
        fragColor = vec4(col, 1.0);
    }
}
```

- [ ] **Step 4: Register the new shaders with the embed tool**

In `build/Makefile`, extend `EMBED_INPUTS` (around line 48-53):

```makefile
EMBED_INPUTS := shaders/model.vert shaders/model.frag shaders/fullscreen.vert \
                shaders/post_bright.frag shaders/post_down.frag shaders/post_up.frag \
                shaders/post_combine.frag shaders/post_streak.frag \
                shaders/post_finish.frag shaders/post_fxaa.frag shaders/background.frag \
                shaders/particle.vert shaders/particle.frag shaders/wboit_resolve.frag \
                shaders/wireframe.vert shaders/wireframe.geom shaders/wireframe.frag \
                res/lang/pt.txt res/lang/en.txt res/env_default.jpg
```

- [ ] **Step 5: Build to verify the new shaders embed cleanly**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```
Expected: `all tests passed` (the `test` target already depends on `$(EMBEDDED_H)`, so this exercises
`embed.exe` against the 3 new files - failure here means a bad path, not a GLSL error, since nothing
compiles the shader source yet).

- [ ] **Step 6: Commit**

```bash
git add shaders/wireframe.vert shaders/wireframe.geom shaders/wireframe.frag build/Makefile
git commit -m "feat: add Wireframe material shaders (vert+geom+frag)"
```

---

### Task 5: `scene.c` - mesh-side wiring (edge list per rebuild)

**Files:**
- Modify: `src/scene.c`

**Interfaces:**
- Consumes: `feature_edges_build()` (Task 1), `gl_program_gs()` (Task 2),
  `Config.wireframe_thickness`/`wireframe_xray` (Task 3), `EMBED_wireframe_*` (Task 4).
- Produces: `SceneRenderer` fields `wire_prog`, `wire_vao`, `wire_vbo`, `wire_edge_count`,
  `wire_depth_fbo`, `wire_depth_tex`, `wire_depth_w`, `wire_depth_h`, `wireframe_thickness`,
  `wireframe_xray` - populated on every mesh rebuild, not yet consumed by rendering (Task 6).

This task has no isolated unit test (GL-dependent, and material_mode==3 isn't reachable via UI
until Task 7) - it's verified by confirming the build stays green and every existing mesh rebuild
path (text/clock/SVG/imported mesh) still works, since the new code runs unconditionally on every
rebuild regardless of which material is selected.

- [ ] **Step 1: Add the new fields to `struct SceneRenderer`**

In `src/scene.c`, add right after the `float edge_bias;` field (around line 49):

```c
    float    wireframe_thickness;
    int      wireframe_xray;

    /* Wireframe - lista de arestas (GL_LINES) + FBO de profundidade
       dedicada de 1 amostra (so' usada no modo Raio-X - ver spec
       2026-09-17-wireframe-material-design.md secao 5) */
    unsigned wire_prog;
    unsigned wire_vao, wire_vbo;
    int      wire_edge_count;
    unsigned wire_depth_fbo, wire_depth_tex;
    int      wire_depth_w, wire_depth_h;
```

- [ ] **Step 2: Add the edge-accumulator helpers**

Add these `static` helpers right before `static int rebuild_mesh(SceneRenderer *s)` (around line 248):

```c
typedef struct { WireEdge *edges; int count, cap; } WireEdgeAccum;

static void wire_accum_append(WireEdgeAccum *acc, const MeshData *md)
{
    WireEdge *e = NULL; int n = 0;
    if (!feature_edges_build(md, 35.0f, &e, &n)) return;
    if (acc->count + n > acc->cap) {
        acc->cap = (acc->count + n) * 2 + 16;
        acc->edges = (WireEdge *)realloc(acc->edges, (size_t)acc->cap * sizeof(WireEdge));
    }
    memcpy(acc->edges + acc->count, e, (size_t)n * sizeof(WireEdge));
    acc->count += n;
    free(e);
}

/* faz upload da lista acumulada pro VBO de SceneRenderer (substitui o
   anterior, se houver) e libera o buffer temporario do acumulador -
   sempre chamada no final de cada rebuild_*, mesmo em caminhos de
   erro (acc vazio -> wire_edge_count vira 0, sem desenhar nada de
   arestas obsoletas de um conteudo anterior). */
static void wire_accum_finish(SceneRenderer *s, WireEdgeAccum *acc)
{
    if (s->wire_vao) {
        glDeleteVertexArrays(1, &s->wire_vao);
        glDeleteBuffers(1, &s->wire_vbo);
        s->wire_vao = s->wire_vbo = 0;
    }
    s->wire_edge_count = acc->count;
    if (acc->count > 0) {
        glGenVertexArrays(1, &s->wire_vao);
        glGenBuffers(1, &s->wire_vbo);
        glBindVertexArray(s->wire_vao);
        glBindBuffer(GL_ARRAY_BUFFER, s->wire_vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(acc->count * (int)sizeof(WireEdge)),
                     acc->edges, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(v3), (void *)0);
        glBindVertexArray(0);
    }
    free(acc->edges);
}
```

Add `#include "geometry/feature_edges.h"` near the top of `src/scene.c`, alongside the other
`geometry/` includes (right after `#include "geometry/mesh_import.h"`).

- [ ] **Step 3: Hook the accumulator into `rebuild_mesh`**

In `rebuild_mesh()` (single-piece text/clock content), right after the existing
`s->mesh = gl_mesh_upload(md.verts, md.nverts, md.idx, md.nidx);` line and before
`mesh_data_free(&md);`, add:

```c
    WireEdgeAccum wacc = { 0 };
    wire_accum_append(&wacc, &md);
    wire_accum_finish(s, &wacc);
```

- [ ] **Step 4: Hook the accumulator into `rebuild_svg_mesh`**

Replace the ENTIRE function body with this updated version (the only changes are the `WireEdgeAccum
wacc` declaration and the 3 lines calling `wire_accum_append`/`wire_accum_finish`, marked `/* NOVO
*/` below - everything else is byte-for-byte the existing code):

```c
static int rebuild_svg_mesh(SceneRenderer *s)
{
    free_svg_pieces(s);
    free_error_plaque(s);

    float tol = s->quality <= 0 ? 0.010f : (s->quality == 1 ? 0.004f : 0.0018f);

    SvgShapeSet svgset;
    int loaded = s->svg_path[0] != 0 && svg_shapes_load(s->svg_path, tol, &svgset);
    if (!loaded || svgset.count == 0) {
        if (loaded) svg_shapes_free(&svgset);
        s->have_svg_mesh = 1;
        char msg[256];
        WideCharToMultiByte(CP_UTF8, 0, i18n_str(STR_ERROR_SVG_NO_SHAPE), -1,
                            msg, (int)sizeof msg, NULL, NULL);
        return build_error_plaque(s, msg);
    }

    s->svg_meshes = (GlMesh *)calloc((size_t)svgset.count, sizeof(GlMesh));
    s->svg_colors = (v3 *)calloc((size_t)svgset.count, sizeof(v3));
    if (!s->svg_meshes || !s->svg_colors) {
        svg_shapes_free(&svgset);
        free_svg_pieces(s);
        return 0;
    }

    WireEdgeAccum wacc = { 0 };   /* NOVO */
    float uhx = 0.0f, uhy = 0.0f, uhz = 0.0f;
    s->wall_cache_count = 0;
    for (int i = 0; i < svgset.count; ++i) {
        MeshData md;
        if (!contour_mesh_build(&svgset.pieces[i].cs, scene_mesh_params(s), &md)) continue;

        s->svg_meshes[s->svg_mesh_count] = gl_mesh_upload(md.verts, md.nverts, md.idx, md.nidx);
        s->svg_colors[s->svg_mesh_count] =
            (v3){ svgset.pieces[i].r, svgset.pieces[i].g, svgset.pieces[i].b };
        wire_accum_append(&wacc, &md);   /* NOVO */

        float phx = 0.5f * (md.maxx - md.minx);
        float phy = 0.5f * (md.maxy - md.miny);
        float phz = 0.5f * (md.maxz - md.minz);
        if (phx > uhx) uhx = phx;
        if (phy > uhy) uhy = phy;
        if (phz > uhz) uhz = phz;

        for (int vi = 0; vi < md.nverts && s->wall_cache_count < 512; ++vi) {
            if (md.verts[vi].surf == 2.0f) {
                s->wall_pos_cache[s->wall_cache_count] =
                    (v3){ md.verts[vi].px, md.verts[vi].py, md.verts[vi].pz };
                s->wall_n_cache[s->wall_cache_count] =
                    (v3){ md.verts[vi].nx, md.verts[vi].ny, md.verts[vi].nz };
                s->wall_cache_count++;
            }
        }

        s->svg_mesh_count++;
        mesh_data_free(&md);
    }
    svg_shapes_free(&svgset);

    if (s->svg_mesh_count == 0) {
        wire_accum_finish(s, &wacc);   /* NOVO - zera arestas obsoletas tambem no erro */
        free_svg_pieces(s);
        s->have_svg_mesh = 1;
        char msg[256];
        WideCharToMultiByte(CP_UTF8, 0, i18n_str(STR_ERROR_SVG_EMPTY), -1,
                            msg, (int)sizeof msg, NULL, NULL);
        return build_error_plaque(s, msg);
    }

    if (uhx < 1e-3f) uhx = 1.0f;
    if (uhy < 1e-3f) uhy = 1.0f;
    s->hx = uhx; s->hy = uhy; s->hz = uhz;
    wire_accum_finish(s, &wacc);   /* NOVO */
    s->have_svg_mesh = 1;
    log_infof("scene: svg '%ls' -> %d peca(s)", s->svg_path, s->svg_mesh_count);
    return 1;
}
```

- [ ] **Step 5: Hook the accumulator into `rebuild_imported_mesh`**

`rebuild_imported_mesh()` has two branches (multi-piece via `mesh_import_load_pieces`, and a
single-piece fallback via `mesh_import_load`), each with their own success and failure paths.
Replace the ENTIRE function body with this updated version (the only changes are the `WireEdgeAccum
wacc` declaration at the top and the 4 lines calling `wire_accum_append`/`wire_accum_finish`,
marked `/* NOVO */` below - everything else is byte-for-byte the existing code):

```c
static int rebuild_imported_mesh(SceneRenderer *s)
{
    free_mesh_pieces(s);
    free_error_plaque(s);
    s->have_mesh_content = 1;

    WireEdgeAccum wacc = { 0 };   /* NOVO */

    if (s->mesh_path[0] != 0 && s->mesh_use_file_materials) {
        MeshPieceSet ps;
        if (mesh_import_load_pieces(s->mesh_path, s->mesh_size_scale, &ps)) {
            s->mesh_pieces = (GlMesh *)calloc((size_t)ps.count, sizeof(GlMesh));
            s->mesh_piece_colors = (v3 *)calloc((size_t)ps.count, sizeof(v3));
            if (s->mesh_pieces && s->mesh_piece_colors) {
                float uhx = 0.0f, uhy = 0.0f, uhz = 0.0f;
                for (int i = 0; i < ps.count; ++i) {
                    MeshData *d = &ps.pieces[i].data;
                    s->mesh_pieces[i] = gl_mesh_upload(d->verts, d->nverts, d->idx, d->nidx);
                    s->mesh_piece_colors[i] = (v3){ ps.pieces[i].r, ps.pieces[i].g, ps.pieces[i].b };
                    wire_accum_append(&wacc, d);   /* NOVO */
                    float phx = 0.5f * (d->maxx - d->minx);
                    float phy = 0.5f * (d->maxy - d->miny);
                    float phz = 0.5f * (d->maxz - d->minz);
                    if (phx > uhx) uhx = phx;
                    if (phy > uhy) uhy = phy;
                    if (phz > uhz) uhz = phz;
                }
                s->mesh_piece_count = ps.count;
                if (uhx < 1e-3f) uhx = 1.0f;
                if (uhy < 1e-3f) uhy = 1.0f;
                s->hx = uhx; s->hy = uhy; s->hz = uhz;
                s->wall_cache_count = 0;
                wire_accum_finish(s, &wacc);   /* NOVO */
                log_infof("scene: malha '%ls' -> %d peca(s) com material do arquivo",
                          s->mesh_path, s->mesh_piece_count);
                mesh_import_pieces_free(&ps);
                return 1;
            }
            wire_accum_finish(s, &wacc);   /* NOVO - zera arestas obsoletas tambem no erro */
            free_mesh_pieces(s);
            mesh_import_pieces_free(&ps);
            return 0;
        }
    }

    MeshData md;
    int ok = s->mesh_path[0] != 0 && mesh_import_load(s->mesh_path, s->mesh_size_scale, &md);
    if (!ok) {
        wire_accum_finish(s, &wacc);   /* NOVO */
        char msg[256];
        WideCharToMultiByte(CP_UTF8, 0, i18n_str(STR_ERROR_MESH_INVALID), -1,
                            msg, (int)sizeof msg, NULL, NULL);
        return build_error_plaque(s, msg);
    }

    if (s->have_mesh) gl_mesh_free(&s->mesh);
    s->mesh = gl_mesh_upload(md.verts, md.nverts, md.idx, md.nidx);
    wire_accum_append(&wacc, &md);   /* NOVO */
    s->hx = 0.5f * (md.maxx - md.minx);
    s->hy = 0.5f * (md.maxy - md.miny);
    s->hz = 0.5f * (md.maxz - md.minz);
    if (s->hx < 1e-3f) s->hx = 1.0f;
    if (s->hy < 1e-3f) s->hy = 1.0f;
    s->wall_cache_count = 0;   /* malha importada nao tem paredes - faiscas nao emitem nela */
    int nv = md.nverts;
    mesh_data_free(&md);
    wire_accum_finish(s, &wacc);   /* NOVO */
    s->have_mesh = 1;
    log_infof("scene: malha '%ls' -> %d verts", s->mesh_path, nv);
    return 1;
}
```

- [ ] **Step 6: Compile `wire_prog` in `scene_create`, clean it up in `scene_destroy`**

In `scene_create()`, right after the existing `s->wboit_resolve_prog` block (after its
`glUniform1i(glGetUniformLocation(s->wboit_resolve_prog, "uRevealLog"), 1);` line), add:

```c
    s->wire_prog = gl_program_gs((const char *)EMBED_wireframe_vert,
                                  (const char *)EMBED_wireframe_geom,
                                  (const char *)EMBED_wireframe_frag);
    if (!s->wire_prog) {
        glDeleteProgram(s->wboit_resolve_prog);
        glDeleteProgram(s->bg_prog);
        material_destroy(&s->mat);
        free(s);
        return NULL;
    }
```

In `scene_destroy()`, right after the existing `if (s->wboit_resolve_prog)
glDeleteProgram(s->wboit_resolve_prog);` line, add:

```c
    if (s->wire_prog) glDeleteProgram(s->wire_prog);
    if (s->wire_vao) { glDeleteVertexArrays(1, &s->wire_vao); glDeleteBuffers(1, &s->wire_vbo); }
    if (s->wire_depth_fbo) {
        glDeleteFramebuffers(1, &s->wire_depth_fbo);
        glDeleteTextures(1, &s->wire_depth_tex);
    }
```

- [ ] **Step 7: Sync the new Config fields in `scene_set_config`**

Right after the existing `s->edge_bias = cfg->edge_bias;` line (around line 543), add:

```c
    s->wireframe_thickness = cfg->wireframe_thickness;
    s->wireframe_xray = cfg->wireframe_xray;
```

- [ ] **Step 8: Build and smoke-test**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
mingw32-make -f build/Makefile release
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
MSYS_NO_PATHCONV=1 M3DT_SELFTEST=1 dist/Modern3DText.exe /s
```
Expected: `all tests passed`; the release build links cleanly (confirms `wireframe.vert/.geom/.frag`
actually compile and link as a program, not just embed as text); the selftest log
(`%LOCALAPPDATA%\Modern3DText\log.txt`) shows no shader/link error and `saver encerrou (frames=90)`.

- [ ] **Step 9: Commit**

```bash
git add src/scene.c
git commit -m "feat: build and upload the Wireframe edge list on every mesh rebuild"
```

---

### Task 6: `scene.c` - render-side (depth pre-pass + line draw dispatch)

**Files:**
- Modify: `src/scene.c`

**Interfaces:**
- Consumes: everything from Task 5 (`s->wire_prog`, `s->wire_vao`/`wire_edge_count`,
  `s->wireframe_thickness`/`wireframe_xray`).
- Produces: `material_mode == 3` becomes a real, renderable material path in `scene_render()`.

No unit test (GL rendering). Verified via a temporary registry override (`material_mode=3`) and a
real `M3DT_SHOT` capture, same technique used throughout this project for Vidro - see Step 4.

- [ ] **Step 1: Add `ensure_wire_depth_target`**

Add this `static` helper near `ensure_wboit_targets` (same file, similar lazy-FBO pattern):

```c
static void ensure_wire_depth_target(SceneRenderer *s, int w, int h)
{
    if (s->wire_depth_fbo && s->wire_depth_w == w && s->wire_depth_h == h) return;
    if (s->wire_depth_fbo) {
        glDeleteFramebuffers(1, &s->wire_depth_fbo);
        glDeleteTextures(1, &s->wire_depth_tex);
    }
    glGenTextures(1, &s->wire_depth_tex);
    glBindTexture(GL_TEXTURE_2D, s->wire_depth_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &s->wire_depth_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s->wire_depth_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, s->wire_depth_tex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    s->wire_depth_w = w;
    s->wire_depth_h = h;
}
```

- [ ] **Step 2: Add `draw_wireframe_lines`**

Add this `static` helper right after `draw_content()`:

```c
static void draw_wireframe_lines(SceneRenderer *s, m4 view, m4 proj, m4 model, int fb_w, int fb_h)
{
    if (s->wire_edge_count == 0 || !s->wire_prog) return;

    glUseProgram(s->wire_prog);
    glUniformMatrix4fv(glGetUniformLocation(s->wire_prog, "uModel"), 1, GL_FALSE, model.m);
    glUniformMatrix4fv(glGetUniformLocation(s->wire_prog, "uView"),  1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(s->wire_prog, "uProj"),  1, GL_FALSE, proj.m);
    glUniform2f(glGetUniformLocation(s->wire_prog, "uViewportSize"), (float)fb_w, (float)fb_h);
    glUniform1f(glGetUniformLocation(s->wire_prog, "uThicknessPx"), s->wireframe_thickness);
    glUniform3f(glGetUniformLocation(s->wire_prog, "uLineColor"),
                s->base_color.x, s->base_color.y, s->base_color.z);
    glUniform3f(glGetUniformLocation(s->wire_prog, "uEmissiveColor"),
                s->emissive_color.x, s->emissive_color.y, s->emissive_color.z);
    glUniform1f(glGetUniformLocation(s->wire_prog, "uEmissiveAmount"), s->emissive_amount);
    glUniform1i(glGetUniformLocation(s->wire_prog, "uXray"), s->wireframe_xray);

    glBindVertexArray(s->wire_vao);

    if (s->wireframe_xray) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s->wire_depth_tex);
        glUniform1i(glGetUniformLocation(s->wire_prog, "uSolidDepth"), 0);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDrawArrays(GL_LINES, 0, s->wire_edge_count * 2);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glActiveTexture(GL_TEXTURE0);
    } else {
        glDepthMask(GL_FALSE);
        glDrawArrays(GL_LINES, 0, s->wire_edge_count * 2);
        glDepthMask(GL_TRUE);
    }

    glBindVertexArray(0);
}
```

- [ ] **Step 3: Dispatch to it from `scene_render()`**

In `scene_render()`, the existing dispatch reads:

```c
    if (s->material_mode == 2 && s->debug_view == 0) {     /* vidro: WBOIT */
        ...
    } else {
        draw_content(s);
    }
```

Change the `else` to check for Wireframe first:

```c
    if (s->material_mode == 2 && s->debug_view == 0) {     /* vidro: WBOIT */
        ...
    } else if (s->material_mode == 3) {                     /* wireframe */
        /* passo 1 (sempre): profundidade "invisivel" da malha solida
           direto no alvo HDR principal ja ligado - nenhuma FBO nova
           precisa pra oclusao normal, o buffer que ja esta' la
           (post_begin) basta. */
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        draw_content(s);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        if (s->wireframe_xray) {
            /* passo 2 (so' raio-x): repete o mesmo desenho numa FBO
               dedicada de 1 amostra so', texture-backed - o shader das
               linhas precisa AMOSTRAR essa profundidade (nao so'
               testar contra ela), e nao da' pra amostrar direto o
               anexo de profundidade do alvo principal quando ele e'
               multisample (MSAA liga/desliga conforme a qualidade
               escolhida pelo usuario). */
            ensure_wire_depth_target(s, fb_w, fb_h);
            GLint prev_fbo = 0;
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, s->wire_depth_fbo);
            glViewport(0, 0, fb_w, fb_h);
            glClear(GL_DEPTH_BUFFER_BIT);
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            draw_content(s);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
            glViewport(0, 0, fb_w, fb_h);
        }

        draw_wireframe_lines(s, view, proj, model, fb_w, fb_h);
    } else {
        draw_content(s);
    }
```

- [ ] **Step 4: Build and verify visually via a real capture**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
mingw32-make -f build/Makefile release
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
MSYS_NO_PATHCONV=1 M3DT_SELFTEST=1 dist/Modern3DText.exe /s
```
Expected: `all tests passed`; selftest log clean, no shader/link errors, `frames=90`, `rc=0`.

Then, with the user's standing authorization to alter the registry for testing (established earlier
this session - restore it exactly afterward): temporarily set `material_mode=3` via `reg add
HKCU\Software\Modern3DText /v material_mode /t REG_DWORD /d 3 /f`, capture a real frame with
`M3DT_SHOT`/`M3DT_SHOT_T` (retry 2-3x if the run exits early - known `/s` flakiness, see
`ref-w64devkit-build` memory), inspect the PNG: expect to see line-drawn letter edges, not a solid
filled object. Also test `wireframe_xray=1` the same way (expect hidden lines dimmer than visible
ones). Restore the registry to its prior state afterward (`reg delete` whatever wasn't there
before).

- [ ] **Step 5: Commit**

```bash
git add src/scene.c
git commit -m "feat: render the Wireframe material (occlusion + X-ray)"
```

---

### Task 7: UI wiring (Material tab)

**Files:**
- Modify: `src/resource.h`
- Modify: `res/screensaver.rc`
- Modify: `src/i18n.h`
- Modify: `src/i18n.c`
- Modify: `res/lang/pt.txt`
- Modify: `res/lang/en.txt`
- Modify: `src/config_dialog.c`

**Interfaces:**
- Consumes: `Config.wireframe_thickness`/`wireframe_xray` (Task 3).
- Produces: a selectable "Wireframe" entry in the Material combo, a line-thickness slider, and an
  X-ray checkbox, following the tab's existing dynamic block-reflow system.

No automated test (Win32 dialog UI). Verified manually: build, run `/c`, select Wireframe, confirm
the thickness slider and X-ray checkbox appear (and Metalização/Aspereza/Ambiente disappear),
confirm Emissivo also appears, drag the slider and toggle X-ray and confirm the live preview updates
(`preview_dirty`).

- [ ] **Step 1: New resource IDs**

In `src/resource.h`, add right after `#define IDC_EDGEBIAS 1321`:

```c
#define IDC_WIRE_THICK_LABEL 1322
#define IDC_WIRE_THICK_VAL   1323
#define IDC_WIRE_THICK       1324
#define IDC_WIRE_XRAY        1325
```

- [ ] **Step 2: New i18n string IDs**

In `src/i18n.h`, in the `StrId` enum: add `STR_MATERIAL_MODE_WIREFRAME,` right after
`STR_MATERIAL_MODE_GLASS,`, and add `STR_MATERIAL_WIRE_THICKNESS_LABEL, STR_MATERIAL_WIRE_XRAY,`
right after `STR_MATERIAL_EDGEBIAS_LABEL,`.

In `src/i18n.c`'s `KEY_NAMES` array, mirror the exact same relative positions: add
`"material.mode.wireframe",` right after `"material.mode.glass",`, and add
`"material.wire_thickness_label", "material.wire_xray",` right after
`"material.edgebias_label",`.

- [ ] **Step 3: New language strings**

In `res/lang/pt.txt`, add right after `material.mode.glass=Vidro`:
```
material.mode.wireframe=Wireframe
```
and right after `material.edgebias_label=Viés aresta/plano:`:
```
material.wire_thickness_label=Espessura da linha:
material.wire_xray=Raio-X
```

In `res/lang/en.txt`, add right after `material.mode.glass=Glass`:
```
material.mode.wireframe=Wireframe
```
and right after `material.edgebias_label=Edge/flat bias:`:
```
material.wire_thickness_label=Line thickness:
material.wire_xray=X-ray
```

- [ ] **Step 4: New controls in the `.rc` Material tab**

In `res/screensaver.rc`, change the `IDD_TAB_MATERIAL` template's declared height (purely a
design-time canvas size - `place_tab_child` resizes it to the real tab area at runtime, so this
doesn't affect anything functional, just keeps the file internally tidy):

```
IDD_TAB_MATERIAL DIALOGEX 0, 0, 240, 320
```

Add these two new controls right after the existing `PUSHBUTTON "Limpar", IDC_ENVCLEAR, 72, 220,
50, 14` line, before the block's `END`:

```
    LTEXT      "Espessura da linha:", IDC_WIRE_THICK_LABEL, 8, 240, 110, 9
    LTEXT      "", IDC_WIRE_THICK_VAL, 182, 240, 50, 9
    CONTROL    "", IDC_WIRE_THICK, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 250, 224, 18
    CONTROL    "Raio-X", IDC_WIRE_XRAY, "Button", BS_AUTOCHECKBOX | WS_TABSTOP, 8, 274, 60, 10
```

- [ ] **Step 5: Extend the block-reflow system in `config_dialog.c`**

Change `#define MAT_BLOCKS 6` to `#define MAT_BLOCKS 8`.

Replace the `g_mat_blocks`/`MAT_BLOCK_N` definitions with (inserting the 2 new blocks between
EdgeBias and the Environment header - order here controls the visual stacking order, independent of
each block's own raw `.rc` position):

```c
static MatCtrl g_mat_blocks[MAT_BLOCKS][4] = {
    { { IDC_METAL_LABEL, 0, 0 }, { IDC_METAL_VAL, 0, 0 }, { IDC_METAL, 0, 0 } },
    { { IDC_ROUGH_LABEL, 0, 0 }, { IDC_ROUGH_VAL, 0, 0 }, { IDC_ROUGH, 0, 0 } },
    { { IDC_EMISSIVE_LABEL, 0, 0 }, { IDC_EMISSIVE_COLOR, 0, 0 }, { IDC_EMISSIVE_VAL, 0, 0 }, { IDC_EMISSIVE, 0, 0 } },
    { { IDC_EDGEBIAS_LABEL, 0, 0 }, { IDC_EDGEBIAS_VAL, 0, 0 }, { IDC_EDGEBIAS, 0, 0 } },
    { { IDC_WIRE_THICK_LABEL, 0, 0 }, { IDC_WIRE_THICK_VAL, 0, 0 }, { IDC_WIRE_THICK, 0, 0 } },
    { { IDC_WIRE_XRAY, 0, 0 } },
    { { IDC_ENV_LABEL, 0, 0 }, { IDC_ENVMODE_EMBED, 0, 0 }, { IDC_ENVMODE_CUSTOM, 0, 0 }, { IDC_ENVMODE_NONE, 0, 0 } },
    { { IDC_ENVPATH, 0, 0 }, { IDC_ENVPICK, 0, 0 }, { IDC_ENVCLEAR, 0, 0 } },
};
static const int MAT_BLOCK_N[MAT_BLOCKS] = { 3, 3, 4, 3, 3, 1, 4, 3 };
```

- [ ] **Step 6: Update `material_layout_apply()`'s visibility rules**

Replace the function body's visibility computation:

```c
static void material_layout_apply(HWND h)
{
    int mode = g_work.material_mode;
    int vis_metal      = (mode == 1);
    int vis_rough      = (mode == 0 || mode == 1 || mode == 2);
    int vis_emissive   = (mode == 0 || mode == 2 || mode == 3);
    int vis_edgebias   = (mode == 2);
    int vis_wire_thick = (mode == 3);
    int vis_wire_xray  = (mode == 3);
    int vis_env_hdr    = (mode == 1 || mode == 2);
    int vis_env_pick   = vis_env_hdr && (g_work.env_mode == 1);
    int visible[MAT_BLOCKS] = { vis_metal, vis_rough, vis_emissive, vis_edgebias,
                                 vis_wire_thick, vis_wire_xray, vis_env_hdr, vis_env_pick };

    int cursor = g_mat_block_top[0];
    for (int b = 0; b < MAT_BLOCKS; ++b) {
        if (!visible[b]) {
            for (int i = 0; i < MAT_BLOCK_N[b]; ++i)
                ShowWindow(GetDlgItem(h, g_mat_blocks[b][i].id), SW_HIDE);
            continue;
        }
        for (int i = 0; i < MAT_BLOCK_N[b]; ++i) {
            HWND ctrl = GetDlgItem(h, g_mat_blocks[b][i].id);
            SetWindowPos(ctrl, NULL, g_mat_blocks[b][i].x, cursor + g_mat_blocks[b][i].rel_y,
                         0, 0, SWP_NOSIZE | SWP_NOZORDER);
            ShowWindow(ctrl, SW_SHOW);
        }
        cursor += g_mat_block_h[b] + g_mat_gap;
    }
}
```

(Only the visibility-flags section actually changed; the stacking loop below it is unchanged - keep
it as-is.)

- [ ] **Step 7: Wire the combo entry, labels, i18n, slider, checkbox**

In `material_labels()`, add right after the existing `IDC_EDGEBIAS_VAL` line:

```c
    swprintf(b, 32, L"%.1fpx", (double)g_work.wireframe_thickness); SetDlgItemTextW(h, IDC_WIRE_THICK_VAL, b);
```

In `material_apply_i18n()`:
- Add right after the existing `SetDlgItemTextW(h, IDC_EDGEBIAS_LABEL, ...)` line:
  ```c
  SetDlgItemTextW(h, IDC_WIRE_THICK_LABEL, i18n_str(STR_MATERIAL_WIRE_THICKNESS_LABEL));
  SetDlgItemTextW(h, IDC_WIRE_XRAY, i18n_str(STR_MATERIAL_WIRE_XRAY));
  ```
- Add the combo entry right after the existing
  `SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_MATERIAL_MODE_GLASS));` line:
  ```c
  SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_MATERIAL_MODE_WIREFRAME));
  ```

In `material_proc`'s `WM_INITDIALOG` handler:
- Add right after the existing `set_slider(h, IDC_EDGEBIAS, 0, 100, ...)` line:
  ```c
  set_slider(h, IDC_WIRE_THICK, 10, 60, (int)(g_work.wireframe_thickness * 10.0f + 0.5f));
  ```
- Add right after the existing `CheckRadioButton(h, IDC_ENVMODE_EMBED, ...)` block:
  ```c
  CheckDlgButton(h, IDC_WIRE_XRAY, g_work.wireframe_xray ? BST_CHECKED : BST_UNCHECKED);
  ```

In `material_proc`'s `WM_HSCROLL` handler, add right after the existing `g_work.edge_bias = ...`
line:
```c
    g_work.wireframe_thickness = (float)SendDlgItemMessageW(h, IDC_WIRE_THICK, TBM_GETPOS, 0, 0) / 10.0f;
```

In `material_proc`'s `WM_COMMAND` handler's `switch (LOWORD(w))`, add a new case (anywhere among the
existing `case IDC_ENVMODE_*:` cases):
```c
        case IDC_WIRE_XRAY:
            g_work.wireframe_xray = (IsDlgButtonChecked(h, IDC_WIRE_XRAY) == BST_CHECKED);
            preview_dirty(h);
            break;
```

- [ ] **Step 8: Build and verify manually**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
mingw32-make -f build/Makefile release
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
MSYS_NO_PATHCONV=1 M3DT_SELFTEST=1 dist/Modern3DText.exe /s
```
Expected: `all tests passed`, selftest log clean.

Then a real capture via the config dialog's own `M3DT_SHOT` path (`M3DT_TAB=2` selects the Material
tab; see `ref-w64devkit-build`/earlier-session precedent for this exact invocation pattern) - or,
simpler, repeat Task 6 Step 4's registry-based `/s` capture, now that `material_mode=3` is also
reachable through the real dialog: open the config dialog, select Material tab, choose "Wireframe"
from the combo, confirm Metalização/Aspereza/Ambiente disappear and Emissivo + the 2 new controls
appear with no overlapping/cut-off controls, drag the thickness slider and toggle X-ray, confirm the
live preview visibly updates each time.

- [ ] **Step 9: Commit**

```bash
git add src/resource.h res/screensaver.rc src/i18n.h src/i18n.c \
        res/lang/pt.txt res/lang/en.txt src/config_dialog.c
git commit -m "feat: add Wireframe material to the config dialog's Material tab"
```

---

## Self-Review Notes

- **Spec coverage:** Section 3 (edge detection) → Task 1. Section 4 (Config) → Task 3. Section 5/7
  (depth pre-pass + occlusion/X-ray) → Task 6. Section 6 (line shaders) → Task 4. Section 8 (UI) →
  Task 7. Section 10 (tests) → Task 1's TDD cube test + each task's build/selftest verification.
  Section 2's "no material.c/model.frag changes needed" architectural simplification (Wireframe
  reuses the already-bound `Material.prog` purely for its depth side-effect during the pre-pass, via
  `glColorMask`, never for its color output) is reflected in Task 6 - no `material.c`/`model.frag`
  task exists because none is needed.
- **Type consistency:** `WireEdge` (Task 1) is consumed identically in Task 5's `wire_accum_append`.
  `gl_program_gs()`'s signature (Task 2) matches its Task 5 call site exactly. `Config` field names
  (Task 3) match `scene_set_config`'s reads (Task 5) and `config_dialog.c`'s reads/writes (Task 7)
  exactly.
