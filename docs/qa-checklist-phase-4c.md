# QA — Fase 4c (streaks de difração: starburst + anamórfico)

Buildar (shell do w64devkit, na raiz): `mingw32-make -f build/Makefile`

## Verificado nesta máquina de dev (headless, RTX 5060 Ti, Win 11 26200)

- [x] `make` (release), `make debug` (clean), `make test` verdes, **sem
      warnings**; `.scr` release **407040 bytes** (~398 KB, < 3 MB); `.scr` debug
      515072 bytes (< 3 MB). `clean` antes do release e antes do debug.
- [x] `test_config`: round-trip de `streaks_mode` / `streaks_intensity` /
      `streaks_length` + `version == 2` (streaks entraram no schema v2, sem bump).
      Clamps de registro cru: `streaks_mode` "9" → 0 (fora de 0..2),
      `streaks_intensity` "-1" → clamp ≥ 0 (faixa 0..2), `streaks_length` "5" →
      clamp ≤ 1.0. Registro v1 (sem as chaves) carrega com os defaults novos
      (`streaks_mode` 0, `intensity` 0.5, `length` 0.5).
- [x] `test_render_tiers`: ladder FULL agora tem **8 passos**
      (`render_quality_ladder_len(FULL) == 8`); ordem: 0 topo → **1 streaks off**
      → 2 bloom off → 3 msaa≤4 → 4 msaa≤2 → 5 msaa 0 → 6 scale≤0.75 → 7 scale 0.5.
      `render_quality_for_step`: passo 0 `streaks==1`, passo 1 `streaks==0 &&
      bloom==1` (streaks é o **primeiro** degrau, antes de bloom), passo ≥ 2
      `streaks==0 && bloom==0`; clamp de step → `step==7`. `REDUCED` = 1 passo já
      degradado (`streaks==0`, bloom 0, msaa ≤ 2, scale ≤ 0.75). `aq_decide`
      inalterado (histerese ≥ 5 s, zona morta 12–22 ms, topo/fundo do ladder;
      `ladder_len` agora 8).
- [x] Capturas `/s` (metálico, `env_path` = gradiente de teste, bloom on) —
      `docs/img/phase4c-*`:
  - **streaks-off** (`streaks_mode=0`): só bloom — halo suave em volta do texto,
    sem raios;
  - **starburst** (`streaks_mode=1`): estrela limpa de **6 pontas** (3 eixos a
    0°/60°/120°) saindo dos realces; ACES segura o brilho, sem estouro de branco;
  - **anamorphic** (`streaks_mode=2`): faixa **horizontal** contínua e macia com
    tinta azulada suave; sem raios verticais/diagonais.
- [x] `phase4c-effects-tab.png`: aba **Efeitos** (a mais cheia agora) com Bloom +
      3 sliders **e** a seção Streaks (combo `Starburst` + Intensidade +
      Comprimento) numa única aba, nada cortado.
- [x] Gate de qualidade: `pr.streaks_mode` só passa se
      `!g->preview && g->quality.streaks && g->post_params.streaks_mode`
      (`gl_window.c`). `post_present`: `has_streaks` exige `streaks_mode != 0 &&
      streaks_intensity > 1e-4` — intensidade 0 pula toda a cadeia de streak.
- [x] `M3DT_FORCE_TIER=reduced` + `streaks_mode=1` → log `tier: reduced
      (M3DT_FORCE_TIER)`; captura **sem bloom e sem streaks**, bordas mais duras
      (tier reduzido zera `q.streaks`).
- [x] `M3DT_AQ_FORCE_MS=30 M3DT_AQ_FAST=1` + `streaks_mode=1 auto_quality=1` →
      **primeira** linha `autoQuality: step 1 (streaks=0 bloom=1 msaa=4
      scale=1.00)` — o screensaver desliga streaks **antes** de qualquer outra
      degradação; passo 2 seguinte desliga bloom.
- [x] Cadeia de streak (`post.c`): bright-pass da cena → 1/4 de resolução
      (`gl_fbo_r11f`) → por eixo, 3 iterações de blur direcional exponencial
      (passos 1, 3, 9; anamórfico usa passo base 1.7× — o comprimento não é
      reescalado) →
      último blur escreve aditivo no `streaks_acc` → composição
      `+ streaks * streaks_intensity` antes do tonemap. 4 FBOs de 1/4 res por
      janela (`streak_src` / `streak_a` / `streak_b` / `streaks_acc`), criados em
      `ensure_size`, liberados em `post_destroy`.
- [x] Sem `glError` / FBO incompleto / `timer query indisponivel` no log.
      10× `/s` + 10× `/c` — todas `exit rc=0`, sem crash, sem erro de FBO/query.

## Falta verificar (sessão interativa / olho humano)

- [ ] `/c` aba **Efeitos**: trocar o combo Streaks entre **Starburst** /
      **Anamórfico** / **Desligado** → o mini-preview muda na hora; sliders de
      **Intensidade** / **Comprimento** respondem ao vivo; o combo em
      **Desligado** desabilita os 2 sliders.
- [ ] `OK` grava (`regedit` → `HKCU\Software\Modern3DText`, valores REG_SZ
      legíveis: `streaks_mode` / `streaks_intensity` / `streaks_length`,
      `version` = `2`); reabrir mostra os valores; `/s` usa o salvo.
- [ ] `/p` no diálogo real de Proteção de Tela do Windows — anima **sem streaks**
      (modo preview), acompanha o resize da caixinha, libera limpo.
- [ ] **Multi-monitor**: 2 telas, cada janela com seus 4 FBOs de streak (1/4 res);
      desconectar uma a quente; sem vazar VRAM.
- [ ] 100 / 150 / 200 % DPI: aba **Efeitos** (a mais cheia agora, com bloom +
      streaks) sem corte.
- [ ] 5 min rodando `/s` com streaks ligados — VRAM / handles GDI/USER estáveis
      (4 FBOs de 1/4 res por janela); se a auto-qualidade agir, não fica
      subindo/descendo em loop.

## Notas conhecidas

- O **anamórfico** usa tinta azulada suave aplicada em **toda** iteração de
  blur — é o look pretendido (lente anamórfica clássica), não um bug de cor.
- Os streaks vêm de um bright-pass em **1/4 de resolução** — como são borrados
  por definição, 1/4 basta e mantém o custo baixo.
- O **comprimento do streak é absoluto em texels do buffer de 1/4 de
  resolução**, não relativo à tela — o mesmo `streaks_length` produz um rastro
  proporcionalmente mais curto em 4K e mais longo em 720p, e **diminuir a
  Escala de Render alonga os streaks** (o buffer de streak encolhe junto com o
  alvo interno). O bloom não tem esse problema (mips em contagem fixa). Corrigir
  exigiria um 4º passe de blur ou mais amostras em resolução alta — fica pra
  Fase 4d; não tentar um reescalonamento ingênuo de `base_step` por
  resolução, pois isso reintroduz o banding que as 3 correções desta fase
  removeram.
- O nº de pontas do starburst é **fixo em 6** (3 eixos a 0°/60°/120°). O controle
  de pontas configuráveis do spec §10.1 **não foi exposto** (decisão do usuário:
  só modo + intensidade + comprimento).
- **Aberração cromática**, **vinheta** e **FXAA** + a reestruturação da cadeia de
  pós são da **Fase 4d**.
- Hooks de teste (todos pré-existentes): `M3DT_SELFTEST`, `M3DT_SHOT` /
  `M3DT_SHOT_T`, `M3DT_LOG_APPEND`, `M3DT_TAB=<0..5>`, `M3DT_HOLD_MS=<ms>`,
  `M3DT_FORCE_TIER=full|reduced`, `M3DT_AQ_FORCE_MS=<ms>` / `M3DT_AQ_FAST=1`.
  Log em `%LOCALAPPDATA%\Modern3DText\log.txt`.
- `PrintWindow` não captura a janela-filha GL do mini-preview → aparece preta nos
  prints do diálogo (a captura `phase4c-effects-tab.png` usa screen-grab). O
  preview funciona ao vivo.
