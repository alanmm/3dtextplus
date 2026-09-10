# QA — Fase 4b (níveis de qualidade + auto-qualidade adaptativa)

Buildar (shell do w64devkit, na raiz): `mingw32-make -f build/Makefile`

## Verificado nesta máquina de dev (headless, RTX 5060 Ti, Win 11 26200)

- [x] `make` (release), `make debug` (clean), `make test` verdes, **sem
      warnings**; `.scr` ~402 KB (< 3 MB).
- [x] `test_config`: round-trip de `fps_cap` / `vsync` / `msaa` / `render_scale`
      / `auto_quality` + `version == 2`; snap dos inválidos (`fps_cap` "999"→120,
      `msaa` "7"→8, `render_scale` "3.0"→1.0, `vsync` "5"→1). Registro v1 (sem as
      chaves) carrega com os defaults novos (migração via `config_defaults`).
- [x] `test_render_tiers`:
  - `m3dt_tier_detect`: NVIDIA/Intel/AMD → full; `GDI Generic` / `llvmpipe` /
    `softpipe` / `Microsoft Basic Render` / `WARP` / `SwiftShader` → reduced;
    case-insensitive.
  - `render_quality_for_step`: cada passo do ladder FULL (0 topo → 1 bloom off →
    2/3/4 MSAA 4→2→0 → 5/6 scale 0.75→0.5), clamp de step, REDUCED = 1 passo já
    degradado (bloom 0, msaa≤2, scale≤0.75).
  - `aq_decide`: p90 > 22 ms + histerese ≥ 5 s → sobe de passo; p90 < 12 ms +
    ≥ 5 s → desce; janela morta 12–22 ms → mantém; topo/fundo do ladder.
- [x] `M3DT_FORCE_TIER=reduced` → log `tier: reduced (M3DT_FORCE_TIER)`; PNG sem
      bloom, bordas mais duras (`docs/img/phase4b-tier-reduced.png`).
- [x] `render_scale` 0.5 → PNG visivelmente mais macio (renderiza em 1/2 e faz
      upscale bilinear no passe de composição), texto legível, sem quebra
      (`docs/img/phase4b-scale-50.png` vs `-scale-100.png`).
- [x] `msaa` do config chega no FBO HDR: `post_begin` recebe `q.msaa`; `0` usa
      `gl_fbo_color16f` com depth (sem resolve), `>=2` usa `gl_fbo_hdr_ms` +
      `glBlitFramebuffer`.
- [x] `vsync` / `fps_cap` no loop do saver: log
      `saver: fps_cap=.. vsync=.. msaa=.. render_scale=.. auto_quality=..`;
      90 frames com `vsync=0 fps_cap=30` levam ~2× o de `fps_cap=60`;
      `fps_cap=120` fica perto de 120 (teto suave, granularidade de `Sleep` 1 ms).
- [x] auto-qualidade: `M3DT_AQ_FORCE_MS=30 M3DT_AQ_FAST=1` → log desce
      `autoQuality: step 1 → 2 → 3 …` em ordem, ~0.4 s entre cada (janela FAST);
      `M3DT_AQ_FORCE_MS=5` → 0 mudanças; sem forçar, na RTX → 0 mudanças (cena
      trivial, GPU bem abaixo de 12 ms). Sem `glError`; timer query (core 3.3)
      disponível (`autoQuality: timer query indisponivel` **não** aparece).
- [x] `/c` selftest: **6 abas** (Conteúdo/Movimento/Material/Geometria/Efeitos/
      **Desempenho**), `TCS_MULTILINE` no tab control, `exit=0`. Combos de FPS/
      MSAA e slider de escala carregam os valores do registro
      (`docs/img/phase4b-perf-tab.png`). Aba Geometria (a mais cheia) não corta
      com o diálogo mais alto (250 dip).
- [x] Estabilidade: 10× `/s` + 10× `/c` — todas `exit=0`, sem crash, sem erro de
      FBO / query no log. `AutoQuality` e `Post` liberados no `gl_window_destroy`
      com o contexto corrente.

## Falta verificar (sessão interativa / olho humano)

- [ ] `/c` aba **Desempenho**: mudar MSAA / escala de render → mini-preview
      responde na hora; `fps_cap` não afeta o preview (esperado). `VSync` on/off
      e `Qualidade automática` on/off gravam.
- [ ] `OK` grava (`regedit` → `HKCU\Software\Modern3DText`, valores REG_SZ
      legíveis, `version` = `2`); reabrir mostra os valores; `/s` usa o salvo.
- [ ] `/p` no diálogo real de Proteção de Tela do Windows — anima, sem bloom
      (modo preview), acompanha o resize da caixinha, libera limpo.
- [ ] **Multi-monitor**: 2 telas, cada janela tem seu `tier` + `AutoQuality`
      (FBOs por janela); desconectar uma a quente; sem vazar VRAM.
- [ ] **Sessão RDP real** → tier `reduced` automático (log). GPU fraca real
      (iGPU Intel) → auto-qualidade degrada e **estabiliza** (histerese de 5 s,
      não fica oscilando entre passos).
- [ ] 100 / 150 / 200 % DPI: abas **Geometria** e **Desempenho** sem corte.
- [ ] 30 min rodando `/s` — VRAM / handles GDI/USER estáveis; se a
      auto-qualidade agir, não sobe/desce em loop.

## Notas conhecidas (fases posteriores)

- O ladder desta fase **não** tem o degrau "streaks off" (streaks entram na
  Fase 4c). Quando entrarem, o degrau vai pro topo do ladder (antes de "bloom
  off").
- O **fallback GDI 2D** (sem contexto GL nenhum) é da Fase 8. Aqui, se o
  contexto GL falhar de vez, ainda cai como antes (log + erro), sem tela 2D.
- `PrintWindow` não captura a janela-filha GL do mini-preview → aparece preta
  nos prints do diálogo. O preview funciona (ver os prints `/s`).
- Hooks de teste novos: `M3DT_FORCE_TIER=full|reduced`,
  `M3DT_AQ_FORCE_MS=<ms>` (injeta tempo de frame), `M3DT_AQ_FAST=1` (janelas de
  histerese curtas p/ teste headless).
- `fps_cap` é um teto suave via `Sleep` (resolução ~1 ms com `timeBeginPeriod`),
  não um limitador preciso; o `vsync` continua sendo o caminho recomendado.
