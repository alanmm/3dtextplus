# Modern 3D Text — Fase 2a: Texto 3D extrudado — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** O `Modern3DText.scr` passa a renderizar **texto 3D extrudado de verdade** — contornos de uma fonte instalada → tampa triangulada (com furos) + paredes laterais → malha OpenGL — girando com o pêndulo limitado e iluminado pelo material especular clássico. Parâmetros ainda fixos no código (a config entra na Fase 2b).

**Architecture:** Três camadas novas sobre o esqueleto da Fase 1. (1) **Geometria**: `font_outline` resolve a fonte via GDI e extrai contornos de glifo com `stb_truetype`; `contour_mesh` trianguliza os contornos com `libtess2` (lida com furos de 'A'/'O') e extruda em tampa-frente + tampa-verso + paredes. (2) **GL moderno**: `glad` carrega as funções do OpenGL 3.3 core; `gl_core` compila shaders e sobe VBO/VAO/EBO. (3) **Cena**: `scene` monta a malha uma vez, posiciona a câmera por auto-fit nos bounds do modelo, anima o pêndulo e desenha; `material` (modo 0) é um Blinn-Phong + rim de 3 luzes fixas. O `host_win32` troca o `glClear` colorido da Fase 1 por um `SceneRenderer` por janela.

**Tech Stack:** C11 · w64devkit (gcc 16.2, `mingw32-make`, `windres`) · OpenGL 3.3 core via `glad` · `stb_truetype.h` · `libtess2` · GLSL 330 · shaders embutidos no binário por um gerador `embed`.

## Global Constraints

Do spec (`docs/superpowers/specs/2026-09-10-modern-3d-text-screensaver-design.md`) e do estado pós-Fase 1. Todo task herda esta seção.

- **Linguagem:** C11. Flags: `-std=c11 -municode -Wall -Wextra`; release `+ -O2 -DNDEBUG`; **nunca** `-ffast-math`. O build tem que ficar **sem warnings** a `-O2 -Wall -Wextra`.
- **Toolchain:** só w64devkit (`C:\Users\alanm\w64devkit`, versão em `toolchain.txt`). `gcc` precisa de `<w64devkit>\bin` no PATH (acha `as`/`ld`). Gerar `glad` uma vez com o Python 3.14 da máquina e **commitar** o resultado — o build em si continua só w64devkit.
- **API gráfica:** OpenGL **3.3 core**. `glad` carregado com loader que usa `wglGetProcAddress` **com fallback para `GetProcAddress(opengl32.dll, ...)`** (funções GL 1.1 não vêm pelo `wglGetProcAddress` no Windows).
- **Libs de terceiros:** vendorizadas em `third_party/`, licenças permissivas (`stb` = domínio público / MIT; `libtess2` = SGI/MIT; `glad` = MIT/gerado). Nada de gerenciador de pacotes no build.
- **Sem dependência de runtime** além de DLLs do Windows. Libs de link inalteradas em relação à Fase 1: `-lopengl32 -lgdi32 -luser32 -lkernel32 -lcomdlg32 -lcomctl32 -lshell32 -lole32 -ladvapi32 -ldwmapi -lwinmm`.
- **Nomes:** binário `Modern3DText.scr`; dados locais em `%LOCALAPPDATA%\Modern3DText\` (log). Sem escrita fora disso e de `HKCU\Software\Modern3DText` (a 2b usa o registro; a 2a ainda não).
- **Tamanho:** o build falha se `dist/Modern3DText.scr` passar de 3 MB.
- **Commits frequentes.** **TDD** nas unidades puras: `mathx`, `contour_mesh`. `font_outline` é testado com asserções robustas a versão de fonte (usa "Arial", sempre presente). Janela/GL: verificação headless (contexto oculto + `glReadPixels`) + captura PNG conferível.
- **Atribuição:** toda mensagem de commit termina com `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>`.
- **Desvios conscientes do spec nesta fase:** (a) o **micro-bevel** do §5.5 é adiado para a Fase 3 (junto com o bevel SDF/geometria) — a 2a faz só tampa + paredes (quinas vivas); (b) **não** há fonte de fallback embutida (`res/fallback-font.ttf` do spec §15 não é criado) — o usuário escolheu "só fontes instaladas", então a fonte ausente cai para uma fonte garantida do sistema (Segoe UI) por caminho.

## Pré-requisitos de vendoring (fazer uma vez, no Task 2 e no Task 4/5 conforme indicado)

Baixados de fontes oficiais, commitados no repo:

| Lib | Origem | Arquivos no repo |
|---|---|---|
| glad (GL 3.3 core) | gerado por `glad2` (pip) | `third_party/glad/include/glad/gl.h`, `third_party/glad/include/KHR/khrplatform.h`, `third_party/glad/src/gl.c` |
| stb_truetype | `raw.githubusercontent.com/nothings/stb/master/stb_truetype.h` | `third_party/stb_truetype.h` |
| stb_image_write | `raw.githubusercontent.com/nothings/stb/master/stb_image_write.h` | `third_party/stb_image_write.h` |
| libtess2 | `github.com/memononen/libtess2` (tarball do master) | `third_party/libtess2/Include/tesselator.h`, `third_party/libtess2/Source/*.{c,h}` |

---

## File Structure

| Arquivo | Responsabilidade |
|---|---|
| `src/util/mathx.h` / `.c` | `v2`/`v3`/`m4` (coluna-major) e ops; `m4_perspective`/`look_at`/`translate`/`rotate_x`/`rotate_y`/`mul`/`mul_point`; `pendulum_angle` |
| `third_party/glad/**` | Loader OpenGL 3.3 core gerado |
| `src/gl_core.h` / `.c` | `gl_load()`; `gl_program(vs,fs)`; `GlMesh` + `gl_mesh_upload`/`draw`/`free`; `MeshVertex` |
| `build/tools/embed.c` | Gera `generated/embedded.h` (arrays de bytes dos shaders, terminados em `\0`) |
| `shaders/model.vert` / `model.frag` | GLSL 330 do material modo 0 (especular clássico) |
| `third_party/stb_truetype.h`, `third_party/stb_image_write.h` | headers vendorizados |
| `src/geometry/stb_impl.c` | única TU com `*_IMPLEMENTATION` de stb |
| `src/geometry/geom_types.h` | `Contour`, `ContourSet`, `MeshData`, `MeshParams` (compartilhado) |
| `src/geometry/font_outline.h` / `.c` | GDI resolve a fonte → `stb_truetype` → `ContourSet` (multi-linha, kerning) |
| `third_party/libtess2/**` | Tesselador de polígonos com furos |
| `src/geometry/contour_mesh.h` / `.c` | `ContourSet` + `MeshParams{depth}` → `MeshData` (tampa frente/verso + paredes) |
| `src/material.h` / `.c` | Programa GL + locais de uniform + `material_draw(...)` (modo 0) |
| `src/scene.h` / `.c` | `SceneRenderer`: monta a malha (params fixos), câmera auto-fit, pêndulo, desenha; `M3DT_SHOT` |
| `src/host_win32.c` | (modif.) `gl_load()` após o contexto; `SceneRenderer` por janela; loop chama `scene_render` |
| `build/tests/test_mathx.c`, `test_contour_mesh.c`, `test_font_outline.c` | testes |
| `build/Makefile` | (modif.) regra do `embed`, novas fontes, `-Ithird_party ... -Igenerated`, libtess2 |

---

## Task 1: `mathx` — vetores, matrizes e o pêndulo (TDD)

**Files:**
- Create: `src/util/mathx.h`, `src/util/mathx.c`
- Create: `build/tests/test_mathx.c`
- Modify: `build/tests/test_main.c` (chamar `run_mathx_tests`)
- Modify: `build/Makefile` (`TEST_SRC += build/tests/test_mathx.c`, `TEST_UNITS += src/util/mathx.c`)

**Interfaces:**
- Consumes: nada.
- Produces:
  ```c
  typedef struct { float x, y; } v2;
  typedef struct { float x, y, z; } v3;
  typedef struct { float m[16]; } m4;   /* coluna-major: m[col*4 + row] */

  v3    v3_add(v3 a, v3 b);
  v3    v3_sub(v3 a, v3 b);
  v3    v3_scale(v3 a, float s);
  float v3_dot(v3 a, v3 b);
  v3    v3_cross(v3 a, v3 b);
  float v3_len(v3 a);
  v3    v3_norm(v3 a);

  m4 m4_identity(void);
  m4 m4_mul(m4 a, m4 b);                 /* a aplicado depois de b */
  m4 m4_perspective(float fovy_rad, float aspect, float znear, float zfar);
  m4 m4_look_at(v3 eye, v3 center, v3 up);
  m4 m4_translate(v3 t);
  m4 m4_rotate_x(float rad);
  m4 m4_rotate_y(float rad);
  v3 m4_mul_point(m4 a, v3 p);           /* p.w = 1, com divisão perspectiva */

  float m3dt_radians(float deg);
  /* ângulo (graus) do pêndulo em t (s); vai/volta suave entre -maxdeg e +maxdeg em `period` s */
  float pendulum_angle(float t, float period, float maxdeg);
  ```

- [ ] **Step 1: Escrever `build/tests/test_mathx.c` (falha: sem implementação)**

```c
#include "test.h"
#include "util/mathx.h"
#include <math.h>

static int near(float a, float b) { return fabsf(a - b) < 1e-4f; }

void run_mathx_tests(void)
{
    /* vetores */
    EXPECT(near(v3_len((v3){3, 4, 0}), 5.0f));
    v3 n = v3_norm((v3){0, 5, 0});
    EXPECT(near(n.y, 1.0f));
    v3 c = v3_cross((v3){1, 0, 0}, (v3){0, 1, 0});
    EXPECT(near(c.z, 1.0f));
    EXPECT(near(v3_dot((v3){1, 2, 3}, (v3){4, 5, 6}), 32.0f));

    /* identidade */
    m4 I = m4_identity();
    m4 II = m4_mul(I, I);
    for (int i = 0; i < 16; ++i) EXPECT(near(II.m[i], I.m[i]));

    /* rotate_y(90°): (1,0,0) -> (0,0,-1) */
    v3 r = m4_mul_point(m4_rotate_y(m3dt_radians(90.0f)), (v3){1, 0, 0});
    EXPECT(near(r.x, 0.0f) && near(r.y, 0.0f) && near(r.z, -1.0f));

    /* rotate_x(90°): (0,1,0) -> (0,0,1) */
    v3 rx = m4_mul_point(m4_rotate_x(m3dt_radians(90.0f)), (v3){0, 1, 0});
    EXPECT(near(rx.x, 0.0f) && near(rx.y, 0.0f) && near(rx.z, 1.0f));

    /* translate */
    v3 t = m4_mul_point(m4_translate((v3){2, -3, 4}), (v3){1, 1, 1});
    EXPECT(near(t.x, 3.0f) && near(t.y, -2.0f) && near(t.z, 5.0f));

    /* look_at: o olho vai para a origem no espaço de view */
    v3 e = m4_mul_point(m4_look_at((v3){0, 0, 10}, (v3){0, 0, 0}, (v3){0, 1, 0}), (v3){0, 0, 10});
    EXPECT(near(e.x, 0.0f) && near(e.y, 0.0f) && near(e.z, 0.0f));

    /* perspective: ponto no plano near mapeia para z_ndc ≈ -1 */
    m4 P = m4_perspective(m3dt_radians(60.0f), 1.0f, 1.0f, 100.0f);
    v3 pn = m4_mul_point(P, (v3){0, 0, -1.0f});
    EXPECT(near(pn.z, -1.0f));

    /* pêndulo */
    EXPECT(near(pendulum_angle(0.0f, 8.0f, 45.0f), -45.0f));
    EXPECT(near(pendulum_angle(4.0f, 8.0f, 45.0f), 45.0f));
    EXPECT(fabsf(pendulum_angle(2.0f, 8.0f, 45.0f)) < 0.5f);          /* meio do trajeto ≈ 0 */
    for (float tt = 0; tt < 40; tt += 0.13f)
        EXPECT(fabsf(pendulum_angle(tt, 8.0f, 45.0f)) <= 45.0f + 1e-3f);
    /* suavidade nos extremos: derivada numérica ≈ 0 perto de t=0 */
    float d = (pendulum_angle(0.02f, 8.0f, 45.0f) - pendulum_angle(0.0f, 8.0f, 45.0f)) / 0.02f;
    EXPECT(fabsf(d) < 2.0f);
    /* anti-simetria de meia-fase */
    EXPECT(near(pendulum_angle(1.0f, 8.0f, 45.0f), -pendulum_angle(5.0f, 8.0f, 45.0f)));
}
```

- [ ] **Step 2: Adicionar a chamada em `build/tests/test_main.c`**

```c
void run_cmdline_tests(void);
void run_log_tests(void);
void run_mathx_tests(void);
```
e no `main`, após `run_log_tests();`:
```c
    run_mathx_tests();
```

- [ ] **Step 3: Atualizar `build/Makefile`**

```make
TEST_SRC   := build/tests/test_main.c build/tests/test_cmdline.c build/tests/test_log.c \
              build/tests/test_mathx.c
TEST_UNITS := src/cmdline.c src/util/log.c src/util/mathx.c
```
E o alvo `test` ganha `-lm` no fim (link da libm para `sqrtf`/`cosf`):
```make
test:
	@mkdir -p build/obj
	$(CC) -std=c11 -Wall -Wextra -Isrc -Ibuild/tests \
	  $(TEST_SRC) $(TEST_UNITS) -o build/obj/run_tests.exe -lm
	./build/obj/run_tests.exe
```

- [ ] **Step 4: Rodar — confirmar falha de link**

```sh
mingw32-make -f build/Makefile test
```
Esperado: `undefined reference to 'v3_len'` etc.

- [ ] **Step 5: Escrever `src/util/mathx.h`**

```c
#ifndef M3DT_MATHX_H
#define M3DT_MATHX_H

typedef struct { float x, y; } v2;
typedef struct { float x, y, z; } v3;
typedef struct { float m[16]; } m4;   /* coluna-major: m[col*4 + row] */

v3    v3_add(v3 a, v3 b);
v3    v3_sub(v3 a, v3 b);
v3    v3_scale(v3 a, float s);
float v3_dot(v3 a, v3 b);
v3    v3_cross(v3 a, v3 b);
float v3_len(v3 a);
v3    v3_norm(v3 a);

m4 m4_identity(void);
m4 m4_mul(m4 a, m4 b);
m4 m4_perspective(float fovy_rad, float aspect, float znear, float zfar);
m4 m4_look_at(v3 eye, v3 center, v3 up);
m4 m4_translate(v3 t);
m4 m4_rotate_x(float rad);
m4 m4_rotate_y(float rad);
v3 m4_mul_point(m4 a, v3 p);

float m3dt_radians(float deg);
float pendulum_angle(float t, float period, float maxdeg);

#endif
```

- [ ] **Step 6: Escrever `src/util/mathx.c`**

```c
#include "util/mathx.h"
#include <math.h>

v3 v3_add(v3 a, v3 b)   { return (v3){ a.x + b.x, a.y + b.y, a.z + b.z }; }
v3 v3_sub(v3 a, v3 b)   { return (v3){ a.x - b.x, a.y - b.y, a.z - b.z }; }
v3 v3_scale(v3 a, float s) { return (v3){ a.x * s, a.y * s, a.z * s }; }
float v3_dot(v3 a, v3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
v3 v3_cross(v3 a, v3 b) {
    return (v3){ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
float v3_len(v3 a) { return sqrtf(v3_dot(a, a)); }
v3 v3_norm(v3 a) {
    float l = v3_len(a);
    return l > 1e-8f ? v3_scale(a, 1.0f / l) : (v3){ 0, 0, 0 };
}

m4 m4_identity(void) {
    m4 r = {{0}};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

m4 m4_mul(m4 a, m4 b) {
    m4 r = {{0}};
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}

m4 m4_perspective(float fovy_rad, float aspect, float znear, float zfar) {
    float f = 1.0f / tanf(fovy_rad * 0.5f);
    m4 r = {{0}};
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = (zfar + znear) / (znear - zfar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
    return r;
}

m4 m4_look_at(v3 eye, v3 center, v3 up) {
    v3 f = v3_norm(v3_sub(center, eye));
    v3 s = v3_norm(v3_cross(f, up));
    v3 u = v3_cross(s, f);
    m4 r = m4_identity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8]  = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9]  = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -v3_dot(s, eye);
    r.m[13] = -v3_dot(u, eye);
    r.m[14] = v3_dot(f, eye);
    return r;
}

m4 m4_translate(v3 t) {
    m4 r = m4_identity();
    r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
    return r;
}

m4 m4_rotate_x(float rad) {
    float c = cosf(rad), s = sinf(rad);
    m4 r = m4_identity();
    r.m[5] = c;  r.m[6] = s;
    r.m[9] = -s; r.m[10] = c;
    return r;
}

m4 m4_rotate_y(float rad) {
    float c = cosf(rad), s = sinf(rad);
    m4 r = m4_identity();
    r.m[0] = c;  r.m[2] = -s;
    r.m[8] = s;  r.m[10] = c;
    return r;
}

v3 m4_mul_point(m4 a, v3 p) {
    float x = a.m[0] * p.x + a.m[4] * p.y + a.m[8]  * p.z + a.m[12];
    float y = a.m[1] * p.x + a.m[5] * p.y + a.m[9]  * p.z + a.m[13];
    float z = a.m[2] * p.x + a.m[6] * p.y + a.m[10] * p.z + a.m[14];
    float w = a.m[3] * p.x + a.m[7] * p.y + a.m[11] * p.z + a.m[15];
    if (fabsf(w) > 1e-8f) { x /= w; y /= w; z /= w; }
    return (v3){ x, y, z };
}

float m3dt_radians(float deg) { return deg * 3.14159265358979323846f / 180.0f; }

float pendulum_angle(float t, float period, float maxdeg) {
    if (period <= 1e-4f) return 0.0f;
    float u = fmodf(t / period, 1.0f);
    if (u < 0.0f) u += 1.0f;
    /* onda triangular: -1 em u=0, +1 em u=0.5, -1 em u=1 */
    float tri = (u < 0.5f) ? (u * 4.0f - 1.0f) : (3.0f - u * 4.0f);
    float s = (tri + 1.0f) * 0.5f;                       /* 0..1 */
    s = s * s * s * (s * (s * 6.0f - 15.0f) + 10.0f);    /* smootherstep -> C2 nos extremos */
    return (s * 2.0f - 1.0f) * maxdeg;
}
```

- [ ] **Step 7: Rodar — confirmar que passa**

```sh
mingw32-make -f build/Makefile test
```
Esperado: `all tests passed`.

- [ ] **Step 8: Commit**

```sh
git add src/util/mathx.h src/util/mathx.c build/tests/test_mathx.c build/tests/test_main.c build/Makefile
git commit -m "$(printf 'feat: mathx - vec/mat helpers and the bounded pendulum curve\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 2: `glad` + `gl_core` — OpenGL 3.3 core moderno

**Files:**
- Create: `third_party/glad/**` (gerado, commitado)
- Create: `src/gl_core.h`, `src/gl_core.c`
- Modify: `build/Makefile` (adicionar `third_party/glad/src/gl.c` e `src/gl_core.c` a `SRC`; includes `-Ithird_party/glad/include -Ithird_party`)
- Modify: `src/host_win32.c` (trocar `#include <GL/gl.h>` por `#include <glad/gl.h>`; chamar `gl_load()` após o 1º `wglMakeCurrent` do contexto real)

**Interfaces:**
- Consumes: `log_*`.
- Produces:
  ```c
  typedef struct { float px, py, pz, nx, ny, nz, surf; } MeshVertex;   /* 7 floats */
  typedef struct { unsigned vao, vbo, ebo; int index_count; } GlMesh;

  int    gl_load(void);                                  /* 0 = falha */
  unsigned gl_program(const char *vs_src, const char *fs_src);   /* 0 = falha, loga o erro */
  GlMesh gl_mesh_upload(const MeshVertex *v, int nverts, const unsigned *idx, int nidx);
  void   gl_mesh_draw(const GlMesh *m);
  void   gl_mesh_free(GlMesh *m);
  ```

- [ ] **Step 1: Gerar o glad (uma vez) com o Python da máquina**

```sh
python -m pip install --user glad2
python -m glad --api gl:core=3.3 --out-path third_party/glad c
# se "python -m glad" não existir, use o script instalado:
#   "$APPDATA/Python/Python314/Scripts/glad" --api gl:core=3.3 --out-path third_party/glad c
ls third_party/glad/include/glad/gl.h third_party/glad/include/KHR/khrplatform.h third_party/glad/src/gl.c
```
Esperado: os 3 arquivos existem. Se não houver internet/pip, baixar a versão gerada do glad web service (gl 3.3, profile core, generator c) e colocar nos mesmos caminhos.

- [ ] **Step 2: Escrever `src/gl_core.h`**

```c
#ifndef M3DT_GL_CORE_H
#define M3DT_GL_CORE_H

typedef struct { float px, py, pz, nx, ny, nz, surf; } MeshVertex;
typedef struct { unsigned vao, vbo, ebo; int index_count; } GlMesh;

int      gl_load(void);
unsigned gl_program(const char *vs_src, const char *fs_src);
GlMesh   gl_mesh_upload(const MeshVertex *v, int nverts, const unsigned *idx, int nidx);
void     gl_mesh_draw(const GlMesh *m);
void     gl_mesh_free(GlMesh *m);

#endif
```

- [ ] **Step 3: Escrever `src/gl_core.c`**

```c
#include "gl_core.h"
#include "util/log.h"

#include <glad/gl.h>
#include <windows.h>
#include <stdlib.h>

static GLADapiproc gl_get_proc(const char *name)
{
    PROC p = wglGetProcAddress(name);
    /* wglGetProcAddress devolve 0, 1, 2, 3 ou -1 para funções do core 1.1 */
    if (p == NULL || p == (PROC)0x1 || p == (PROC)0x2 || p == (PROC)0x3 || p == (PROC)(INT_PTR)-1) {
        static HMODULE gl = NULL;
        if (!gl) gl = LoadLibraryA("opengl32.dll");
        p = gl ? GetProcAddress(gl, name) : NULL;
    }
    return (GLADapiproc)p;
}

int gl_load(void)
{
    int v = gladLoadGL(gl_get_proc);
    if (!v) { log_errorf("gladLoadGL falhou"); return 0; }
    log_infof("glad: GL %d.%d", GLAD_VERSION_MAJOR(v), GLAD_VERSION_MINOR(v));
    return 1;
}

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
        log_errorf("shader %s: %s", type == GL_VERTEX_SHADER ? "vert" : "frag", buf);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

unsigned gl_program(const char *vs_src, const char *fs_src)
{
    unsigned vs = compile(GL_VERTEX_SHADER, vs_src);
    unsigned fs = compile(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) { if (vs) glDeleteShader(vs); if (fs) glDeleteShader(fs); return 0; }

    unsigned p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
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

GlMesh gl_mesh_upload(const MeshVertex *v, int nverts, const unsigned *idx, int nidx)
{
    GlMesh m = { 0, 0, 0, 0 };
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);

    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(nverts * (int)sizeof(MeshVertex)), v, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(nidx * (int)sizeof(unsigned)), idx, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *)(6 * sizeof(float)));

    glBindVertexArray(0);
    m.index_count = nidx;
    return m;
}

void gl_mesh_draw(const GlMesh *m)
{
    glBindVertexArray(m->vao);
    glDrawElements(GL_TRIANGLES, m->index_count, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void gl_mesh_free(GlMesh *m)
{
    if (m->ebo) glDeleteBuffers(1, &m->ebo);
    if (m->vbo) glDeleteBuffers(1, &m->vbo);
    if (m->vao) glDeleteVertexArrays(1, &m->vao);
    m->vao = m->vbo = m->ebo = 0;
    m->index_count = 0;
}
```

- [ ] **Step 4: Atualizar `build/Makefile`**

```make
CFLAGS := -std=c11 -municode -Wall -Wextra -Isrc -Ithird_party -Ithird_party/glad/include -Igenerated
...
SRC := src/main.c src/cmdline.c src/util/log.c src/util/mathx.c src/gl_core.c \
       src/host_win32.c src/config_dialog.c third_party/glad/src/gl.c
```
Regra de objeto para `third_party/`:
```make
build/obj/%.o: third_party/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wno-pedantic -c $< -o $@
```
(e ajustar `OBJ` para cobrir os dois prefixos)
```make
OBJ := $(patsubst src/%.c,build/obj/src/%.o,$(filter src/%,$(SRC))) \
       $(patsubst third_party/%.c,build/obj/tp/%.o,$(filter third_party/%,$(SRC)))
```
com regras:
```make
build/obj/src/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
build/obj/tp/%.o: third_party/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -w -c $< -o $@        # libs de terceiros: sem -Wall/-Wextra
```

- [ ] **Step 5: Atualizar `src/host_win32.c` para carregar o glad**

- Trocar `#include <GL/gl.h>` por `#include <glad/gl.h>`.
- Em `m3dt_wgl_bootstrap`, o `glGetString` da linha de log já funciona (GL 1.1). Sem mudança.
- Em `gl_window_create`, logo após `wglMakeCurrent(g->dc, g->rc);` e o `wglSwapIntervalEXT`, adicionar:
  ```c
  if (!gl_load()) { log_errorf("gl_load falhou"); }
  ```
  (incluir `#include "gl_core.h"` no topo). Chamar `gl_load()` uma vez é suficiente (ponteiros globais); chamadas repetidas são inócuas.
- `gl_render_clear` continua igual por ora (é removido no Task 7). Ele usa `glViewport/glClearColor/glClear` — todos presentes no `glad/gl.h`.

- [ ] **Step 6: Verificação headless — compilar um shader trivial e desenhar um triângulo**

Adicionar temporariamente ao fim de `src/gl_core.c`:
```c
int gl_core_smoke(void);
int gl_core_smoke(void)
{
    static const char *VS =
        "#version 330 core\nlayout(location=0) in vec3 p;\nvoid main(){gl_Position=vec4(p,1.0);}\n";
    static const char *FS =
        "#version 330 core\nout vec4 c;\nvoid main(){c=vec4(1.0,0.5,0.2,1.0);}\n";
    unsigned prog = gl_program(VS, FS);
    if (!prog) return 1;

    MeshVertex tri[3] = {
        { -0.5f, -0.5f, 0, 0, 0, 1, 0 },
        {  0.5f, -0.5f, 0, 0, 0, 1, 0 },
        {  0.0f,  0.5f, 0, 0, 0, 1, 0 },
    };
    unsigned idx[3] = { 0, 1, 2 };
    GlMesh m = gl_mesh_upload(tri, 3, idx, 3);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    gl_mesh_draw(&m);

    GLenum e = glGetError();
    gl_mesh_free(&m);
    glDeleteProgram(prog);
    if (e != GL_NO_ERROR) { log_errorf("gl_core_smoke glError=0x%04x", (unsigned)e); return 1; }
    log_infof("gl_core_smoke: OK");
    return 0;
}
```
Substituir temporariamente o corpo de `wWinMain` em `src/main.c` por:
```c
    log_init();
    extern int m3dt_task4_gl_probe(HINSTANCE);   /* helper abaixo */
    int rc = m3dt_task4_gl_probe(hInst);
    log_shutdown();
    return rc;
```
e adicionar em `src/host_win32.c` (temporário, removido no Step 8):
```c
int m3dt_task4_gl_probe(HINSTANCE hInst)
{
    m3dt_set_dpi_aware();
    m3dt_wgl_bootstrap(hInst);
    GlWindow g;
    if (!gl_window_create(hInst, &g, WS_OVERLAPPEDWINDOW, 0, NULL, 0, 0, 320, 200, L"M3DTGLProbe", NULL))
        return 1;
    extern int gl_core_smoke(void);
    int rc = gl_core_smoke();
    gl_window_destroy(&g);
    return rc;
}
```
Buildar e rodar (via cópia `.exe`, invocação direta):
```sh
mingw32-make -f build/Makefile debug
cp dist/Modern3DText.scr "$TEMP/m3dt_p.exe" && "$TEMP/m3dt_p.exe"; echo "exit=$?"
cat "$LOCALAPPDATA/Modern3DText/log.txt"
```
Esperado: `exit=0`, log com `glad: GL 3.3`, `gl_core_smoke: OK`.

- [ ] **Step 7: Rodar os testes unitários**

```sh
mingw32-make -f build/Makefile test
```
Esperado: `all tests passed` (nada novo, só garantir que o Makefile ainda compila os testes).

- [ ] **Step 8: Remover os smokes temporários**

Apagar `gl_core_smoke` de `src/gl_core.c` e `m3dt_task4_gl_probe` de `src/host_win32.c`. Restaurar `src/main.c` para a versão da Fase 1 (log_init → parse → dispatch → log_shutdown; ver `git show f2c3619:src/main.c`).

- [ ] **Step 9: Buildar release + checagem de tamanho**

```sh
mingw32-make -f build/Makefile clean && mingw32-make -f build/Makefile
```
Esperado: `Built dist/Modern3DText.scr (<N> bytes)` com `N < 3145728` (o glad adiciona ~100–200 KB).

- [ ] **Step 10: Commit**

```sh
git add third_party/glad src/gl_core.h src/gl_core.c src/host_win32.c src/main.c build/Makefile
git commit -m "$(printf 'feat: glad GL 3.3 core loader + gl_core shader/mesh helpers\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 3: `embed` + shaders do modelo

**Files:**
- Create: `build/tools/embed.c`
- Create: `shaders/model.vert`, `shaders/model.frag`
- Modify: `build/Makefile` (regra `generated/embedded.h`; `$(OBJ)` depende dela)

**Interfaces:**
- Consumes: nada.
- Produces: `generated/embedded.h` com, para cada arquivo `X/Y.ext`, um `static const unsigned char EMBED_Y_ext[]` (terminado em `\0`) e `static const int EMBED_Y_ext_len` (sem contar o `\0`). Ex.: `shaders/model.vert` → `EMBED_model_vert`, `EMBED_model_vert_len`.

- [ ] **Step 1: Escrever `build/tools/embed.c`**

```c
/* embed <saida.h> <arquivo> [arquivo...]
   Gera um header com o conteudo de cada arquivo como array de bytes (com \0 final). */
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static void sym_from_path(const char *path, char *out, int n)
{
    const char *base = path;
    for (const char *p = path; *p; ++p) if (*p == '/' || *p == '\\') base = p + 1;
    int j = 0;
    for (const char *p = base; *p && j < n - 1; ++p)
        out[j++] = (isalnum((unsigned char)*p)) ? *p : '_';
    out[j] = 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "uso: embed saida.h arquivo...\n"); return 2; }
    FILE *o = fopen(argv[1], "wb");
    if (!o) { fprintf(stderr, "nao abriu %s\n", argv[1]); return 1; }

    fprintf(o, "/* AUTO-GERADO por build/tools/embed.c - nao editar */\n#ifndef M3DT_EMBEDDED_H\n#define M3DT_EMBEDDED_H\n\n");

    for (int a = 2; a < argc; ++a) {
        FILE *f = fopen(argv[a], "rb");
        if (!f) { fprintf(stderr, "nao abriu %s\n", argv[a]); fclose(o); return 1; }
        char sym[128];
        sym_from_path(argv[a], sym, sizeof sym);
        fprintf(o, "static const unsigned char EMBED_%s[] = {\n", sym);
        int c, n = 0, col = 0;
        while ((c = fgetc(f)) != EOF) {
            fprintf(o, "%d,", c);
            n++;
            if (++col == 20) { fputc('\n', o); col = 0; }
        }
        fprintf(o, "0 };\n");                 /* \0 final */
        fprintf(o, "static const int EMBED_%s_len = %d;\n\n", sym, n);
        fclose(f);
    }

    fprintf(o, "#endif\n");
    fclose(o);
    return 0;
}
```

- [ ] **Step 2: Escrever `shaders/model.vert`**

```glsl
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in float aSurf;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vWorld;
out vec3 vNrm;
out float vSurf;

void main()
{
    vec4 w = uModel * vec4(aPos, 1.0);
    vWorld = w.xyz;
    vNrm = mat3(uModel) * aNrm;
    vSurf = aSurf;
    gl_Position = uProj * uView * w;
}
```

- [ ] **Step 3: Escrever `shaders/model.frag` (material modo 0 — especular clássico)**

```glsl
#version 330 core
in vec3 vWorld;
in vec3 vNrm;
in float vSurf;

uniform vec3 uCamPos;
uniform vec3 uBaseColor;

out vec4 fragColor;

const vec3 KEY_DIR  = normalize(vec3(-0.40, -0.75, -0.55));  // direcao da luz -> superficie
const vec3 FILL_DIR = normalize(vec3( 0.55,  0.30, -0.45));
const vec3 KEY_COL  = vec3(1.00, 0.96, 0.88);
const vec3 FILL_COL = vec3(0.55, 0.62, 0.80);
const vec3 RIM_COL  = vec3(0.55, 0.68, 0.95);

void main()
{
    vec3 N = normalize(vNrm);
    if (!gl_FrontFacing) N = -N;               // extrusao e two-sided
    vec3 V = normalize(uCamPos - vWorld);
    vec3 base = uBaseColor;

    float kd = max(dot(N, -KEY_DIR), 0.0);
    vec3 col = base * (0.12 + 0.88 * kd) * KEY_COL;
    col += base * max(dot(N, -FILL_DIR), 0.0) * 0.30 * FILL_COL;

    vec3 H = normalize(-KEY_DIR + V);
    float spec = pow(max(dot(N, H), 0.0), 96.0);
    col += vec3(1.0) * spec * 0.85;

    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    col += RIM_COL * rim * 0.35;

    // realce leve das quinas (paredes: surf=2) para dar contorno
    if (vSurf > 1.5 && vSurf < 2.5) col *= 0.92;

    fragColor = vec4(col, 1.0);
}
```

- [ ] **Step 4: Regras no `build/Makefile`**

```make
EMBED_INPUTS := shaders/model.vert shaders/model.frag

generated/embedded.h: build/tools/embed.c $(EMBED_INPUTS)
	@mkdir -p generated build/obj
	$(CC) -O2 -std=c11 build/tools/embed.c -o build/obj/embed.exe
	./build/obj/embed.exe generated/embedded.h $(EMBED_INPUTS)

# objetos que usam shaders dependem do header gerado
build/obj/src/material.o build/obj/src/scene.o: generated/embedded.h
```
Adicionar `generated/embedded.h` como pré-requisito de ordem no alvo raiz não é necessário se as dependências acima estiverem certas, mas para segurança inclua-o em `$(OUT)`:
```make
$(OUT): generated/embedded.h $(OBJ) $(RES)
```
E `clean` já remove `generated` (Fase 1).

- [ ] **Step 5: Gerar e conferir**

```sh
mingw32-make -f build/Makefile generated/embedded.h
head -c 300 generated/embedded.h
gcc -std=c11 -Igenerated -x c -c -o /dev/null - <<'EOF'
#include "embedded.h"
int main(void){ return EMBED_model_vert_len + EMBED_model_frag_len; }
EOF
echo "compila header: $?"
```
Esperado: `generated/embedded.h` começa com o comentário AUTO-GERADO e define `EMBED_model_vert`; o teste de compilação retorna 0.

- [ ] **Step 6: Commit**

```sh
git add build/tools/embed.c shaders/model.vert shaders/model.frag build/Makefile
git commit -m "$(printf 'feat: embed tool + model shaders (classic specular, mode 0)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 4: `font_outline` — fonte instalada → contornos

**Files:**
- Create: `third_party/stb_truetype.h` (baixado)
- Create: `src/geometry/geom_types.h`
- Create: `src/geometry/stb_impl.c`
- Create: `src/geometry/font_outline.h`, `src/geometry/font_outline.c`
- Create: `build/tests/test_font_outline.c`
- Modify: `build/tests/test_main.c`, `build/Makefile`

**Interfaces:**
- Consumes: `log_*`.
- Produces:
  ```c
  /* geom_types.h */
  typedef struct { v2 *pts; int count; } Contour;        /* fechado (fechamento implicito) */
  typedef struct {
      Contour *contours; int count;
      float minx, miny, maxx, maxy;                       /* bbox da uniao */
  } ContourSet;

  /* font_outline.h */
  int  font_build_contours(const char *utf8, const wchar_t *family, int bold, int italic,
                           float flatten_tol, ContourSet *out);   /* 0 = falha */
  void contourset_free(ContourSet *cs);
  ```
  Contornos em unidades de em (escala `1/unitsPerEm`), origem no centro da bbox, Y para cima.

- [ ] **Step 1: Baixar `stb_truetype.h`**

```sh
curl -sL -o third_party/stb_truetype.h https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h
grep -m1 "stb_truetype" third_party/stb_truetype.h
```

- [ ] **Step 2: `src/geometry/geom_types.h`**

```c
#ifndef M3DT_GEOM_TYPES_H
#define M3DT_GEOM_TYPES_H
#include "util/mathx.h"

typedef struct { v2 *pts; int count; } Contour;

typedef struct {
    Contour *contours;
    int      count;
    float    minx, miny, maxx, maxy;
} ContourSet;

typedef struct {
    MeshVertexTag _unused;   /* placeholder removido no Task 5 */
} MeshParams_placeholder;

#endif
```
> Nota: `MeshData`/`MeshParams` reais entram no Task 5 (em `contour_mesh.h`). Aqui só `Contour`/`ContourSet`. Remova o placeholder acima — deixe o header só com `Contour` e `ContourSet` e o `#include "util/mathx.h"`.

Versão final do arquivo:
```c
#ifndef M3DT_GEOM_TYPES_H
#define M3DT_GEOM_TYPES_H
#include "util/mathx.h"

typedef struct { v2 *pts; int count; } Contour;

typedef struct {
    Contour *contours;
    int      count;
    float    minx, miny, maxx, maxy;
} ContourSet;

#endif
```

- [ ] **Step 3: `src/geometry/stb_impl.c`**

```c
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
```
> `stb_image_write.h` é baixado no Task 7 Step 1; se este task rodar antes, baixe agora:
> `curl -sL -o third_party/stb_image_write.h https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h`

- [ ] **Step 4: `src/geometry/font_outline.h`**

```c
#ifndef M3DT_FONT_OUTLINE_H
#define M3DT_FONT_OUTLINE_H
#include "geometry/geom_types.h"
#include <wchar.h>

int  font_build_contours(const char *utf8, const wchar_t *family, int bold, int italic,
                         float flatten_tol, ContourSet *out);
void contourset_free(ContourSet *cs);

#endif
```

- [ ] **Step 5: Escrever `build/tests/test_font_outline.c` (falha primeiro)**

```c
#include "test.h"
#include "geometry/font_outline.h"
#include <math.h>

void run_font_outline_tests(void)
{
    ContourSet cs;

    /* 'A' em Arial: contorno externo + 1 furo => >= 2 contornos */
    EXPECT(font_build_contours("A", L"Arial", 0, 0, 0.01f, &cs) == 1);
    EXPECT(cs.count >= 2);
    EXPECT(cs.maxx > cs.minx && cs.maxy > cs.miny);
    contourset_free(&cs);

    /* string vazia => 0 contornos, sem crash */
    EXPECT(font_build_contours("", L"Arial", 0, 0, 0.01f, &cs) == 1);
    EXPECT(cs.count == 0);
    contourset_free(&cs);

    /* multi-linha: "A\nA" mais alto que "A" */
    ContourSet one, two;
    font_build_contours("A", L"Arial", 0, 0, 0.01f, &one);
    font_build_contours("A\nA", L"Arial", 0, 0, 0.01f, &two);
    EXPECT((two.maxy - two.miny) > (one.maxy - one.miny) * 1.5f);
    contourset_free(&one);
    contourset_free(&two);

    /* fonte inexistente => cai para uma fonte do sistema, ainda retorna geometria */
    EXPECT(font_build_contours("A", L"NaoExisteEssaFonte123", 0, 0, 0.01f, &cs) == 1);
    EXPECT(cs.count >= 1);
    contourset_free(&cs);
}
```

- [ ] **Step 6: Escrever `src/geometry/font_outline.c`**

```c
#include "geometry/font_outline.h"
#include "util/log.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "stb_truetype.h"

/* --- bytes do arquivo da fonte selecionada, via GDI --- */
static unsigned char *load_face_bytes(const wchar_t *family, int bold, int italic, DWORD *out_size)
{
    HDC dc = CreateCompatibleDC(NULL);
    LOGFONTW lf;
    memset(&lf, 0, sizeof lf);
    lf.lfHeight = -256;
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfItalic = (BYTE)(italic ? 1 : 0);
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    wcsncpy(lf.lfFaceName, family && family[0] ? family : L"Segoe UI", LF_FACESIZE - 1);

    HFONT font = CreateFontIndirectW(&lf);
    HGDIOBJ old = SelectObject(dc, font);

    DWORD size = GetFontData(dc, 0, 0, NULL, 0);
    if (size == GDI_ERROR || size == 0) {
        /* pode ser .ttc */
        const DWORD ttcf = 0x66637474; /* 'ttcf' little-endian */
        size = GetFontData(dc, ttcf, 0, NULL, 0);
    }
    unsigned char *buf = NULL;
    if (size != GDI_ERROR && size > 0) {
        buf = (unsigned char *)malloc(size);
        DWORD got = GetFontData(dc, 0, 0, buf, size);
        if (got == GDI_ERROR) got = GetFontData(dc, 0x66637474, 0, buf, size);
        if (got == GDI_ERROR) { free(buf); buf = NULL; }
        else *out_size = size;
    }

    SelectObject(dc, old);
    DeleteObject(font);
    DeleteDC(dc);

    if (!buf) {
        /* fallback duro: Segoe UI pelo arquivo */
        wchar_t path[MAX_PATH];
        UINT n = GetWindowsDirectoryW(path, MAX_PATH);
        if (n) {
            wcsncat(path, L"\\Fonts\\segoeui.ttf", MAX_PATH - n - 1);
            HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (h != INVALID_HANDLE_VALUE) {
                DWORD sz = GetFileSize(h, NULL), rd = 0;
                buf = (unsigned char *)malloc(sz);
                if (buf && ReadFile(h, buf, sz, &rd, NULL) && rd == sz) *out_size = sz;
                else { free(buf); buf = NULL; }
                CloseHandle(h);
            }
        }
    }
    return buf;
}

/* achata uma quadratica/cubica em segmentos; empurra pontos em `c` */
typedef struct { v2 *p; int n, cap; } PtBuf;
static void pb_push(PtBuf *b, float x, float y)
{
    if (b->n == b->cap) { b->cap = b->cap ? b->cap * 2 : 64; b->p = realloc(b->p, (size_t)b->cap * sizeof *b->p); }
    b->p[b->n++] = (v2){ x, y };
}
static void flat_quad(PtBuf *b, float x0, float y0, float cx, float cy, float x1, float y1, float tol)
{
    /* subdivisão adaptativa simples */
    float mx = (x0 + 2 * cx + x1) * 0.25f, my = (y0 + 2 * cy + y1) * 0.25f;
    float lx = (x0 + x1) * 0.5f, ly = (y0 + y1) * 0.5f;
    if ((mx - lx) * (mx - lx) + (my - ly) * (my - ly) <= tol * tol) { pb_push(b, x1, y1); return; }
    float ax = (x0 + cx) * 0.5f, ay = (y0 + cy) * 0.5f;
    float bx = (cx + x1) * 0.5f, by = (cy + y1) * 0.5f;
    float abx = (ax + bx) * 0.5f, aby = (ay + by) * 0.5f;
    flat_quad(b, x0, y0, ax, ay, abx, aby, tol);
    flat_quad(b, abx, aby, bx, by, x1, y1, tol);
}
static void flat_cubic(PtBuf *b, float x0, float y0, float c0x, float c0y,
                       float c1x, float c1y, float x1, float y1, float tol)
{
    float lx = (x0 + x1) * 0.5f, ly = (y0 + y1) * 0.5f;
    float mx = (x0 + 3 * c0x + 3 * c1x + x1) * 0.125f, my = (y0 + 3 * c0y + 3 * c1y + y1) * 0.125f;
    if ((mx - lx) * (mx - lx) + (my - ly) * (my - ly) <= tol * tol) { pb_push(b, x1, y1); return; }
    float ab_x = (x0 + c0x) * .5f, ab_y = (y0 + c0y) * .5f;
    float bc_x = (c0x + c1x) * .5f, bc_y = (c0y + c1y) * .5f;
    float cd_x = (c1x + x1) * .5f, cd_y = (c1y + y1) * .5f;
    float abc_x = (ab_x + bc_x) * .5f, abc_y = (ab_y + bc_y) * .5f;
    float bcd_x = (bc_x + cd_x) * .5f, bcd_y = (bc_y + cd_y) * .5f;
    float m_x = (abc_x + bcd_x) * .5f, m_y = (abc_y + bcd_y) * .5f;
    flat_cubic(b, x0, y0, ab_x, ab_y, abc_x, abc_y, m_x, m_y, tol);
    flat_cubic(b, m_x, m_y, bcd_x, bcd_y, cd_x, cd_y, x1, y1, tol);
}

int font_build_contours(const char *utf8, const wchar_t *family, int bold, int italic,
                        float flatten_tol, ContourSet *out)
{
    memset(out, 0, sizeof *out);

    DWORD fsize = 0;
    unsigned char *fbytes = load_face_bytes(family, bold, italic, &fsize);
    if (!fbytes) { log_errorf("font: nenhuma fonte carregada"); return 0; }

    stbtt_fontinfo fi;
    int off = stbtt_GetFontOffsetForIndex(fbytes, 0);
    if (!stbtt_InitFont(&fi, fbytes, off)) { free(fbytes); log_errorf("font: InitFont"); return 0; }

    int ascent, descent, linegap;
    stbtt_GetFontVMetrics(&fi, &ascent, &descent, &linegap);
    float unitsPerEm = (float)(ascent - descent);
    if (unitsPerEm < 1.0f) unitsPerEm = 2048.0f;
    float sc = 1.0f / unitsPerEm;
    float line_adv = (float)(ascent - descent + linegap) * sc;
    float tol_units = flatten_tol / sc;

    /* acumula contornos e a bbox em unidades de fonte, com pen (px,py) */
    Contour *cons = NULL; int ncons = 0, ccap = 0;
    float penx = 0.0f, peny = 0.0f;
    float bminx = 1e30f, bminy = 1e30f, bmaxx = -1e30f, bmaxy = -1e30f;

    /* decodifica UTF-8 simples */
    const unsigned char *s = (const unsigned char *)utf8;
    int prev_cp = 0;
    while (*s) {
        int cp = *s;
        int adv = 1;
        if (cp >= 0xF0) { cp = ((cp & 7) << 18) | ((s[1] & 63) << 12) | ((s[2] & 63) << 6) | (s[3] & 63); adv = 4; }
        else if (cp >= 0xE0) { cp = ((cp & 15) << 12) | ((s[1] & 63) << 6) | (s[2] & 63); adv = 3; }
        else if (cp >= 0xC0) { cp = ((cp & 31) << 6) | (s[1] & 63); adv = 2; }
        s += adv;

        if (cp == '\n') { penx = 0.0f; peny -= line_adv / sc; prev_cp = 0; continue; }

        if (prev_cp) penx += stbtt_GetCodepointKernAdvance(&fi, prev_cp, cp);

        stbtt_vertex *verts = NULL;
        int nv = stbtt_GetCodepointShape(&fi, cp, &verts);
        PtBuf cur = { 0 };
        for (int i = 0; i < nv; ++i) {
            stbtt_vertex *v = &verts[i];
            float vx = penx + v->x, vy = peny + v->y;
            if (v->type == STBTT_vmove) {
                if (cur.n >= 3) {
                    if (ncons == ccap) { ccap = ccap ? ccap * 2 : 8; cons = realloc(cons, (size_t)ccap * sizeof *cons); }
                    cons[ncons].pts = malloc((size_t)cur.n * sizeof(v2));
                    for (int k = 0; k < cur.n; ++k) {
                        cons[ncons].pts[k].x = cur.p[k].x * sc;
                        cons[ncons].pts[k].y = cur.p[k].y * sc;
                    }
                    cons[ncons].count = cur.n;
                    ncons++;
                }
                cur.n = 0;
                pb_push(&cur, vx, vy);
            } else if (v->type == STBTT_vline) {
                pb_push(&cur, vx, vy);
            } else if (v->type == STBTT_vcurve) {
                v2 p0 = cur.n ? cur.p[cur.n - 1] : (v2){ vx, vy };
                flat_quad(&cur, p0.x, p0.y, penx + v->cx, peny + v->cy, vx, vy, tol_units);
            } else if (v->type == STBTT_vcubic) {
                v2 p0 = cur.n ? cur.p[cur.n - 1] : (v2){ vx, vy };
                flat_cubic(&cur, p0.x, p0.y, penx + v->cx, peny + v->cy,
                           penx + v->cx1, peny + v->cy1, vx, vy, tol_units);
            }
            if (vx < bminx) bminx = vx; if (vx > bmaxx) bmaxx = vx;
            if (vy < bminy) bminy = vy; if (vy > bmaxy) bmaxy = vy;
        }
        if (cur.n >= 3) {
            if (ncons == ccap) { ccap = ccap ? ccap * 2 : 8; cons = realloc(cons, (size_t)ccap * sizeof *cons); }
            cons[ncons].pts = malloc((size_t)cur.n * sizeof(v2));
            for (int k = 0; k < cur.n; ++k) {
                cons[ncons].pts[k].x = cur.p[k].x * sc;
                cons[ncons].pts[k].y = cur.p[k].y * sc;
            }
            cons[ncons].count = cur.n;
            ncons++;
        }
        free(cur.p);
        stbtt_FreeShape(&fi, verts);

        int aw, lsb;
        stbtt_GetCodepointHMetrics(&fi, cp, &aw, &lsb);
        penx += aw;
        prev_cp = cp;
    }
    free(fbytes);

    /* centraliza na bbox */
    float cx = 0.0f, cy = 0.0f;
    if (bmaxx > bminx) {
        cx = (bminx + bmaxx) * 0.5f * sc;
        cy = (bminy + bmaxy) * 0.5f * sc;
        for (int i = 0; i < ncons; ++i)
            for (int k = 0; k < cons[i].count; ++k) { cons[i].pts[k].x -= cx; cons[i].pts[k].y -= cy; }
    }

    out->contours = cons;
    out->count = ncons;
    if (ncons > 0) {
        out->minx = bminx * sc - cx; out->maxx = bmaxx * sc - cx;
        out->miny = bminy * sc - cy; out->maxy = bmaxy * sc - cy;
    }
    return 1;
}

void contourset_free(ContourSet *cs)
{
    for (int i = 0; i < cs->count; ++i) free(cs->contours[i].pts);
    free(cs->contours);
    memset(cs, 0, sizeof *cs);
}
```

- [ ] **Step 7: `build/tests/test_main.c` + `build/Makefile`**

`test_main.c`: declarar e chamar `run_font_outline_tests();`.

`Makefile`: o alvo `test` precisa de `windows.h` + GDI para `font_outline.c` e `stb_impl.c`. Ajustar:
```make
TEST_SRC   := build/tests/test_main.c build/tests/test_cmdline.c build/tests/test_log.c \
              build/tests/test_mathx.c build/tests/test_font_outline.c
TEST_UNITS := src/cmdline.c src/util/log.c src/util/mathx.c \
              src/geometry/font_outline.c src/geometry/stb_impl.c
test:
	@mkdir -p build/obj
	$(CC) -std=c11 -Wall -Wextra -Isrc -Ithird_party -Ibuild/tests \
	  $(TEST_SRC) $(TEST_UNITS) -o build/obj/run_tests.exe -lm -lgdi32 -luser32
	./build/obj/run_tests.exe
```
E adicionar `src/geometry/font_outline.c src/geometry/stb_impl.c` a `SRC` do app (o `stb_impl.c` compila com `-w` — é código de terceiros na prática; adicione um alvo específico ou mova para `third_party/`. Mais simples: compilar `stb_impl.c` com `-w` via regra dedicada:)
```make
build/obj/src/geometry/stb_impl.o: src/geometry/stb_impl.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -w -c $< -o $@
```

- [ ] **Step 8: Rodar — falha, depois passa**

```sh
mingw32-make -f build/Makefile test   # 1o: erros de link (esperado)
# apos escrever font_outline.c:
mingw32-make -f build/Makefile test   # esperado: all tests passed
```

- [ ] **Step 9: Commit**

```sh
git add third_party/stb_truetype.h src/geometry/geom_types.h src/geometry/stb_impl.c \
        src/geometry/font_outline.h src/geometry/font_outline.c \
        build/tests/test_font_outline.c build/tests/test_main.c build/Makefile
git commit -m "$(printf 'feat: font_outline - GDI face resolution + stb_truetype glyph contours\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 5: `contour_mesh` — contornos → malha 3D (TDD)

**Files:**
- Create: `third_party/libtess2/**` (baixado)
- Create: `src/geometry/contour_mesh.h`, `src/geometry/contour_mesh.c`
- Create: `build/tests/test_contour_mesh.c`
- Modify: `build/tests/test_main.c`, `build/Makefile`

**Interfaces:**
- Consumes: `ContourSet` (Task 4), `MeshVertex` (Task 2).
- Produces:
  ```c
  typedef struct { float depth; } MeshParams;
  typedef struct {
      MeshVertex *verts; int nverts;
      unsigned   *idx;   int nidx;
      float minx, miny, minz, maxx, maxy, maxz;
  } MeshData;

  int  contour_mesh_build(const ContourSet *cs, MeshParams p, MeshData *out);   /* 0 = falha/vazio */
  void mesh_data_free(MeshData *m);
  ```
  Malha = tampa frontal (z=+depth/2, normal +Z, surf=0) + tampa traseira (z=-depth/2, normal -Z, surf=1) + paredes (normal da aresta para fora, surf=2). Sem bevel.

- [ ] **Step 1: Baixar libtess2**

```sh
mkdir -p third_party/libtess2
curl -sL https://github.com/memononen/libtess2/archive/refs/heads/master.tar.gz | tar xz -C /tmp
cp -r /tmp/libtess2-master/Include third_party/libtess2/Include
cp -r /tmp/libtess2-master/Source  third_party/libtess2/Source
ls third_party/libtess2/Include/tesselator.h third_party/libtess2/Source/tess.c
```

- [ ] **Step 2: Escrever `build/tests/test_contour_mesh.c` (falha primeiro)**

```c
#include "test.h"
#include "geometry/contour_mesh.h"
#include <math.h>
#include <stdlib.h>

static ContourSet cs_make(int nc)
{
    ContourSet cs; cs.count = nc; cs.contours = calloc((size_t)nc, sizeof(Contour));
    cs.minx = cs.miny = -1; cs.maxx = cs.maxy = 1;
    return cs;
}
static void cs_set(ContourSet *cs, int i, const float *xy, int npts)
{
    cs->contours[i].count = npts;
    cs->contours[i].pts = malloc((size_t)npts * sizeof(v2));
    for (int k = 0; k < npts; ++k) cs->contours[i].pts[k] = (v2){ xy[k*2], xy[k*2+1] };
}
static void cs_free(ContourSet *cs)
{
    for (int i = 0; i < cs->count; ++i) free(cs->contours[i].pts);
    free(cs->contours);
}

void run_contour_mesh_tests(void)
{
    /* quadrado unitário */
    {
        ContourSet cs = cs_make(1);
        float sq[] = { -1,-1,  1,-1,  1,1,  -1,1 };
        cs_set(&cs, 0, sq, 4);
        MeshData md;
        EXPECT(contour_mesh_build(&cs, (MeshParams){ 0.5f }, &md) == 1);
        EXPECT(md.nverts > 0 && md.nidx > 0 && md.nidx % 3 == 0);
        /* bounds em z = ±0.25 */
        EXPECT(fabsf(md.minz + 0.25f) < 1e-4f && fabsf(md.maxz - 0.25f) < 1e-4f);
        /* toda normal de tampa aponta ±Z; toda normal de parede tem |nz| ~ 0 */
        int caps = 0, walls = 0;
        for (int i = 0; i < md.nverts; ++i) {
            const MeshVertex *v = &md.verts[i];
            if (v->surf < 1.5f) { EXPECT(fabsf(fabsf(v->nz) - 1.0f) < 1e-3f); caps++; }
            else { EXPECT(fabsf(v->nz) < 1e-3f); walls++; }
        }
        EXPECT(caps > 0 && walls > 0);
        mesh_data_free(&md);
        cs_free(&cs);
    }

    /* quadrado com furo => ainda produz malha (anel) sem crash */
    {
        ContourSet cs = cs_make(2);
        float outer[] = { -2,-2,  2,-2,  2,2,  -2,2 };
        float inner[] = { -1,1,  1,1,  1,-1,  -1,-1 };   /* winding oposto */
        cs_set(&cs, 0, outer, 4);
        cs_set(&cs, 1, inner, 4);
        MeshData md;
        EXPECT(contour_mesh_build(&cs, (MeshParams){ 0.4f }, &md) == 1);
        EXPECT(md.nidx % 3 == 0 && md.nverts > 0);
        mesh_data_free(&md);
        cs_free(&cs);
    }

    /* "L" côncavo */
    {
        ContourSet cs = cs_make(1);
        float L[] = { 0,0,  2,0,  2,1,  1,1,  1,3,  0,3 };
        cs_set(&cs, 0, L, 6);
        MeshData md;
        EXPECT(contour_mesh_build(&cs, (MeshParams){ 0.3f }, &md) == 1);
        EXPECT(md.nidx % 3 == 0);
        mesh_data_free(&md);
        cs_free(&cs);
    }

    /* degenerado: contorno com 2 pontos => malha vazia, retorno 0, sem crash */
    {
        ContourSet cs = cs_make(1);
        float two[] = { 0,0, 1,1 };
        cs_set(&cs, 0, two, 2);
        MeshData md;
        int r = contour_mesh_build(&cs, (MeshParams){ 0.3f }, &md);
        EXPECT(r == 0 || md.nidx == 0);
        if (r == 1) mesh_data_free(&md);
        cs_free(&cs);
    }
}
```

- [ ] **Step 3: `src/geometry/contour_mesh.h`**

```c
#ifndef M3DT_CONTOUR_MESH_H
#define M3DT_CONTOUR_MESH_H
#include "geometry/geom_types.h"
#include "gl_core.h"

typedef struct { float depth; } MeshParams;

typedef struct {
    MeshVertex *verts; int nverts;
    unsigned   *idx;   int nidx;
    float minx, miny, minz, maxx, maxy, maxz;
} MeshData;

int  contour_mesh_build(const ContourSet *cs, MeshParams p, MeshData *out);
void mesh_data_free(MeshData *m);

#endif
```

- [ ] **Step 4: `src/geometry/contour_mesh.c`**

```c
#include "geometry/contour_mesh.h"
#include "util/log.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "tesselator.h"

typedef struct { MeshVertex *v; int n, cap; } VBuf;
typedef struct { unsigned *i; int n, cap; } IBuf;

static void vpush(VBuf *b, MeshVertex mv)
{
    if (b->n == b->cap) { b->cap = b->cap ? b->cap * 2 : 256; b->v = realloc(b->v, (size_t)b->cap * sizeof *b->v); }
    b->v[b->n++] = mv;
}
static void ipush(IBuf *b, unsigned x)
{
    if (b->n == b->cap) { b->cap = b->cap ? b->cap * 2 : 512; b->i = realloc(b->i, (size_t)b->cap * sizeof *b->i); }
    b->i[b->n++] = x;
}

int contour_mesh_build(const ContourSet *cs, MeshParams p, MeshData *out)
{
    memset(out, 0, sizeof *out);
    if (cs->count == 0) return 0;

    /* contornos válidos? */
    int total_pts = 0;
    for (int i = 0; i < cs->count; ++i) {
        if (cs->contours[i].count >= 3) total_pts += cs->contours[i].count;
    }
    if (total_pts < 3) return 0;

    const float hz = p.depth * 0.5f;

    TESStesselator *t = tessNewTess(NULL);
    for (int i = 0; i < cs->count; ++i) {
        if (cs->contours[i].count < 3) continue;
        tessAddContour(t, 2, cs->contours[i].pts, (int)sizeof(v2), cs->contours[i].count);
    }
    if (!tessTesselate(t, TESS_WINDING_NONZERO, TESS_POLYGONS, 3, 2, NULL)) {
        tessDeleteTess(t);
        return 0;
    }

    const float *tv = tessGetVertices(t);
    const int   *te = tessGetElements(t);
    int nte = tessGetElementCount(t);

    VBuf vb = { 0 };
    IBuf ib = { 0 };

    /* --- tampa frontal (z = +hz, normal +Z, surf 0) e traseira (z = -hz, -Z, surf 1) --- */
    for (int e = 0; e < nte; ++e) {
        int a = te[e * 3 + 0], b = te[e * 3 + 1], c = te[e * 3 + 2];
        if (a == TESS_UNDEF || b == TESS_UNDEF || c == TESS_UNDEF) continue;
        float ax = tv[a * 2], ay = tv[a * 2 + 1];
        float bx = tv[b * 2], by = tv[b * 2 + 1];
        float cx = tv[c * 2], cy = tv[c * 2 + 1];

        unsigned base = (unsigned)vb.n;
        vpush(&vb, (MeshVertex){ ax, ay, hz, 0, 0, 1, 0 });
        vpush(&vb, (MeshVertex){ bx, by, hz, 0, 0, 1, 0 });
        vpush(&vb, (MeshVertex){ cx, cy, hz, 0, 0, 1, 0 });
        ipush(&ib, base + 0); ipush(&ib, base + 1); ipush(&ib, base + 2);

        unsigned bb = (unsigned)vb.n;
        vpush(&vb, (MeshVertex){ ax, ay, -hz, 0, 0, -1, 1 });
        vpush(&vb, (MeshVertex){ bx, by, -hz, 0, 0, -1, 1 });
        vpush(&vb, (MeshVertex){ cx, cy, -hz, 0, 0, -1, 1 });
        ipush(&ib, bb + 0); ipush(&ib, bb + 2); ipush(&ib, bb + 1);   /* winding invertido */
    }

    /* --- paredes: para cada contorno de entrada, um quad por aresta --- */
    for (int ci = 0; ci < cs->count; ++ci) {
        const Contour *co = &cs->contours[ci];
        if (co->count < 3) continue;
        for (int k = 0; k < co->count; ++k) {
            v2 p0 = co->pts[k];
            v2 p1 = co->pts[(k + 1) % co->count];
            float ex = p1.x - p0.x, ey = p1.y - p0.y;
            float el = sqrtf(ex * ex + ey * ey);
            if (el < 1e-9f) continue;
            /* normal 2D à direita da aresta (para contorno CCW aponta para fora) */
            float nx = ey / el, ny = -ex / el;

            unsigned base = (unsigned)vb.n;
            vpush(&vb, (MeshVertex){ p0.x, p0.y,  hz, nx, ny, 0, 2 });
            vpush(&vb, (MeshVertex){ p1.x, p1.y,  hz, nx, ny, 0, 2 });
            vpush(&vb, (MeshVertex){ p1.x, p1.y, -hz, nx, ny, 0, 2 });
            vpush(&vb, (MeshVertex){ p0.x, p0.y, -hz, nx, ny, 0, 2 });
            ipush(&ib, base + 0); ipush(&ib, base + 1); ipush(&ib, base + 2);
            ipush(&ib, base + 0); ipush(&ib, base + 2); ipush(&ib, base + 3);
        }
    }

    tessDeleteTess(t);

    if (vb.n == 0 || ib.n == 0) { free(vb.v); free(ib.i); return 0; }

    /* bounds */
    float mnx = 1e30f, mny = 1e30f, mnz = 1e30f, mxx = -1e30f, mxy = -1e30f, mxz = -1e30f;
    for (int i = 0; i < vb.n; ++i) {
        MeshVertex *v = &vb.v[i];
        if (v->px < mnx) mnx = v->px; if (v->px > mxx) mxx = v->px;
        if (v->py < mny) mny = v->py; if (v->py > mxy) mxy = v->py;
        if (v->pz < mnz) mnz = v->pz; if (v->pz > mxz) mxz = v->pz;
    }

    out->verts = vb.v; out->nverts = vb.n;
    out->idx = ib.i;   out->nidx = ib.n;
    out->minx = mnx; out->miny = mny; out->minz = mnz;
    out->maxx = mxx; out->maxy = mxy; out->maxz = mxz;
    return 1;
}

void mesh_data_free(MeshData *m)
{
    free(m->verts); free(m->idx);
    memset(m, 0, sizeof *m);
}
```

- [ ] **Step 5: `build/Makefile` — libtess2 + testes**

```make
LIBTESS_SRC := $(wildcard third_party/libtess2/Source/*.c)
LIBTESS_OBJ := $(patsubst third_party/%.c,build/obj/tp/%.o,$(LIBTESS_SRC))
CFLAGS += -Ithird_party/libtess2/Include

SRC := ... src/geometry/contour_mesh.c ...
# adicionar $(LIBTESS_OBJ) na regra de link do $(OUT):
$(OUT): generated/embedded.h $(OBJ) $(LIBTESS_OBJ) $(RES)
	@mkdir -p dist
	$(CC) $(OBJ) $(LIBTESS_OBJ) $(RES) -o $@ $(LDFLAGS) $(LIBS)
	...

build/obj/tp/%.o: third_party/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -w -c $< -o $@

# testes
TEST_SRC   += build/tests/test_contour_mesh.c
TEST_UNITS += src/geometry/contour_mesh.c
test:
	@mkdir -p build/obj
	$(CC) -std=c11 -Wall -Wextra -Isrc -Ithird_party -Ithird_party/libtess2/Include -Ibuild/tests \
	  $(TEST_SRC) $(TEST_UNITS) $(LIBTESS_SRC) -o build/obj/run_tests.exe -lm -lgdi32 -luser32
	./build/obj/run_tests.exe
```

- [ ] **Step 6: Rodar — falha, depois passa**

```sh
mingw32-make -f build/Makefile test
```
Esperado no fim: `all tests passed`.

- [ ] **Step 7: Commit**

```sh
git add third_party/libtess2 src/geometry/contour_mesh.h src/geometry/contour_mesh.c \
        build/tests/test_contour_mesh.c build/tests/test_main.c build/Makefile
git commit -m "$(printf 'feat: contour_mesh - libtess2 cap triangulation + extruded walls\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 6: `material` + `scene` — câmera, pêndulo, desenho

**Files:**
- Create: `src/material.h`, `src/material.c`
- Create: `src/scene.h`, `src/scene.c`
- Modify: `build/Makefile` (`SRC += src/material.c src/scene.c`)

**Interfaces:**
- Consumes: `gl_core`, `mathx`, `font_outline`, `contour_mesh`, `EMBED_model_*` de `generated/embedded.h`.
- Produces:
  ```c
  /* material.h */
  typedef struct {
      unsigned prog;
      int uModel, uView, uProj, uCamPos, uBaseColor;
  } Material;
  int  material_init(Material *m);                                        /* compila do embedded */
  void material_begin(const Material *m, m4 view, m4 proj, v3 campos, v3 base);
  void material_set_model(const Material *m, m4 model);
  void material_destroy(Material *m);

  /* scene.h */
  typedef struct SceneRenderer SceneRenderer;
  SceneRenderer *scene_create(void);         /* NULL em falha; usa params fixos da Fase 2a */
  void scene_render(SceneRenderer *s, double t, int fb_w, int fb_h);
  void scene_destroy(SceneRenderer *s);
  ```

- [ ] **Step 1: `src/material.h`** — conforme o bloco Interfaces acima.

- [ ] **Step 2: `src/material.c`**

```c
#include "material.h"
#include "gl_core.h"
#include "util/log.h"
#include <glad/gl.h>
#include "embedded.h"

int material_init(Material *m)
{
    m->prog = gl_program((const char *)EMBED_model_vert, (const char *)EMBED_model_frag);
    if (!m->prog) return 0;
    m->uModel     = glGetUniformLocation(m->prog, "uModel");
    m->uView      = glGetUniformLocation(m->prog, "uView");
    m->uProj      = glGetUniformLocation(m->prog, "uProj");
    m->uCamPos    = glGetUniformLocation(m->prog, "uCamPos");
    m->uBaseColor = glGetUniformLocation(m->prog, "uBaseColor");
    return 1;
}

void material_begin(const Material *m, m4 view, m4 proj, v3 campos, v3 base)
{
    glUseProgram(m->prog);
    glUniformMatrix4fv(m->uView, 1, GL_FALSE, view.m);
    glUniformMatrix4fv(m->uProj, 1, GL_FALSE, proj.m);
    glUniform3f(m->uCamPos, campos.x, campos.y, campos.z);
    glUniform3f(m->uBaseColor, base.x, base.y, base.z);
}

void material_set_model(const Material *m, m4 model)
{
    glUniformMatrix4fv(m->uModel, 1, GL_FALSE, model.m);
}

void material_destroy(Material *m)
{
    if (m->prog) glDeleteProgram(m->prog);
    m->prog = 0;
}
```

- [ ] **Step 3: `src/scene.h`** — conforme Interfaces.

- [ ] **Step 4: `src/scene.c`**

```c
#include "scene.h"
#include "material.h"
#include "gl_core.h"
#include "geometry/font_outline.h"
#include "geometry/contour_mesh.h"
#include "util/mathx.h"
#include "util/log.h"

#include <glad/gl.h>
#include <stdlib.h>
#include <math.h>

/* --- parametros fixos da Fase 2a (viram Config na 2b) --- */
#define SC_TEXT      "Modern 3D Text"
#define SC_FAMILY    L"Segoe UI"
#define SC_BOLD      1
#define SC_ITALIC    0
#define SC_DEPTH     0.30f
#define SC_MAXANGLE  42.0f
#define SC_TILTX     8.0f
#define SC_PERIOD    9.0f
#define SC_FLATTEN   0.004f

struct SceneRenderer {
    Material mat;
    GlMesh   mesh;
    float    radius;      /* esfera envolvente do modelo */
    v3       base_color;
};

SceneRenderer *scene_create(void)
{
    SceneRenderer *s = calloc(1, sizeof *s);
    if (!s) return NULL;

    if (!material_init(&s->mat)) { free(s); return NULL; }

    ContourSet cs;
    if (!font_build_contours(SC_TEXT, SC_FAMILY, SC_BOLD, SC_ITALIC, SC_FLATTEN, &cs)) {
        material_destroy(&s->mat); free(s); return NULL;
    }
    MeshData md;
    int ok = contour_mesh_build(&cs, (MeshParams){ SC_DEPTH }, &md);
    contourset_free(&cs);
    if (!ok) { material_destroy(&s->mat); free(s); return NULL; }

    s->mesh = gl_mesh_upload(md.verts, md.nverts, md.idx, md.nidx);

    float dx = md.maxx - md.minx, dy = md.maxy - md.miny, dz = md.maxz - md.minz;
    s->radius = 0.5f * sqrtf(dx * dx + dy * dy + dz * dz);
    if (s->radius < 1e-3f) s->radius = 1.0f;
    mesh_data_free(&md);

    s->base_color = (v3){ 0.72f, 0.74f, 0.78f };

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);           /* extrusao two-sided */
    return s;
}

void scene_render(SceneRenderer *s, double t, int fb_w, int fb_h)
{
    if (fb_w < 1) fb_w = 1;
    if (fb_h < 1) fb_h = 1;
    glViewport(0, 0, fb_w, fb_h);
    glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)fb_w / (float)fb_h;
    float fovy = m3dt_radians(35.0f);

    /* auto-fit: distância para o raio caber em ~70% da menor dimensão */
    float fit = tanf(fovy * 0.5f) * 0.72f;
    if (aspect < 1.0f) fit *= aspect;                    /* retrato: limita pela horizontal */
    float dist = s->radius / fmaxf(fit, 1e-3f) + s->radius;

    v3 eye = { 0.0f, 0.0f, dist };
    m4 view = m4_look_at(eye, (v3){ 0, 0, 0 }, (v3){ 0, 1, 0 });
    m4 proj = m4_perspective(fovy, aspect, 0.05f, dist * 4.0f + 10.0f);

    float ay = pendulum_angle((float)t, SC_PERIOD, SC_MAXANGLE);
    float ax = pendulum_angle((float)t + SC_PERIOD * 0.25f, SC_PERIOD, SC_TILTX);
    m4 model = m4_mul(m4_rotate_y(m3dt_radians(ay)), m4_rotate_x(m3dt_radians(ax)));

    material_begin(&s->mat, view, proj, eye, s->base_color);
    material_set_model(&s->mat, model);
    gl_mesh_draw(&s->mesh);
}

void scene_destroy(SceneRenderer *s)
{
    if (!s) return;
    gl_mesh_free(&s->mesh);
    material_destroy(&s->mat);
    free(s);
}
```

- [ ] **Step 5: `build/Makefile`** — `SRC += src/material.c src/scene.c`. Confirmar que `build/obj/src/material.o` e `build/obj/src/scene.o` dependem de `generated/embedded.h` (regra do Task 3 Step 4).

- [ ] **Step 6: Compilar (sem wire ainda) para pegar erros**

```sh
mingw32-make -f build/Makefile 2>&1 | tail -20
```
Esperado: compila (mesmo que `scene_*` ainda não seja chamado). Warnings = corrigir.

- [ ] **Step 7: Commit**

```sh
git add src/material.h src/material.c src/scene.h src/scene.c build/Makefile
git commit -m "$(printf 'feat: scene + material - auto-fit camera, pendulum, classic specular\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 7: Ligar a cena no saver e no preview + captura PNG

**Files:**
- Modify: `src/host_win32.c` (SceneRenderer por janela; remover `gl_render_clear`; hook `M3DT_SHOT`)
- Create: `build/tests/` — nada; verificação é headless + PNG
- Modify: `build/Makefile` (`SRC += src/geometry/stb_impl.c` já está; garantir `stb_image_write.h` baixado)

**Interfaces:**
- Consumes: `scene_create/render/destroy`, `gl_load`.
- Produces: `/s` e `/p` renderizam o texto 3D. Env `M3DT_SHOT=<caminho.png>` grava um PNG do frame ~60 e encerra (via `stbi_write_png`).

- [ ] **Step 1: Garantir `stb_image_write.h`**

```sh
test -f third_party/stb_image_write.h || \
  curl -sL -o third_party/stb_image_write.h https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h
```

- [ ] **Step 2: `src/host_win32.c` — trocar o render**

Adicionar includes:
```c
#include "gl_core.h"
#include "scene.h"
#include "stb_image_write.h"
```
Estender `GlWindow`:
```c
typedef struct { HWND hwnd; HDC dc; HGLRC rc; int w, h; SceneRenderer *scene; } GlWindow;
```
Em `gl_window_create`, após `gl_load()` (Task 2 Step 5) e a leitura de `g->w/g->h`:
```c
    g->scene = scene_create();
    if (!g->scene) log_errorf("scene_create falhou (janela %ls)", cls);
```
Em `gl_window_destroy`, antes de destruir o contexto:
```c
    if (g->rc) {
        wglMakeCurrent(g->dc, g->rc);
        if (g->scene) { scene_destroy(g->scene); g->scene = NULL; }
    }
```
Remover `gl_render_clear` inteira. Criar a nova função de frame:
```c
static void gl_window_frame(GlWindow *g, double t)
{
    wglMakeCurrent(g->dc, g->rc);
    RECT cr; GetClientRect(g->hwnd, &cr);
    g->w = cr.right; g->h = cr.bottom;
    if (g->scene) scene_render(g->scene, t, g->w, g->h);
    else { glViewport(0, 0, g->w, g->h); glClearColor(0.1f, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT); }
    SwapBuffers(g->dc);
}
```
Trocar as chamadas `gl_render_clear(&win[i], t)` / `gl_render_clear(&g, t)` por `gl_window_frame(...)` em `host_run_saver` e `host_run_preview`.

- [ ] **Step 3: Hook `M3DT_SHOT` no `host_run_saver`**

Logo após criar as janelas e antes do loop:
```c
    char shot[MAX_PATH]; shot[0] = 0;
    GetEnvironmentVariableA("M3DT_SHOT", shot, sizeof shot);
```
No loop, após `gl_window_frame(&win[i], t)` para `i==0`, quando `frame == 60 && shot[0]`:
```c
        if (shot[0] && frame == 60) {
            wglMakeCurrent(win[0].dc, win[0].rc);
            int W = win[0].w, H = win[0].h;
            unsigned char *px = malloc((size_t)W * H * 3);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, px);
            stbi_flip_vertically_on_write(1);
            stbi_write_png(shot, W, H, 3, px, W * 3);
            free(px);
            log_infof("shot salvo em %s (%dx%d)", shot, W, H);
            request_quit();
        }
```
(o `M3DT_SELFTEST` já faz `++frame`; garantir que `frame` incrementa mesmo sem selftest — mover o `++frame` para fora do `if (selftest)`.)

- [ ] **Step 4: Build + captura headless**

```sh
mingw32-make -f build/Makefile clean && mingw32-make -f build/Makefile
cp dist/Modern3DText.scr "$TEMP/m3dt_shot.exe"
rm -f "$LOCALAPPDATA/Modern3DText/log.txt"
M3DT_SELFTEST=1 M3DT_SHOT="$TEMP/m3dt_phase2a.png" "$TEMP/m3dt_shot.exe" /s
echo "exit=$?"
cat "$LOCALAPPDATA/Modern3DText/log.txt"
ls -l "$TEMP/m3dt_phase2a.png"
```
Esperado: `exit=0`; log com `glad: GL 3.3`, `shot salvo`, sem `glError`; o PNG existe. **Abrir/inspecionar o PNG**: deve mostrar o texto "Modern 3D Text" em 3D, com relevo e realce especular, sobre fundo quase preto. Se estiver em branco/preto puro ou com artefato grosseiro, investigar antes de commitar.

- [ ] **Step 5: Testes unitários (regressão)**

```sh
mingw32-make -f build/Makefile test
```
Esperado: `all tests passed`.

- [ ] **Step 6: Commit**

```sh
git add src/host_win32.c third_party/stb_image_write.h build/Makefile
git commit -m "$(printf 'feat: render extruded 3D text in saver + preview; M3DT_SHOT capture\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 8: Integração, QA da Fase 2a, tag

**Files:**
- Modify: `docs/qa-checklist-phase-1.md` → criar `docs/qa-checklist-phase-2a.md`
- Modify: `README.md` (nota de status)
- Modify: `build/Makefile` (garantir `make` / `make debug` / `make test` verdes de ponta a ponta)

- [ ] **Step 1: `make clean && make` e `make test` limpos**

```sh
mingw32-make -f build/Makefile clean && mingw32-make -f build/Makefile
mingw32-make -f build/Makefile test
mingw32-make -f build/Makefile clean && mingw32-make -f build/Makefile debug
```
Esperado: os três verdes; `dist/Modern3DText.scr` < 3 MB (esperado ~600 KB–1,2 MB com glad + libtess2 + stb).

- [ ] **Step 2: `docs/qa-checklist-phase-2a.md`**

```markdown
# QA — Fase 2a (texto 3D extrudado)

## Headless (feito na máquina de dev)
- [ ] `make`, `make debug`, `make test` verdes; `.scr` < 3 MB.
- [ ] `mathx` e `contour_mesh`: testes passam. `font_outline`: 'A' >= 2 contornos, multi-linha, fallback de fonte.
- [ ] `M3DT_SELFTEST=1 M3DT_SHOT=out.png ...scr /s` gera um PNG com o texto "Modern 3D Text"
      em 3D, relevo visível, realce especular, fundo escuro. Sem `glError` no log.

## Interativo (precisa de olho humano)
- [ ] `Modern3DText.scr /s` (ou "Visualizar" no painel): texto 3D girando em pêndulo,
      nunca mostra o verso invertido, some nas bordas do arco.
- [ ] Preview do painel de Proteção de Tela mostra o texto 3D (não só cor).
- [ ] Dois monitores: cada um com o texto enquadrado (auto-fit), proporção correta.
- [ ] Sai no input (mouse > 4 px / tecla / clique). Cursor some/volta.
- [ ] 5 min rodando: sem crescimento de memória/handles.
- [ ] GPU fraca / RDP: se o contexto 3.3 falhar, hoje o texto some (fallback real é a Fase 3);
      registrar o comportamento observado.
```

- [ ] **Step 3: README — nota de status**

Adicionar sob o título:
```markdown
Status: Fase 2a — renderiza texto 3D extrudado (fonte instalada -> contornos ->
tampa+paredes), pêndulo limitado, material especular. Parâmetros ainda fixos
no código; configuração entra na Fase 2b.
```

- [ ] **Step 4: Rodar o checklist interativo** (marcar itens; abrir issues para falhas antes de fechar).

- [ ] **Step 5: Commit + tag**

```sh
git add docs/qa-checklist-phase-2a.md README.md build/Makefile
git commit -m "$(printf 'feat: phase 2a integration - extruded 3D text renders end to end\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
git tag -a v0.2.0-phase2a -m "Fase 2a: texto 3D extrudado (font_outline + contour_mesh + scene + material 0)"
```

---

## Self-Review

**1. Cobertura (recorte da Fase 2a — spec §17 item 2, menos o que foi explicitamente adiado):**
- `font_outline` (GDI + stb_truetype, multi-linha, kerning) → Task 4. ✓
- `contour_mesh` (tampa + paredes; **micro-bevel adiado p/ Fase 3**, documentado nas Global Constraints) → Task 5. ✓
- `scene` (câmera auto-fit + pêndulo) → Task 6. ✓
- `material` modo 0 (especular clássico) → Tasks 3 (GLSL) + 6 (uniforms). ✓
- Infra que o spec pressupõe: `glad` (Task 2), `mathx` (Task 1), `embed` + shaders (Task 3). ✓
- Integração no `.scr` (`/s` e `/p` renderizam o texto) → Task 7. ✓
- Testes exigidos pelo spec §14.1: `mathx` (Task 1), `contour_mesh` (Task 5), `font_outline` (Task 4 — com a ressalva de fonte de sistema em vez de fonte embutida, já que o usuário recusou fontes embutidas). ✓
- **Fora da 2a (entra na 2b):** `Config` struct + registro, diálogo com abas Conteúdo/Movimento, mini-preview ao vivo. **Fora da 2a (Fase 3+):** SDF/bevel, shell, SVG, mesh import, clock, pós-processamento, outros materiais, fundos, i18n, presets, fallback GDI 2D.

**2. Placeholders:** o `geom_types.h` no Task 4 Step 2 mostra um rascunho e logo em seguida a "Versão final do arquivo" completa — a versão final é a que vale (sem placeholder). Nenhum "TODO"/"TBD". Código de terceiros (glad/stb/libtess2) é baixado por comando explícito, não hand-wave. Verificações de janela/GL têm comando + resultado esperado, e o Task 7 produz um PNG conferível.

**3. Consistência de tipos:**
- `MeshVertex { float px,py,pz,nx,ny,nz,surf }` — definido em `gl_core.h` (Task 2), usado em `contour_mesh.c` (Task 5, campos `px..surf`), consumido por `gl_mesh_upload` (Task 2) e pelos atributos 0/1/2 do `model.vert` (Task 3, `aPos`/`aNrm`/`aSurf`). Layout de atributos (offsets 0/3/6 floats, stride `sizeof(MeshVertex)`) casa com o `layout(location=...)` do shader. ✓
- `ContourSet` / `Contour` — `geom_types.h` (Task 4), produzido por `font_build_contours` (Task 4), consumido por `contour_mesh_build` (Task 5) e pelos testes. ✓
- `MeshData` / `MeshParams` — `contour_mesh.h` (Task 5), consumidos por `scene_create` (Task 6). `MeshParams` é `{ float depth }` em ambos. ✓
- `Material` campos `prog,uModel,uView,uProj,uCamPos,uBaseColor` — `material.h` (Task 6), setados em `material.c` a partir dos nomes de uniform que existem em `model.vert`/`model.frag` (Task 3): `uModel/uView/uProj` (vert), `uCamPos/uBaseColor` (frag). ✓
- `SceneRenderer` opaco — `scene.h` (Task 6), ponteiro guardado em `GlWindow.scene` (Task 7). `scene_create/render/destroy` chamados em `host_win32.c` (Task 7) com as assinaturas de `scene.h`. ✓
- `gl_load` / `gl_program` / `gl_mesh_*` — `gl_core.h` (Task 2), usados por `material.c`/`scene.c` (Task 6) e `host_win32.c` (Tasks 2, 7). ✓
- `pendulum_angle(t, period, maxdeg)` / `m4_*` — `mathx.h` (Task 1), usados em `scene.c` (Task 6) exatamente com essa assinatura. ✓
- `EMBED_model_vert` / `EMBED_model_frag` (+ `_len`) — gerados pelo `embed` (Task 3) a partir de `shaders/model.vert`/`.frag`, consumidos em `material.c` (Task 6). Nome do símbolo = basename com não-alfanum → `_`: `model.vert` → `model_vert`. ✓
- `M3DT_SELFTEST` (Fase 1) + `M3DT_SHOT` (Task 7): o `++frame` precisa sair do `if (selftest)` para o shot funcionar sem selftest — anotado no Task 7 Step 3. ✓

Sem inconsistências pendentes.

---

## Execution Handoff

**Plano completo e salvo em `docs/superpowers/plans/2026-09-10-modern-3d-text-phase-2a-extruded-text.md`. Duas opções de execução:**

**1. Subagent-Driven (recomendado)** — despacho um subagente novo por task, reviso entre tasks.

**2. Inline Execution** — executo os tasks nesta sessão com a skill executing-plans, em lotes com checkpoints.

**Qual abordagem?**

> Como na Fase 1: build e verificações headless rodam aqui (shell do w64devkit, GPU real). O Task 7 gera um PNG que eu abro pra conferir visualmente que o texto 3D está renderizando. A 2b (config + diálogo) vira um plano separado depois desta.
