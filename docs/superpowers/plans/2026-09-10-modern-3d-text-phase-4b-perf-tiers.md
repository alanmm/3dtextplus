# Modern 3D Text — Fase 4b: níveis de qualidade + auto-qualidade — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** O screensaver passa a se adaptar à máquina onde roda. Ganha os campos de
**desempenho** no `Config` (`fps_cap`, `vsync`, `msaa`, `render_scale`,
`auto_quality`), um módulo **`render_tiers`** que classifica a GPU no startup
(cheia / reduzida, mais overrides de RDP e `M3DT_FORCE_TIER`), **render scale**
(renderiza numa fração da resolução e faz upscale no passe final), **vsync / fps
cap** configuráveis, e uma malha de **auto-qualidade adaptativa** que mede o tempo
de GPU por frame (`GL_TIME_ELAPSED`) e degrada em degraus com histerese quando não
sustenta ~45 fps. Uma aba **Desempenho** no diálogo expõe tudo.

**Architecture:** `render_tiers.c` é o cérebro: uma função pura
`m3dt_tier_detect(gl_renderer)` (testável) + wrappers finos para RDP/env; um
`RenderQuality` = os ajustes *efetivos* (bloom on/off, samples de MSAA,
render_scale) derivados de `(Config, tier, passo do ladder)` por outra função pura
`render_quality_for_step(...)`; e um `AutoQuality` que guarda um anel de tempos de
GPU e decide subir/descer um passo. `post` deixa de fixar o MSAA e passa a receber
`samples` + separar o tamanho *interno* (cena) do tamanho de *saída* (composição),
o que dá o render scale de graça. `gl_window` carrega o `RenderQuality` + o
`AutoQuality`, mede a GPU em volta de `post_begin..post_present`, e aplica os
degraus só no modo saver. O host respeita `vsync` (`wglSwapIntervalEXT`) e faz o
pacing de `fps_cap` no loop do saver. A aba Desempenho segue o mesmo padrão das
abas 4a (sub-diálogo + `WM_HSCROLL`/`WM_COMMAND` → `g_work` + `WM_APP+1`).

**Tech Stack:** C11 · w64devkit · OpenGL 3.3 / GLSL 330 · `GL_TIME_ELAPSED`
(timer query, core 3.3) · `GetSystemMetrics(SM_REMOTESESSION)` · reuso de
`config`/`post`/`gl_core`/`gl_window`/`host_win32`/`config_dialog`/`scene`.

## Global Constraints

Do spec (§8.1, §10.1, §11) e do estado pós-Fase 4a. Todo task herda esta seção.

- **Linguagem:** C11. Flags `-std=c11 -municode -Wall -Wextra` (+ `-O2 -DNDEBUG`
  release). **Sem `-ffast-math`.** Build **sem warnings** (o projeto trata warning
  como bug; corrigir na hora).
- **Toolchain:** só w64devkit em `C:\Users\alanm\w64devkit`. **Sem downloads
  novos.** Todo comando de shell começa com
  `export PATH="/c/Users/alanm/w64devkit/bin:$PATH"`.
- **API gráfica:** OpenGL 3.3 core. `GL_TIME_ELAPSED` / `glGenQueries` /
  `glGetQueryObjectui64v` já estão no glad gerado (core 3.3). Nos contextos de
  fallback (3.1/2.1) o timer query pode não existir → `AutoQuality` se
  auto-desabilita (checa `glGetError` depois do primeiro `glGenQueries` e do
  primeiro `glBeginQuery`).
- **Formatos de FBO** (da 4a, não mudam): cor HDR `GL_RGBA16F`; bloom
  `GL_R11F_G11F_B10F`. MSAA no FBO **HDR** (não no default). `samples` efetivo =
  `min(pedido, gl_max_samples())`; `0` = sem MSAA (usa `gl_fbo_color16f` com
  depth em vez de `gl_fbo_hdr_ms`).
- **Registro:** só `HKCU\Software\Modern3DText` (testes: `..._test`). Nada fora
  disso e de `%LOCALAPPDATA%\Modern3DText\`.
- **`.scr` ≤ 3 MB** (falha o build se passar). i18n só na Fase 8 — strings novas
  em português direto no código/`.rc`, como as abas atuais.
- **Fora desta fase (vai pra 4c):** streaks de difração (starburst **e**
  anamórfico, escolhível), aberração cromática, vinheta, FXAA. O ladder de
  auto-qualidade nesta fase **não** tem o degrau "streaks off" (ainda não
  existem) — quando a 4c entrar, insere o degrau no topo do ladder.
- **Fora desta fase (Fase 8):** fallback **GDI 2D** sem GL (`fallback_gdi.c`).
  Esta fase só faz os níveis *dentro* do GL (cheio / reduzido).
- **`.exe` gotcha:** o build só atualiza `dist/Modern3DText.scr`. Antes de rodar
  o binário direto, `cp -f dist/Modern3DText.scr dist/Modern3DText.exe`. E o MSYS
  mastiga argumentos que começam com `/` → passar `MSYS_NO_PATHCONV=1` ao rodar
  `./dist/Modern3DText.exe /s`.
- **Commits frequentes**, um por task. TDD onde é determinístico (`config`,
  `render_tiers`, `render_quality_for_step`, decisão do `AutoQuality`). GL: PNG
  conferível (comparações com/sem render scale, tier reduzido, etc.).
- **Atribuição:** todo commit termina com
  `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>`.
- **Hooks de teste** já existentes e reusados: `M3DT_SELFTEST=1` (janela
  auto-fecha), `M3DT_SHOT=<path>` + `M3DT_SHOT_T=<s>` (captura PNG headless),
  `M3DT_LOG_APPEND=1` (não trunca o log), `M3DT_TAB=<n>` / `M3DT_HOLD_MS=<ms>`
  (aba inicial + timeout do `/c` selftest). Novos nesta fase: `M3DT_FORCE_TIER`,
  `M3DT_AQ_FORCE_MS`, `M3DT_AQ_FAST` (Task 5).

---

## File Structure

| Arquivo | Responsabilidade |
|---|---|
| `src/config.h` / `.c` | (+) `int fps_cap; int vsync; int msaa; float render_scale; int auto_quality;` após `bloom_radius`. Defaults, clamps, chaves REG_SZ homônimas, migração de `version` 1→2. |
| `src/render_tiers.h` / `.c` | **novo.** `M3dtTier` (FULL/REDUCED); `m3dt_tier_detect(const char *renderer)` (pura); `m3dt_tier_forced(M3dtTier*)` (lê `M3DT_FORCE_TIER`); `m3dt_is_remote_session()`; `m3dt_tier_name()`. `RenderQuality { int bloom, msaa; float render_scale; int step; }`; `render_quality_for_step(const Config*, M3dtTier, int step)` (pura); `render_quality_ladder_len(M3dtTier)`. `AutoQuality` opaco: `aq_create/destroy`, `aq_frame_begin/end`, `aq_update(AutoQuality*, RenderQuality* io)` → 1 se mudou. |
| `src/gl_core.h` / `.c` | (modif.) `gl_fbo_color16f` já tem depth opcional. (+) nada novo obrigatório — `gl_fbo_hdr_ms` com `samples<=1` já cai pra caminho single-sample? **Não** — Task 3 ajusta `post` pra escolher `gl_fbo_color16f` quando `samples<=1`. |
| `src/post.h` / `.c` | (modif.) `post_begin(Post*, int in_w, int in_h, int samples)`; `post_present(Post*, int out_w, int out_h, PostParams)` (tamanho interno vem do último `post_begin`); `ensure_size` re-cria o alvo da cena quando `samples` muda e alterna MSAA↔single-sample; composição faz upscale (`GL_LINEAR` no `p->hdr.color`). |
| `src/gl_window.h` / `.c` | (modif.) `GlWindow` ganha `RenderQuality quality; AutoQuality *aq; M3dtTier tier;`. `gl_window_create(..., int preview)` inalterado na assinatura; internamente resolve o tier + `render_quality_for_step(cfg, tier, 0)` e cria o `AutoQuality` (só se `!preview && cfg->auto_quality`). `gl_window_frame`: render scale + timer de GPU + `aq_update` + aplica. `gl_window_set_config` recomputa o `RenderQuality` base. `gl_window_destroy` libera o `aq`. vsync via `wglSwapIntervalEXT(cfg->vsync?1:0)`. |
| `src/host_win32.c` | (modif.) `host_run_saver`: pacing de `fps_cap` no loop (quando `>0`); loga o tier no startup. `env_flag` já existe; (+) helper `env_str`. |
| `res/screensaver.rc`, `src/resource.h` | (+) `IDD_TAB_PERF 115` + IDs `IDC_FPSCAP/IDC_VSYNC/IDC_MSAA/IDC_RSCALE/IDC_RSCALE_VAL/IDC_AUTOQ` (1600+). `IDC_TABS` ganha `TCS_MULTILINE`; `IDD_CONFIG` cresce em altura pra 2 fileiras de aba. |
| `src/config_dialog.c` | (+) `g_perf` sub-diálogo + `perf_proc`; `TabCtrl_InsertItem` índice 5; `select_tab` até 5; `preview`/`gl_window` já pega os campos novos via `gl_window_set_config`. |
| `build/Makefile` | `src/render_tiers.c` em `SRC_C`; `build/tests/test_render_tiers.c` em `TEST_SRC`, `src/render_tiers.c` em `TEST_UNITS`. |
| `build/tests/test_config.c` | (+) round-trip + clamps dos 5 campos de perf; migração `version` 1→2. |
| `build/tests/test_render_tiers.c` | **novo.** tabela de strings de `GL_RENDERER` → tier; `render_quality_for_step` em cada passo; decisão do `AutoQuality` com tempos injetados (ladder + histerese). |
| `build/tests/test_main.c` | (+) `run_render_tiers_tests();` |
| `docs/qa-checklist-phase-4b.md` | **novo.** |
| `README.md` | status → Fase 4b. |

---

## Task 1: Config — campos de desempenho + migração (TDD)

**Files:** `src/config.h`, `src/config.c`, `build/tests/test_config.c`

**Interfaces — `Config` ganha (após `bloom_radius`):**
```c
int   fps_cap;       /* 0 (sem limite) | 30 | 60 | 120 */
int   vsync;         /* 0/1 */
int   msaa;          /* 0 | 2 | 4 | 8 */
float render_scale;  /* 0.5 .. 1.0 */
int   auto_quality;  /* 0/1 */
```
Padrões: `fps_cap = 60`, `vsync = 1`, `msaa = 4`, `render_scale = 1.0f`,
`auto_quality = 1`. `CFG_VERSION` passa de `1` para `2`.

**Regra de saneamento** (decidida agora, os testes batem com ela): `fps_cap` e
`msaa` fazem *snap pro valor válido mais próximo* (`fps_cap ∈ {0,30,60,120}`,
`msaa ∈ {0,2,4,8}`; empate → o maior). Então `fps_cap` cru `999` → `120`, `7` →
`8`, `10` → `0`. `vsync`/`auto_quality` → `!= 0 ? 1 : 0`. `render_scale` →
`clampf(0.5, 1.0)`.

Migração: `config_load_from` já chama `config_defaults` primeiro, então um
registro `version = 1` (sem as chaves novas) carrega com os padrões acima
automaticamente — nenhum bloco de migração dedicado. `#define CFG_VERSION 2`; o
`config_save_to` (que já grava `CFG_VERSION`) reescreve `version = 2` no próximo
OK/Aplicar. Só um comentário no `.c` explicando.

- [ ] **Step 1: `build/tests/test_config.c`** — no bloco round-trip existente,
  setar antes do `config_save_to`:
  `a.fps_cap = 30; a.vsync = 0; a.msaa = 8; a.render_scale = 0.75f;
  a.auto_quality = 0;` e, depois do `config_load_from(&b, TESTKEY)`, conferir:
```c
    EXPECT(b.fps_cap == 30);
    EXPECT(b.vsync == 0);
    EXPECT(b.msaa == 8);
    EXPECT(nearf(b.render_scale, 0.75f));
    EXPECT(b.auto_quality == 0);
    EXPECT(b.version == 2);
```
  Adicionar um bloco de clamp novo no fim da função:
```c
    /* clamp perf: valores crus invalidos no registro */
    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);
    {
        HKEY k;
        RegCreateKeyExW(HKEY_CURRENT_USER, TESTKEY, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL);
        struct { const wchar_t *n, *v; } kv[] = {
            { L"fps_cap", L"999" }, { L"msaa", L"7" }, { L"render_scale", L"3.0" },
            { L"vsync", L"5" }, { L"auto_quality", L"0" },
        };
        for (int i = 0; i < 5; ++i)
            RegSetValueExW(k, kv[i].n, 0, REG_SZ, (const BYTE*)kv[i].v,
                           (DWORD)((wcslen(kv[i].v) + 1) * sizeof(wchar_t)));
        RegCloseKey(k);
    }
    Config c;
    config_load_from(&c, TESTKEY);
    EXPECT(c.fps_cap == 120);                 /* 999 -> valido mais proximo */
    EXPECT(c.msaa == 8);                      /* 7 -> 8 */
    EXPECT(nearf(c.render_scale, 1.0f));      /* 3.0 -> clamp 1.0 */
    EXPECT(c.vsync == 1);                     /* 5 -> 1 */
    EXPECT(c.auto_quality == 0);
```

- [ ] **Step 2: rodar o teste — deve falhar** (compilação: `'Config' has no
  member named 'fps_cap'`).
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```

- [ ] **Step 3: `src/config.h`** — os 5 campos após `bloom_radius`, com os
  comentários de faixa do bloco **Interfaces** acima. `src/config.c`:
  - `#define CFG_VERSION 2` — comentário: `/* v2: + campos perf; v1 migra via defaults */`.
  - `config_defaults`: `c->fps_cap = 60; c->vsync = 1; c->msaa = 4;
    c->render_scale = 1.0f; c->auto_quality = 1;`
  - Em `config_load_from`, após as leituras de bloom (usa o `float f;` já
    declarado ali):
```c
    reg_get_i(k, L"fps_cap", &c->fps_cap);
    reg_get_i(k, L"vsync", &c->vsync);
    reg_get_i(k, L"msaa", &c->msaa);
    reg_get_i(k, L"auto_quality", &c->auto_quality);
    if (reg_get_f(k, L"render_scale", &f)) c->render_scale = f;
```
  - No saneamento (fim da função), um helper local `snap` no topo do arquivo:
```c
static int snap_to(int v, const int *opts, int n)
{
    int best = opts[0], bd = 1 << 30;
    for (int i = 0; i < n; ++i) {
        int d = v > opts[i] ? v - opts[i] : opts[i] - v;
        if (d <= bd) { bd = d; best = opts[i]; }   /* <= => empate pro maior */
    }
    return best;
}
```
    e no saneamento:
```c
    static const int FPS_OPTS[4] = { 0, 30, 60, 120 };
    static const int MSAA_OPTS[4] = { 0, 2, 4, 8 };
    c->fps_cap = snap_to(c->fps_cap, FPS_OPTS, 4);
    c->msaa = snap_to(c->msaa, MSAA_OPTS, 4);
    c->vsync = c->vsync ? 1 : 0;
    c->auto_quality = c->auto_quality ? 1 : 0;
    c->render_scale = clampf(c->render_scale, 0.5f, 1.0f);
```
  - Em `config_save_to`, após os `set_f` de bloom:
```c
    set_f(k, L"fps_cap", (float)c->fps_cap);
    set_f(k, L"vsync", (float)c->vsync);
    set_f(k, L"msaa", (float)c->msaa);
    set_f(k, L"render_scale", c->render_scale);
    set_f(k, L"auto_quality", (float)c->auto_quality);
```

- [ ] **Step 4: rodar — deve passar.**
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```
  Esperado: `all tests passed`.

- [ ] **Step 5: Commit**
```sh
git add src/config.h src/config.c build/tests/test_config.c
git commit -m "$(printf 'feat: config perf fields (fps_cap/vsync/msaa/render_scale/auto_quality), schema v2\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 2: `render_tiers` — detecção estática (TDD)

**Files:** `src/render_tiers.h` (novo), `src/render_tiers.c` (novo),
`build/tests/test_render_tiers.c` (novo), `build/tests/test_main.c`,
`build/Makefile`

**Interfaces — `src/render_tiers.h`:**
```c
#ifndef M3DT_RENDER_TIERS_H
#define M3DT_RENDER_TIERS_H

#include "config.h"

typedef enum { M3DT_TIER_FULL = 0, M3DT_TIER_REDUCED = 1 } M3dtTier;

/* Pura: classifica pela string de GL_RENDERER (case-insensitive).
   NULL/"" -> FULL. Renderers de software/WARP/GDI -> REDUCED. */
M3dtTier    m3dt_tier_detect(const char *gl_renderer);

/* Le M3DT_FORCE_TIER ("full"|"reduced", case-insensitive). 1 e escreve *out se setado. */
int         m3dt_tier_forced(M3dtTier *out);

/* GetSystemMetrics(SM_REMOTESESSION) != 0 */
int         m3dt_is_remote_session(void);

const char *m3dt_tier_name(M3dtTier t);   /* "full" | "reduced" */

/* Combina tudo (env > RDP > string). Loga a decisao. */
M3dtTier    m3dt_tier_resolve(const char *gl_renderer);

#endif
```

- [ ] **Step 1: `build/tests/test_render_tiers.c`** (só a parte estática por
  enquanto):
```c
#include "test.h"
#include "render_tiers.h"
#include <string.h>

void run_render_tiers_tests(void)
{
    EXPECT(m3dt_tier_detect(NULL) == M3DT_TIER_FULL);
    EXPECT(m3dt_tier_detect("") == M3DT_TIER_FULL);
    EXPECT(m3dt_tier_detect("NVIDIA GeForce RTX 5060 Ti/PCIe/SSE2") == M3DT_TIER_FULL);
    EXPECT(m3dt_tier_detect("Intel(R) UHD Graphics 620") == M3DT_TIER_FULL);
    EXPECT(m3dt_tier_detect("AMD Radeon RX 6600") == M3DT_TIER_FULL);

    EXPECT(m3dt_tier_detect("GDI Generic") == M3DT_TIER_REDUCED);
    EXPECT(m3dt_tier_detect("llvmpipe (LLVM 15.0.7, 256 bits)") == M3DT_TIER_REDUCED);
    EXPECT(m3dt_tier_detect("softpipe") == M3DT_TIER_REDUCED);
    EXPECT(m3dt_tier_detect("Microsoft Basic Render Driver") == M3DT_TIER_REDUCED);
    EXPECT(m3dt_tier_detect("D3D12 (WARP)") == M3DT_TIER_REDUCED);
    EXPECT(m3dt_tier_detect("Google SwiftShader") == M3DT_TIER_REDUCED);
    EXPECT(m3dt_tier_detect("GDI GENERIC") == M3DT_TIER_REDUCED);   /* case-insensitive */

    EXPECT(strcmp(m3dt_tier_name(M3DT_TIER_FULL), "full") == 0);
    EXPECT(strcmp(m3dt_tier_name(M3DT_TIER_REDUCED), "reduced") == 0);
}
```

- [ ] **Step 2: `build/tests/test_main.c`** — declarar `void run_render_tiers_tests(void);`
  e chamar `run_render_tiers_tests();` (depois de `run_sdf_tests();`).

- [ ] **Step 3: `build/Makefile`** — `TEST_SRC` += `build/tests/test_render_tiers.c`;
  `TEST_UNITS` += `src/render_tiers.c`. (Também `SRC_C` += `src/render_tiers.c`
  pro build principal.)

- [ ] **Step 4: rodar — falha** (link error: `undefined reference to m3dt_tier_detect`).
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```

- [ ] **Step 5: `src/render_tiers.c`** — parte estática:
```c
#include "render_tiers.h"
#include "util/log.h"

#include <windows.h>
#include <ctype.h>
#include <string.h>

static int contains_ci(const char *hay, const char *needle)
{
    if (!hay || !needle) return 0;
    size_t nl = strlen(needle);
    if (nl == 0) return 0;
    for (const char *p = hay; *p; ++p) {
        size_t i = 0;
        while (i < nl && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i]))
            ++i;
        if (i == nl) return 1;
    }
    return 0;
}

M3dtTier m3dt_tier_detect(const char *r)
{
    static const char *bad[] = {
        "GDI Generic", "llvmpipe", "softpipe", "swrast",
        "Microsoft Basic Render", "WARP", "SwiftShader", "Gallium llvmpipe",
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
        if (contains_ci(r, bad[i])) return M3DT_TIER_REDUCED;
    return M3DT_TIER_FULL;
}

int m3dt_tier_forced(M3dtTier *out)
{
    char b[16];
    DWORD n = GetEnvironmentVariableA("M3DT_FORCE_TIER", b, sizeof b);
    if (n == 0 || n >= sizeof b) return 0;
    if (contains_ci(b, "reduced")) { *out = M3DT_TIER_REDUCED; return 1; }
    if (contains_ci(b, "full"))    { *out = M3DT_TIER_FULL;    return 1; }
    return 0;
}

int m3dt_is_remote_session(void) { return GetSystemMetrics(SM_REMOTESESSION) != 0; }

const char *m3dt_tier_name(M3dtTier t) { return t == M3DT_TIER_REDUCED ? "reduced" : "full"; }

M3dtTier m3dt_tier_resolve(const char *gl_renderer)
{
    M3dtTier t;
    if (m3dt_tier_forced(&t)) { log_infof("tier: %s (M3DT_FORCE_TIER)", m3dt_tier_name(t)); return t; }
    if (m3dt_is_remote_session()) { log_infof("tier: reduced (sessao remota)"); return M3DT_TIER_REDUCED; }
    t = m3dt_tier_detect(gl_renderer);
    log_infof("tier: %s (renderer='%s')", m3dt_tier_name(t), gl_renderer ? gl_renderer : "?");
    return t;
}
```
  `RenderQuality` + `render_quality_for_step` + `AutoQuality` ficam pro Task 3/5
  — deixar só um `#include` guard e as funções acima; **não** declarar as outras
  ainda (evita símbolo não usado). Ajuste `render_tiers.h` pra conter só o que
  este task implementa.

- [ ] **Step 6: rodar — passa.**
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test && mingw32-make -f build/Makefile
```
  Esperado: `all tests passed` e `Built dist/Modern3DText.scr`.

- [ ] **Step 7: Commit**
```sh
git add src/render_tiers.h src/render_tiers.c build/tests/test_render_tiers.c build/tests/test_main.c build/Makefile
git commit -m "$(printf 'feat: render_tiers static GPU classification (full/reduced) + M3DT_FORCE_TIER\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 3: `RenderQuality` + MSAA/render-scale efetivos no `post` e no `gl_window`

**Files:** `src/render_tiers.h` / `.c`, `src/post.h` / `.c`, `src/gl_window.h` /
`.c`, `src/host_win32.c`, `build/tests/test_render_tiers.c`

**Interfaces — adicionar a `render_tiers.h`:**
```c
typedef struct {
    int   bloom;         /* 0/1 efetivo (ja considerando o toggle do usuario? NAO:
                            so o gate de qualidade; o gl_window faz AND com cfg.bloom_on) */
    int   msaa;          /* 0|2|4|8 efetivo */
    float render_scale;  /* 0.5..1.0 efetivo */
    int   step;          /* posicao no ladder; 0 = topo (melhor) */
} RenderQuality;

/* Pura. step 0 = (cfg.msaa, cfg.render_scale, bloom=1). Cada passo piora um item.
   Em REDUCED o step 0 ja vem degradado (bloom 0, msaa <= 2, scale <= 0.75). */
RenderQuality render_quality_for_step(const Config *cfg, M3dtTier tier, int step);

/* Numero de passos validos (>=1). step e clampado a [0, len-1]. */
int render_quality_ladder_len(M3dtTier tier);
```

**Ladder (FULL), do topo pro fundo:**
| step | mudança acumulada |
|---|---|
| 0 | `bloom=1`, `msaa=cfg.msaa`, `scale=cfg.render_scale` |
| 1 | `bloom=0` |
| 2 | `msaa=min(cfg.msaa,4)` |
| 3 | `msaa=min(cfg.msaa,2)` |
| 4 | `msaa=0` |
| 5 | `scale=min(cfg.render_scale,0.75)` |
| 6 | `scale=0.5` |

`render_quality_ladder_len(FULL) = 7`. **REDUCED:** só 1 step —
`render_quality_for_step(cfg, REDUCED, 0) = { bloom=0, msaa=min(cfg.msaa,2),
scale=min(cfg.render_scale,0.75), step=0 }`, `render_quality_ladder_len(REDUCED)=1`.

- [ ] **Step 1: teste** — em `test_render_tiers.c` adicionar:
```c
    Config c; config_defaults(&c);      /* msaa 4, scale 1.0 */
    RenderQuality q0 = render_quality_for_step(&c, M3DT_TIER_FULL, 0);
    EXPECT(q0.bloom == 1 && q0.msaa == 4 && q0.render_scale > 0.99f);
    RenderQuality q1 = render_quality_for_step(&c, M3DT_TIER_FULL, 1);
    EXPECT(q1.bloom == 0 && q1.msaa == 4);
    RenderQuality q4 = render_quality_for_step(&c, M3DT_TIER_FULL, 4);
    EXPECT(q4.msaa == 0 && q4.bloom == 0);
    RenderQuality q6 = render_quality_for_step(&c, M3DT_TIER_FULL, 6);
    EXPECT(q6.render_scale <= 0.5f + 1e-4f);
    RenderQuality qhi = render_quality_for_step(&c, M3DT_TIER_FULL, 99);  /* clamp */
    EXPECT(qhi.render_scale <= 0.5f + 1e-4f);
    EXPECT(render_quality_ladder_len(M3DT_TIER_FULL) == 7);

    RenderQuality qr = render_quality_for_step(&c, M3DT_TIER_REDUCED, 0);
    EXPECT(qr.bloom == 0 && qr.msaa <= 2 && qr.render_scale <= 0.75f + 1e-4f);
    EXPECT(render_quality_ladder_len(M3DT_TIER_REDUCED) == 1);
```
  Rodar → falha (símbolo). 

- [ ] **Step 2: `render_tiers.c`** — implementar `render_quality_for_step` /
  `render_quality_ladder_len` exatamente pela tabela acima:
```c
int render_quality_ladder_len(M3dtTier tier)
{
    return tier == M3DT_TIER_REDUCED ? 1 : 7;
}

static float minf(float a, float b) { return a < b ? a : b; }
static int   mini(int a, int b)     { return a < b ? a : b; }

RenderQuality render_quality_for_step(const Config *cfg, M3dtTier tier, int step)
{
    int len = render_quality_ladder_len(tier);
    if (step < 0) step = 0;
    if (step > len - 1) step = len - 1;

    RenderQuality q;
    q.step = step;

    if (tier == M3DT_TIER_REDUCED) {
        q.bloom = 0;
        q.msaa = mini(cfg->msaa, 2);
        q.render_scale = minf(cfg->render_scale, 0.75f);
        return q;
    }

    q.bloom = (step >= 1) ? 0 : 1;
    q.msaa = cfg->msaa;
    if (step >= 2) q.msaa = mini(q.msaa, 4);
    if (step >= 3) q.msaa = mini(q.msaa, 2);
    if (step >= 4) q.msaa = 0;
    q.render_scale = cfg->render_scale;
    if (step >= 5) q.render_scale = minf(q.render_scale, 0.75f);
    if (step >= 6) q.render_scale = 0.5f;
    return q;
}
```
  Rodar `mingw32-make -f build/Makefile test` → passa.

- [ ] **Step 3: `src/post.*` — separar tamanho interno de saída + MSAA
  parametrizável.** `post.h`:
```c
void post_begin(Post *p, int in_w, int in_h, int samples);   /* liga o alvo da cena */
void post_present(Post *p, int out_w, int out_h, PostParams pr);  /* -> FB 0, faz upscale */
```
  `post.c`:
  - `struct Post` ganha `int in_w, in_h, samples, ms_on;` (renomear os `w,h`
    atuais pra `in_w,in_h`).
  - `ensure_size(Post *p, int w, int h, int samples)`: recria tudo se
    `w/h/samples` mudou. `p->ms_on = (samples >= 2)`.
    - `ms_on`: como hoje — `p->hdr_ms = gl_fbo_hdr_ms(w, h, samples)` (alvo da
      cena) + `p->hdr = gl_fbo_color16f(w, h, 0)` (destino do resolve, usado pelo
      bright-pass).
    - `!ms_on`: **não** cria `hdr_ms`; `p->hdr = gl_fbo_color16f(w, h, 1)` (com
      depth) é o alvo da cena **e** a entrada do bright-pass; sem resolve.
    - bloom mips: iguais (`w/2^(i+1)`), independente de `ms_on`.
  - `post_begin(p, in_w, in_h, samples)`:
```c
    if (in_w < 1) in_w = 1;
    if (in_h < 1) in_h = 1;
    if (samples > 8) samples = 8;
    ensure_size(p, in_w, in_h, samples);
    gl_fbo_bind(p->ms_on ? &p->hdr_ms : &p->hdr);   /* alvo da cena */
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
```
  - `post_present(p, out_w, out_h, pr)`:
    - se `p->ms_on`: `gl_blit_resolve(&p->hdr_ms, &p->hdr);` senão nada (a cena já
      está em `p->hdr`).
    - bright-pass / downsample / upsample: **iguais**, operam em `p->hdr` (tamanho
      `p->in_w×p->in_h`) e nos mips (relativos a `in_w`).
    - composição: `glBindFramebuffer(0); glViewport(0,0,out_w,out_h);` — o
      triângulo fullscreen cobre a viewport de saída; `p->hdr.color` é `GL_LINEAR`
      + `GL_CLAMP_TO_EDGE` (já é) → upscale bilinear automático quando
      `in<out`. Nada mais muda.
  - `post_create`: tirar o `p->samples = mx>=4?4:...` (agora vem por frame).
  - `post_destroy`: inalterado (libera hdr_ms, hdr, bloom[]).

- [ ] **Step 4: `src/gl_window.*`.** `gl_window.h`: sem mudança de assinatura.
  `gl_window.c`:
  - `#include "render_tiers.h"`. `struct GlWindow` ganha
    `M3dtTier tier; RenderQuality quality;` (o `AutoQuality *aq` entra no Task 5;
    deixar o campo já declarado como `void *aq;` NULL por ora **não** — só
    adicionar no Task 5).
  - Em `gl_window_create`, depois do `gl_load()` e do log de `GL_RENDERER`:
```c
    const char *rends = (const char *)glGetString(GL_RENDERER);
    g->tier = preview ? M3DT_TIER_FULL : m3dt_tier_resolve(rends);
    {
        Config tmp;                        /* fallback se cfg == NULL */
        const Config *qc = cfg;
        if (!qc) { config_defaults(&tmp); qc = &tmp; }
        g->quality = render_quality_for_step(qc, g->tier, 0);
    }
```
    (preview/mini-preview sempre FULL step 0 — a janelinha não adapta. O
    `gl_window_set_config` logo abaixo, quando `cfg != NULL`, sobrescreve com o
    valor real — precisa `#include "config.h"` já presente via `gl_window.h`.)
  - `gl_window_set_config(g, cfg)`: além do que já faz, recomputar
    `g->quality = render_quality_for_step(cfg, g->tier, g->quality.step);`
    (mantém o passo atual do ladder, só reavalia os valores) e
    `if (g->rc && p_wglSwapIntervalEXT) p_wglSwapIntervalEXT(cfg->vsync ? 1 : 0);`
    Guardar `g->vsync = cfg->vsync;` num campo novo (pro host saber, Task 4).
    **Cuidado:** `gl_window_set_config` é chamado no `gl_window_create` antes do
    `scene`; o `wglMakeCurrent` já foi feito. OK.
  - `gl_window_frame(g, t)`:
```c
    wglMakeCurrent(g->dc, g->rc);
    RECT cr; GetClientRect(g->hwnd, &cr);
    g->w = cr.right; g->h = cr.bottom;

    RenderQuality q = g->quality;
    int sw = (int)(g->w * q.render_scale + 0.5f); if (sw < 16) sw = 16;
    int sh = (int)(g->h * q.render_scale + 0.5f); if (sh < 16) sh = 16;

    post_begin(g->post, sw, sh, q.msaa);
    if (g->scene) scene_render(g->scene, t, sw, sh);
    else { glClearColor(0.10f,0,0,1); glClear(GL_COLOR_BUFFER_BIT); }
    PostParams pr = g->post_params;
    pr.bloom = (!g->preview && q.bloom && g->post_params.bloom) ? 1 : 0;
    post_present(g->post, g->w, g->h, pr);

    SwapBuffers(g->dc);
```
  - `gl_window_render_scene_at`: idem sem `SwapBuffers`.
  - `frame_post_params` (helper da 4a) — **remover**, a lógica agora está inline
    (usa `q.bloom`). Confira que não há outro caller.

- [ ] **Step 5: `src/host_win32.c`** — no `host_run_saver`, logo após criar as
  janelas, o tier já foi logado dentro do `gl_window_create`. Nada obrigatório
  aqui neste task (vsync/fps ficam no Task 4). Só garantir que compila.

- [ ] **Step 6: Build + captura comparativa.**
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile && cp -f dist/Modern3DText.scr dist/Modern3DText.exe
S="C:/Users/alanm/AppData/Local/Temp/claude/<sessao>/scratchpad"   # ajuste
# tier cheio, render_scale 1.0
powershell -Command "New-Item 'HKCU:\Software\Modern3DText' -Force | Out-Null; \
  'material_mode 1','roughness 0.16','metalness 1','base_color #C8CED8','bloom_on 1','render_scale 1.0','msaa 4' | % { \
    $p=$_.Split(' '); Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name $p[0] -Value $p[1] -Type String }"
MSYS_NO_PATHCONV=1 M3DT_LOG_APPEND=1 M3DT_SELFTEST=1 M3DT_SHOT="$S/t3_full.png" M3DT_SHOT_T=2.4 ./dist/Modern3DText.exe /s
# render_scale 0.5
powershell -Command "Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name render_scale -Value '0.5' -Type String"
MSYS_NO_PATHCONV=1 M3DT_LOG_APPEND=1 M3DT_SELFTEST=1 M3DT_SHOT="$S/t3_scale50.png" M3DT_SHOT_T=2.4 ./dist/Modern3DText.exe /s
# tier reduzido (bloom some, msaa<=2, scale<=0.75)
powershell -Command "Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name render_scale -Value '1.0' -Type String"
MSYS_NO_PATHCONV=1 M3DT_FORCE_TIER=reduced M3DT_LOG_APPEND=1 M3DT_SELFTEST=1 M3DT_SHOT="$S/t3_reduced.png" M3DT_SHOT_T=2.4 ./dist/Modern3DText.exe /s
powershell -Command "Remove-Item 'HKCU:\Software\Modern3DText' -Recurse -Force"
```
  **Conferir** (abrir os 3 PNGs + o log):
  - `t3_full`: bloom presente, nitidez normal.
  - `t3_scale50`: mesmo enquadramento, visivelmente mais macio/aliasado (metade
    da resolução), sem quebrar; texto ainda legível.
  - `t3_reduced`: **sem** bloom, bordas mais duras (msaa 2), leve suavização
    (scale 0.75). Log tem `tier: reduced (M3DT_FORCE_TIER)`.
  - Sem `glError` / `FBO ... incompleto` no log em nenhum.

- [ ] **Step 7: regressão + estabilidade.**
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test         # all tests passed
for i in $(seq 1 10); do MSYS_NO_PATHCONV=1 M3DT_SELFTEST=1 ./dist/Modern3DText.exe /s >/dev/null 2>&1 || echo FAIL $i; done
for i in $(seq 1 10); do MSYS_NO_PATHCONV=1 M3DT_SELFTEST=1 ./dist/Modern3DText.exe /c >/dev/null 2>&1 || echo FAIL $i; done
```

- [ ] **Step 8: Commit**
```sh
git add src/render_tiers.h src/render_tiers.c src/post.h src/post.c src/gl_window.h src/gl_window.c src/host_win32.c build/tests/test_render_tiers.c
git commit -m "$(printf 'feat: effective RenderQuality - parametric MSAA + render scale in the post chain\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 4: `vsync` + `fps_cap` no host

**Files:** `src/gl_window.h` / `.c`, `src/host_win32.c`

**Interfaces:** `gl_window.h` (+) `int gl_window_vsync(const GlWindow *g);`
(getter trivial do campo `g->vsync` gravado no Task 3 Step 4).

- [ ] **Step 1: `gl_window.c`** — garantir que `gl_window_create` também aplica o
  vsync inicial (o `p_wglSwapIntervalEXT(1)` fixo de hoje vira
  `p_wglSwapIntervalEXT(cfg ? (cfg->vsync ? 1 : 0) : 1)`). Adicionar
  `int gl_window_vsync(const GlWindow *g) { return g->vsync; }`.

- [ ] **Step 2: `src/host_win32.c` — pacing no loop do saver.** No topo de
  `host_run_saver`, depois de `config_load(&cfg)`:
```c
    /* alvo de frame em ms: fps_cap 0 => sem alvo (deixa o vsync/loop mandar) */
    const double frame_ms = cfg.fps_cap > 0 ? 1000.0 / (double)cfg.fps_cap : 0.0;
```
  No fim de cada iteração do `while (!g_quit)`, trocar o `Sleep(1)` por:
```c
        if (frame_ms > 0.0) {
            LARGE_INTEGER e; QueryPerformanceCounter(&e);
            double used = (double)(e.QuadPart - iter_start.QuadPart) * 1000.0 / (double)freq.QuadPart;
            double rest = frame_ms - used;
            if (rest > 1.5) Sleep((DWORD)(rest - 0.5));
            else Sleep(0);
        } else {
            Sleep(1);
        }
```
  onde `iter_start` é um `QueryPerformanceCounter` no **início** da iteração
  (adicionar `LARGE_INTEGER iter_start; QueryPerformanceCounter(&iter_start);`
  logo após o `while (!g_quit) {`).
  > `timeBeginPeriod(1)` já está ativo, então `Sleep` tem resolução ~1 ms.
  > Não busca precisão de vsync; é só um teto pra não fritar a GPU a 1000 fps
  > quando o vsync está off.

- [ ] **Step 3: log** — logo depois de criar as janelas no saver:
```c
    log_infof("saver: fps_cap=%d vsync=%d msaa=%d render_scale=%.2f auto_quality=%d",
              cfg.fps_cap, cfg.vsync, cfg.msaa, (double)cfg.render_scale, cfg.auto_quality);
```

- [ ] **Step 4: Build + verificação.**
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile && cp -f dist/Modern3DText.scr dist/Modern3DText.exe
# vsync on, cap 60: ~90 frames devem levar ~1.5s (selftest fecha em frame>=90)
powershell -Command "New-Item 'HKCU:\Software\Modern3DText' -Force | Out-Null; Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name vsync -Value '1' -Type String; Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name fps_cap -Value '60' -Type String"
time MSYS_NO_PATHCONV=1 M3DT_LOG_APPEND=1 M3DT_SELFTEST=1 ./dist/Modern3DText.exe /s
# vsync off, cap 30: mesmo nº de frames, deve levar ~3s
powershell -Command "Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name vsync -Value '0' -Type String; Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name fps_cap -Value '30' -Type String"
time MSYS_NO_PATHCONV=1 M3DT_LOG_APPEND=1 M3DT_SELFTEST=1 ./dist/Modern3DText.exe /s
powershell -Command "Remove-Item 'HKCU:\Software\Modern3DText' -Recurse -Force"
```
  Conferir: o log mostra a linha `saver: fps_cap=... vsync=...` com os valores
  certos; a run com `cap 30` demora ~2× a com `cap 60`; nenhuma trava.

- [ ] **Step 5: regressão.** `mingw32-make -f build/Makefile test` → `all tests passed`.

- [ ] **Step 6: Commit**
```sh
git add src/gl_window.h src/gl_window.c src/host_win32.c
git commit -m "$(printf 'feat: honor perf.vsync and perf.fps_cap in the saver loop\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 5: auto-qualidade adaptativa (`GL_TIME_ELAPSED` + histerese)

**Files:** `src/render_tiers.h` / `.c`, `src/gl_window.c`,
`build/tests/test_render_tiers.c`

**Interfaces — adicionar a `render_tiers.h`:**
```c
typedef struct AutoQuality AutoQuality;

/* cfg + tier definem o ladder. Comeca no step 0. NULL se malloc falhar. */
AutoQuality *aq_create(const Config *cfg, M3dtTier tier);
void         aq_destroy(AutoQuality *aq);

/* Em volta do render de 1 frame (scene + post). No-op se o timer query
   nao existe / aq == NULL. */
void aq_frame_begin(AutoQuality *aq);
void aq_frame_end(AutoQuality *aq);

/* Chama depois do aq_frame_end. Se decidiu mudar de passo, escreve o novo
   RenderQuality em *io e retorna 1. Senao retorna 0. */
int  aq_update(AutoQuality *aq, RenderQuality *io);

/* --- so pra teste: decisao pura --- */
typedef struct {
    int   step;         /* passo atual */
    int   ladder_len;
    double p90_ms;      /* percentil 90 da janela */
    double since_change_s;
} AqInput;
/* Retorna o novo passo (>= 0). Regra:
   sobe de passo (degrada) se p90 > 22 e since_change >= 5;
   desce de passo se step>0 e p90 < 12 e since_change >= 5;
   senao mantem. */
int aq_decide(AqInput in);
```

**Implementação (`render_tiers.c`):**
```c
struct AutoQuality {
    Config   cfg;
    M3dtTier tier;
    int      step;
    int      enabled;        /* 0 se timer query indisponivel */
    unsigned q[2];           /* timer queries double-buffered */
    int      qi;             /* indice do frame atual */
    int      primed;         /* ja tem query pendente do frame anterior */
    double   ring[32];
    int      ring_n, ring_head;
    LARGE_INTEGER last_change, freq;
    int      forced_ms;      /* M3DT_AQ_FORCE_MS: >0 injeta esse tempo */
};
```
- `aq_create`: `enabled = (tier == FULL && render_quality_ladder_len(FULL) > 1)`.
  Lê `M3DT_AQ_FORCE_MS` (int) → `forced_ms`. Lê `M3DT_AQ_FAST` (bool) → se setado,
  troca as janelas de 5 s por 0.4 s e o mínimo de amostras de 20 pra 8 (pra teste
  headless). `glGenQueries(2, aq->q)`; se `glGetError()` != 0 → `enabled = 0`.
  `QueryPerformanceFrequency(&aq->freq); QueryPerformanceCounter(&aq->last_change);`
- `aq_frame_begin`: `if (!enabled || forced_ms>0) return; glBeginQuery(GL_TIME_ELAPSED, q[qi]);`
- `aq_frame_end`:
  - se `forced_ms > 0`: `ring_push(forced_ms); return;`
  - `glEndQuery(GL_TIME_ELAPSED);`
  - se `primed`: lê `q[qi^1]` — `glGetQueryObjectuiv(q[qi^1], GL_QUERY_RESULT_AVAILABLE, &avail);`
    se `avail`: `glGetQueryObjectui64v(q[qi^1], GL_QUERY_RESULT, &ns); ring_push(ns/1e6);`
  - `primed = 1; qi ^= 1;`
- `ring_push(ms)`: buffer circular de 32; guarda `ring_n = min(ring_n+1, 32)`.
- `aq_update`:
  - `min_samples = M3DT_AQ_FAST ? 8 : 20; win_s = M3DT_AQ_FAST ? 0.4 : 5.0;`
  - se `!enabled` ou `ring_n < min_samples` → retorna 0.
  - `p90 = percentil(ring, ring_n, 0.90)` (copia + `qsort` + índice).
  - `now`; `since = (now - last_change) / freq` (segundos).
  - `AqInput in = { step, render_quality_ladder_len(tier), p90, since };`
    (mas com `win_s` no lugar de 5 — passe `win_s` como 4º campo já normalizado:
    em vez de `since_change_s`, passe `since / win_s` e compare com `1.0`.)
    **Simplifica:** `aq_decide` compara `since_change_s >= 5.0`; no modo FAST,
    multiplique `since` por `5.0/win_s` antes de montar o `AqInput`. Assim
    `aq_decide` fica pura e sem parâmetro de janela.
  - `int ns = aq_decide(in);` se `ns != step`: `step = ns;
    QueryPerformanceCounter(&last_change); ring_n = 0; /* limpa a janela */
    *io = render_quality_for_step(&cfg, tier, step); return 1;`
  - senão 0.
- `aq_decide` (pura):
```c
int aq_decide(AqInput in)
{
    if (in.since_change_s < 5.0) return in.step;
    if (in.p90_ms > 22.0 && in.step < in.ladder_len - 1) return in.step + 1;
    if (in.p90_ms < 12.0 && in.step > 0)                 return in.step - 1;
    return in.step;
}
```

- [ ] **Step 1: teste da decisão pura** — `test_render_tiers.c`:
```c
    /* aq_decide */
    AqInput a = { .step = 0, .ladder_len = 7, .p90_ms = 30.0, .since_change_s = 6.0 };
    EXPECT(aq_decide(a) == 1);                         /* lento -> degrada */
    a.since_change_s = 2.0; EXPECT(aq_decide(a) == 0); /* histerese trava */
    AqInput b = { .step = 3, .ladder_len = 7, .p90_ms = 8.0, .since_change_s = 9.0 };
    EXPECT(aq_decide(b) == 2);                         /* folgado -> recupera */
    AqInput c = { .step = 6, .ladder_len = 7, .p90_ms = 99.0, .since_change_s = 9.0 };
    EXPECT(aq_decide(c) == 6);                         /* ja no fundo */
    AqInput d = { .step = 0, .ladder_len = 7, .p90_ms = 8.0, .since_change_s = 9.0 };
    EXPECT(aq_decide(d) == 0);                         /* ja no topo */
    AqInput e = { .step = 2, .ladder_len = 7, .p90_ms = 16.0, .since_change_s = 9.0 };
    EXPECT(aq_decide(e) == 2);                         /* zona morta 12..22 */
```
  Rodar → falha. Implementar `aq_decide` + o resto. Rodar → passa.
  (As funções `aq_create/frame_begin/frame_end/update` tocam GL/Win32 — não são
  cobertas por unit test; ficam pro teste headless no Step 4.)

- [ ] **Step 2: `gl_window.c` integra.** `struct GlWindow` (+) `AutoQuality *aq;`.
  - `gl_window_create`: após resolver `g->tier` e `g->quality`,
    `g->aq = (!preview && cfg && cfg->auto_quality) ? aq_create(cfg, g->tier) : NULL;`
  - `gl_window_frame`: `aq_frame_begin(g->aq);` **antes** do `post_begin`;
    `aq_frame_end(g->aq);` **depois** do `post_present`; então
```c
    RenderQuality nq = g->quality;
    if (aq_update(g->aq, &nq)) {
        g->quality = nq;
        log_infof("autoQuality: step %d (bloom=%d msaa=%d scale=%.2f)",
                  nq.step, nq.bloom, nq.msaa, (double)nq.render_scale);
    }
    SwapBuffers(g->dc);
```
  - `gl_window_render_scene_at`: **não** mexe no `aq` (é frame de captura, fora
    do orçamento).
  - `gl_window_set_config`: se o `cfg->auto_quality` mudou de 0→1 e `g->aq` é
    NULL e `!g->preview` → `g->aq = aq_create(cfg, g->tier);`; se mudou 1→0 →
    `aq_destroy(g->aq); g->aq = NULL; g->quality = render_quality_for_step(cfg,
    g->tier, 0);` (volta pro topo). Do contrário, `aq_create` novo com o cfg
    atualizado só se já existia (pra pegar msaa/scale novos no ladder) — mais
    simples: sempre destruir e recriar quando `auto_quality` está on.
  - `gl_window_destroy`: `if (g->aq) { aq_destroy(g->aq); g->aq = NULL; }` (com
    contexto corrente, junto do `post_destroy`).

- [ ] **Step 3: `render_tiers.c` — `M3DT_AQ_FORCE_MS` / `M3DT_AQ_FAST`** já
  descritos acima; garantir que sem essas envs o comportamento é o normal.

- [ ] **Step 4: teste headless do ladder.**
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile && cp -f dist/Modern3DText.scr dist/Modern3DText.exe
powershell -Command "New-Item 'HKCU:\Software\Modern3DText' -Force | Out-Null; Set-ItemProperty 'HKCU:\Software\Modern3DText' -Name auto_quality -Value '1' -Type String"
# injeta 30 ms/frame + janelas rapidas: deve descer varios degraus em ~90 frames
MSYS_NO_PATHCONV=1 M3DT_AQ_FORCE_MS=30 M3DT_AQ_FAST=1 M3DT_LOG_APPEND=1 M3DT_SELFTEST=1 ./dist/Modern3DText.exe /s
grep "autoQuality: step" "$LOCALAPPDATA/Modern3DText/log.txt"
# injeta 5 ms/frame: nao deve descer nenhum degrau
MSYS_NO_PATHCONV=1 M3DT_AQ_FORCE_MS=5 M3DT_AQ_FAST=1 M3DT_LOG_APPEND=1 M3DT_SELFTEST=1 ./dist/Modern3DText.exe /s
grep -c "autoQuality: step" "$LOCALAPPDATA/Modern3DText/log.txt"   # espera 0 na 2a run
powershell -Command "Remove-Item 'HKCU:\Software\Modern3DText' -Recurse -Force"
```
  Conferir: run de 30 ms → log tem `autoQuality: step 1` depois `step 2`… em
  ordem, com ~0.4 s entre cada (a janela FAST). Run de 5 ms → nenhuma linha
  `autoQuality`. Sem `glError`.

- [ ] **Step 5: teste real (sem forçar) — não regride numa GPU boa.**
```sh
MSYS_NO_PATHCONV=1 M3DT_LOG_APPEND=1 M3DT_SELFTEST=1 ./dist/Modern3DText.exe /s
grep -c "autoQuality" "$LOCALAPPDATA/Modern3DText/log.txt"   # 0 na RTX
```

- [ ] **Step 6: regressão + estabilidade** (`make test`; 10× `/s`; 10× `/c`).

- [ ] **Step 7: Commit**
```sh
git add src/render_tiers.h src/render_tiers.c src/gl_window.c build/tests/test_render_tiers.c
git commit -m "$(printf 'feat: adaptive autoQuality - GPU frame timing drives the quality ladder with hysteresis\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 6: aba Desempenho no diálogo

**Files:** `src/resource.h`, `res/screensaver.rc`, `src/config_dialog.c`

**IDs novos (`resource.h`):**
```c
#define IDD_TAB_PERF      115
#define IDC_FPSCAP        1600
#define IDC_VSYNC         1601
#define IDC_MSAA          1602
#define IDC_RSCALE        1603
#define IDC_RSCALE_VAL    1604
#define IDC_AUTOQ         1605
```

**Aba Desempenho (índice 5):** combo **Limite de FPS** (Sem limite / 30 / 60 /
120), check **VSync**, combo **Anti-serrilhado (MSAA)** (Desligado / 2x / 4x /
8x), slider **Escala de render** (50–100 %), check **Qualidade automática**
(quando ligado, os outros continuam editáveis — são o ponto de partida do
ladder; um `LTEXT` explica isso).

- [ ] **Step 1: `res/screensaver.rc` — `IDC_TABS` ganha `TCS_MULTILINE`** e o
  `IDD_CONFIG` cresce pra caber 2 fileiras de aba. (`TBS_HORZ` já é usado no
  `.rc` sem `#include <commctrl.h>` — `TCS_MULTILINE` deve funcionar igual; se o
  `windres` reclamar de símbolo indefinido, adicionar `#include <commctrl.h>` no
  topo do `.rc`.)
```
IDD_CONFIG DIALOGEX 0, 0, 448, 250
...
    CONTROL  "", IDC_TABS, "SysTabControl32", WS_TABSTOP | TCS_MULTILINE, 6, 6, 250, 238
    CONTROL  "", IDC_PREVIEW, "Static", SS_BLACKFRAME, 264, 18, 178, 192
    DEFPUSHBUTTON "OK",       IDOK,      264, 220, 56, 14
    PUSHBUTTON    "Cancelar", IDCANCEL,  326, 220, 56, 14
    PUSHBUTTON    "Aplicar",  IDC_APPLY, 386, 220, 56, 14
```
  Os 6 templates de aba (`IDD_TAB_*`) sobem de `196` pra `200` de altura (a área
  útil encolhe ~16 px com a 2ª fileira). Conferir no screenshot que a aba
  **Geometria** (a mais cheia, controle mais baixo em `y=170..184`) não corta;
  se cortar, subir `IDD_CONFIG` pra `262` e os templates pra `212`.

- [ ] **Step 2: `res/screensaver.rc` — `IDD_TAB_PERF`** (antes do `VS_VERSION_INFO`):
```
IDD_TAB_PERF DIALOGEX 0, 0, 240, 200
STYLE DS_SETFONT | DS_CONTROL | WS_CHILD
FONT 9, "Segoe UI", 400, 0, 0x1
BEGIN
    LTEXT      "Limite de FPS:", -1, 8, 10, 90, 9
    COMBOBOX   IDC_FPSCAP, 100, 8, 90, 80, CBS_DROPDOWNLIST | WS_TABSTOP
    AUTOCHECKBOX "VSync", IDC_VSYNC, 8, 30, 120, 12
    LTEXT      "Anti-serrilhado (MSAA):", -1, 8, 50, 110, 9
    COMBOBOX   IDC_MSAA, 120, 48, 70, 80, CBS_DROPDOWNLIST | WS_TABSTOP
    LTEXT      "Escala de render:", -1, 8, 72, 90, 9
    LTEXT      "", IDC_RSCALE_VAL, 182, 72, 50, 9
    CONTROL    "", IDC_RSCALE, "msctls_trackbar32", WS_TABSTOP | TBS_HORZ, 8, 82, 224, 18
    AUTOCHECKBOX "Qualidade automatica", IDC_AUTOQ, 8, 108, 160, 12
    LTEXT      "Com a qualidade automatica ligada, os valores acima sao o ponto de partida; o screensaver reduz sozinho se a GPU nao acompanhar.",
               -1, 8, 124, 224, 32
END
```

- [ ] **Step 3: `src/config_dialog.c`.**
  - `static HWND g_perf;` junto dos outros `g_*`.
  - `select_tab(int sel)` — (+) `ShowWindow(g_perf, sel == 5 ? SW_SHOW : SW_HIDE);`
  - `perf_labels(HWND h)`:
```c
static void perf_labels(HWND h)
{
    wchar_t b[32];
    swprintf(b, 32, L"%d%%", (int)(g_work.render_scale * 100.0f + 0.5f));
    SetDlgItemTextW(h, IDC_RSCALE_VAL, b);
}
```
  - `perf_proc`:
```c
static INT_PTR CALLBACK perf_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            static const wchar_t *fps[] = { L"Sem limite", L"30", L"60", L"120" };
            static const int fpsv[] = { 0, 30, 60, 120 };
            for (int i = 0; i < 4; ++i)
                SendDlgItemMessageW(h, IDC_FPSCAP, CB_ADDSTRING, 0, (LPARAM)fps[i]);
            int fi = 0; for (int i = 0; i < 4; ++i) if (fpsv[i] == g_work.fps_cap) fi = i;
            SendDlgItemMessageW(h, IDC_FPSCAP, CB_SETCURSEL, fi, 0);

            static const wchar_t *ms[] = { L"Desligado", L"2x", L"4x", L"8x" };
            static const int msv[] = { 0, 2, 4, 8 };
            for (int i = 0; i < 4; ++i)
                SendDlgItemMessageW(h, IDC_MSAA, CB_ADDSTRING, 0, (LPARAM)ms[i]);
            int mi = 2; for (int i = 0; i < 4; ++i) if (msv[i] == g_work.msaa) mi = i;
            SendDlgItemMessageW(h, IDC_MSAA, CB_SETCURSEL, mi, 0);

            CheckDlgButton(h, IDC_VSYNC, g_work.vsync ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(h, IDC_AUTOQ, g_work.auto_quality ? BST_CHECKED : BST_UNCHECKED);
            set_slider(h, IDC_RSCALE, 50, 100, (int)(g_work.render_scale * 100.0f + 0.5f));
            perf_labels(h);
            return TRUE;
        }
        case WM_HSCROLL:
            g_work.render_scale =
                (float)SendDlgItemMessageW(h, IDC_RSCALE, TBM_GETPOS, 0, 0) / 100.0f;
            perf_labels(h);
            preview_dirty(h);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDC_FPSCAP:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        static const int fpsv[] = { 0, 30, 60, 120 };
                        int i = (int)SendDlgItemMessageW(h, IDC_FPSCAP, CB_GETCURSEL, 0, 0);
                        if (i >= 0 && i < 4) g_work.fps_cap = fpsv[i];
                        preview_dirty(h);
                    }
                    break;
                case IDC_MSAA:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        static const int msv[] = { 0, 2, 4, 8 };
                        int i = (int)SendDlgItemMessageW(h, IDC_MSAA, CB_GETCURSEL, 0, 0);
                        if (i >= 0 && i < 4) g_work.msaa = msv[i];
                        preview_dirty(h);
                    }
                    break;
                case IDC_VSYNC:
                    g_work.vsync = (IsDlgButtonChecked(h, IDC_VSYNC) == BST_CHECKED);
                    preview_dirty(h);
                    break;
                case IDC_AUTOQ:
                    g_work.auto_quality = (IsDlgButtonChecked(h, IDC_AUTOQ) == BST_CHECKED);
                    preview_dirty(h);
                    break;
            }
            return TRUE;
    }
    return FALSE;
}
```
  - No `WM_INITDIALOG` do `dlg_proc`: `ti.pszText = L"Desempenho";
    TabCtrl_InsertItem(tabs, 5, &ti);`; criar `g_perf = CreateDialogW(...,
    MAKEINTRESOURCEW(IDD_TAB_PERF), h, perf_proc);`; `place_tab_child(h, tabs,
    g_perf);` (o `select_tab(0)` já esconde as outras).
  - O mini-preview: `gl_window_set_config(g_preview, &g_work)` já é chamado no
    tick quando `g_dirty` — como o Task 3/5 fez `gl_window_set_config` reavaliar
    `RenderQuality` e o vsync, mexer nos controles de Desempenho já reflete no
    preview (MSAA/escala mudam na hora; fps_cap não afeta o preview, tudo bem).

- [ ] **Step 4: build + selftest headless de cada aba.**
```sh
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile && cp -f dist/Modern3DText.scr dist/Modern3DText.exe
for t in 0 1 2 3 4 5; do MSYS_NO_PATHCONV=1 M3DT_SELFTEST=1 M3DT_TAB=$t ./dist/Modern3DText.exe /c >/dev/null 2>&1 || echo "FAIL tab $t"; done
```
  Depois capturar a aba Desempenho e a Geometria (a que pode cortar) com o
  script `shot_dialog.ps1` da Fase 4a (`-Tab 5` e `-Tab 3`) e **conferir
  visualmente**: 6 abas em 2 fileiras, nenhum controle cortado, os combos com os
  valores certos.

- [ ] **Step 5: `make test`** → `all tests passed` (não deve ter regressão; a aba
  não tem lógica testável por unit).

- [ ] **Step 6: Commit**
```sh
git add src/resource.h res/screensaver.rc src/config_dialog.c
git commit -m "$(printf 'feat: config dialog Performance tab (fps cap, vsync, MSAA, render scale, auto quality)\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 7: integração, QA da Fase 4b, tag

**Files:** `docs/qa-checklist-phase-4b.md` (novo), `README.md`,
`res/screensaver.rc` (versão → 0.4.1), `docs/img/phase4b-*.png`

- [ ] **Step 1:** `mingw32-make -f build/Makefile clean && mingw32-make -f
  build/Makefile` (release) + `mingw32-make -f build/Makefile debug` (clean) +
  `mingw32-make -f build/Makefile test` — os 3 verdes, **sem warnings**; `.scr`
  < 3 MB (esperado ~410 KB).

- [ ] **Step 2: capturas `docs/img/`** (material metálico + `env_path` de teste,
  como na 4a):
  - `phase4b-scale-100.png` / `phase4b-scale-50.png` — mesma cena,
    `render_scale` 1.0 vs 0.5, lado a lado no README.
  - `phase4b-tier-reduced.png` — `M3DT_FORCE_TIER=reduced` (sem bloom, mais
    duro).
  - `phase4b-perf-tab.png` — a aba Desempenho (via `shot_dialog.ps1 -Tab 5`).
  Conferir os 4.

- [ ] **Step 3: `docs/qa-checklist-phase-4b.md`** — seguir o formato dos
  anteriores (`docs/qa-checklist-phase-4a.md`):
  - **Verificado headless (dev):** `make`/`debug`/`test` verdes sem warning;
    `.scr` ~410 KB. `test_config` (perf round-trip + clamps + `version` 1→2).
    `test_render_tiers` (tabela de strings; `render_quality_for_step` em cada
    passo; `aq_decide` ladder + histerese + zona morta). `M3DT_FORCE_TIER=reduced`
    → log `tier: reduced`, sem bloom no PNG. `render_scale` 0.5 → PNG mais macio,
    sem quebra. `msaa` do config chega no FBO (0 → bordas duras, 8 → suaves).
    `vsync=0 fps_cap=30` → run 2× mais lenta que `fps_cap=60`. `M3DT_AQ_FORCE_MS=30
    M3DT_AQ_FAST=1` → log desce degraus em ordem; `=5` → não desce. Sem
    `glError` / FBO incompleto. 10× `/s`, 10× `/c` sem crash.
  - **Falta verificar (interativo):** `/c` aba Desempenho — cada controle reflete
    no mini-preview (MSAA/escala na hora); `OK` grava, `regedit` mostra REG_SZ;
    reabrir mantém. `/p` no diálogo real do Windows. **Multi-monitor**: 2 telas,
    cada uma com seu tier/ladder; desconectar uma a quente. **RDP real** → tier
    reduced automático. GPU fraca real (iGPU) → autoQuality degrada e estabiliza
    (histerese, não fica oscilando). 100/150/200 % DPI: abas Geometria +
    Desempenho sem corte (2 fileiras de aba). 30 min `/s` — VRAM/handles
    estáveis; se autoQuality agir, não sobe/desce em loop.
  - **Notas conhecidas:** o ladder desta fase não tem "streaks off" (Fase 4c). O
    fallback **GDI 2D** (sem GL) é Fase 8 — aqui, se o contexto GL falhar
    totalmente, ainda cai como antes (log + erro). `PrintWindow` não pega a
    janela-filha GL do preview.

- [ ] **Step 4: `README.md`** — bloco **Status** → Fase 4b:
  > **Status:** Fase 4b — **níveis de qualidade** (cheio / reduzido, detecção de
  > GPU de software / RDP / `M3DT_FORCE_TIER`), **render scale** + **VSync** +
  > **limite de FPS** configuráveis, e **qualidade automática** que mede o tempo
  > de GPU (`GL_TIME_ELAPSED`) e degrada em degraus com histerese. Aba
  > **Desempenho** no diálogo. Antes: render HDR + bloom + tonemap ACES (4a); …
  Trocar a imagem do topo por `docs/img/phase4b-scale-100.png` ou manter a da 4a
  (à escolha do executor; a 4a mostra melhor o bloom).

- [ ] **Step 5: versão** — `res/screensaver.rc`: `FILEVERSION 0,4,1,0`,
  `PRODUCTVERSION 0,4,1,0`, `"FileVersion" "0.4.1.0"`, `"ProductVersion"
  "0.4.1.0"`. Rebuild, `cp` pro `.exe`, conferir com
  `(Get-Item dist\Modern3DText.scr).VersionInfo`.

- [ ] **Step 6: rodar o checklist interativo** (o executor faz as partes
  headless e marca; as interativas ficam pro Alan, como nas fases anteriores).

- [ ] **Step 7: Commit + tag**
```sh
git add docs/qa-checklist-phase-4b.md docs/img/phase4b-*.png README.md res/screensaver.rc
git commit -m "$(printf 'feat: phase 4b - quality tiers, render scale, vsync/fps cap, adaptive autoQuality\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
git tag -a v0.4.1-phase4b -m "Fase 4b: niveis de qualidade + auto-qualidade adaptativa"
```

- [ ] **Step 8: finishing-a-development-branch** — rodar `make test` no resultado
  do merge, apresentar as 3 opções (merge local / PR / manter) e executar a
  escolha. Base = `main`.

---

## Self-Review

**1. Cobertura (spec §8.1 passos de infra + §10.1 `perf` + §11):**
- `perf.fpsCap / vsync / msaa / renderScale / autoQuality` no `Config` → Task 1. ✓
- `perf.msaa` limitado a `min(valor, GL_MAX_SAMPLES)` e caindo pro maior
  suportado → `post` já faz `samples>8?8` + `gl_max_samples()` no
  `gl_fbo_hdr_ms`; `render_quality_for_step` nunca pede acima de `cfg.msaa`. ✓
- `perf.renderScale` (0.5–1.0): cena + pós na fração, upscale no passe final →
  Task 3 (`post_begin` interno vs `post_present` saída). ✓
- `render_tiers`: renderer suspeito (`GDI Generic`, `llvmpipe`, `softpipe`,
  `Microsoft Basic Render`, `WARP`) → reduzido → Task 2. `SM_REMOTESESSION` →
  reduzido → Task 2. `M3DT_FORCE_TIER` → Task 2. ✓
- Medição adaptativa (`GL_TIME_ELAPSED`, ~30 frames, > ~22 ms degrada em degraus
  com histerese de 5 s) → Task 5. Ladder do spec:
  `streaks off → bloom off → MSAA 4→2→0 → renderScale 1→0.75→0.5 → reduzido`;
  esta fase omite "streaks off" (não existem) e o degrau final "modo reduzido" é
  representado por `step 6` + tier reduced na 4c. Documentado na Global
  Constraints e nas notas do QA. ✓
- **Modo preview** = sem bloom, sem adaptação → `gl_window` cria com `preview=1`
  → tier FULL fixo, `aq = NULL`, `pr.bloom = 0`. ✓ (da 4a, mantido)
- Controles no diálogo (§10.3, aba **Desempenho**) → Task 6. ✓
- **Fora desta fase:** streaks / CA / vinheta / FXAA (Fase 4c); `fallback_gdi.c`
  (Fase 8). Anotado na Global Constraints. ✓

**2. Placeholders:** nenhum "TODO"/"TBD". Todo código está inline (shaders não
mudam nesta fase). As funções puras (`m3dt_tier_detect`,
`render_quality_for_step`, `aq_decide`) vêm completas e são o núcleo testável;
as partes GL/Win32 (`aq_frame_*`, pacing) vêm completas e são verificadas por
teste headless com PNG + log + `M3DT_AQ_FORCE_MS`. Ordem das tasks: config →
detecção estática → efetivar no render → vsync/fps → adaptativo → UI →
integração. Cada uma compila e tem entregável verificável isolado.

**3. Consistência de tipos:**
- `Config` (+`int fps_cap, vsync, msaa, auto_quality; float render_scale`) —
  Task 1; lido por `render_quality_for_step` (Task 3), `aq_create` (Task 5),
  `host_run_saver` (Task 4), `perf_proc` (Task 6). Faixas dos controles (Task 6:
  slider 50–100, combos {0,30,60,120}/{0,2,4,8}) batem com os clamps (Task 1). ✓
- `M3dtTier { M3DT_TIER_FULL=0, M3DT_TIER_REDUCED=1 }` — `render_tiers.h` (Task
  2), usado por `render_quality_for_step`/`aq_*` (Task 3/5) e `gl_window` (Task
  3). ✓
- `RenderQuality { int bloom, msaa; float render_scale; int step; }` —
  `render_tiers.h` (Task 3), montado por `render_quality_for_step`, mutado por
  `aq_update`, consumido em `gl_window_frame` (`q.msaa`→`post_begin`,
  `q.render_scale`→`sw/sh`, `q.bloom`→`pr.bloom`). ✓
- `AutoQuality` opaco + `AqInput`/`aq_decide` pura — `render_tiers.h` (Task 5);
  `AutoQuality *aq` em `GlWindow` (Task 5); `aq_create/destroy/frame_begin/
  frame_end/update` chamados em `gl_window.c` (Task 5). ✓
- `post_begin(Post*, int, int, int)` / `post_present(Post*, int, int,
  PostParams)` — assinatura nova (Task 3); todos os call sites em `gl_window.c`
  (2: `_frame`, `_render_scene_at`) atualizados no mesmo task. `frame_post_params`
  helper da 4a **removido** no Task 3 Step 4. ✓
- IDs `IDD_TAB_PERF 115` / `IDC_FPSCAP..IDC_AUTOQ` 1600+ — `resource.h` (Task 6);
  6ª aba índice 5; `select_tab` estendido; `g_perf` alternado. `IDC_TABS` ganha
  `TCS_MULTILINE`. ✓
- Hooks de teste: `M3DT_FORCE_TIER` (Task 2), `M3DT_AQ_FORCE_MS` / `M3DT_AQ_FAST`
  (Task 5) — lidos só em `render_tiers.c`, documentados na Global Constraints e
  no QA. ✓

Sem inconsistências.

---

## Execution Handoff

**Plano completo e salvo em
`docs/superpowers/plans/2026-09-10-modern-3d-text-phase-4b-perf-tiers.md`. Duas
opções de execução:**

**1. Subagent-Driven (recomendado)** — um subagente novo por task, revisão entre
tasks, iteração rápida.

**2. Inline Execution** — executa as tasks nesta sessão via `executing-plans`,
com checkpoints pra revisão.

**Qual abordagem?**
