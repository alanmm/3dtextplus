# QA — Fase 4a (HDR + bloom + tonemap ACES)

Buildar (shell do w64devkit, na raiz): `mingw32-make -f build/Makefile`

## Verificado nesta máquina de dev (headless, RTX 5060 Ti, Win 11 26200)

- [x] `make` (release), `make debug` (clean), `make test` verdes, **sem warnings**;
      `.scr` ~395 KB (< 3 MB).
- [x] `test_config`: round-trip de `bloom_on` / `bloom_threshold` /
      `bloom_intensity` / `bloom_radius`; clamps (`bloom_on` "9"→1, threshold
      "9"→3.0 e "0"→0.2, intensity "9"→2.0, radius "9"→1.0). `reg_get_f` continua
      tolerando vírgula decimal (locale pt-BR).
- [x] Cadeia de pós no `gl_window`: `post_begin` liga o FBO HDR MSAA (RGBA16F +
      depth), `scene_render` desenha nele, `post_present` faz
      `glBlitFramebuffer` (resolve) → bright-pass → 6 mips de downsample (13-tap)
      → upsample tent aditivo → composição `cena + bloom·intensidade` → **tonemap
      ACES + gamma 2.2** no framebuffer 0. Sem `glError`, FBOs completos
      (`glCheckFramebufferStatus`).
- [x] Capturas `/s` (metálico, `env_path` = gradiente de teste) — `docs/img/phase4a-*`:
  - **bloom-off**: só o resolve + tonemap ACES — cena limpa, sem "branco chapado";
  - **bloom-on**: os realces do ambiente sangram um halo suave; transições mais
    macias; o texto "descola" do fundo.
- [x] `/s` selftest (`M3DT_SELFTEST=1`): 90 frames, `exit=0`, sem `glError`,
      renderiza a 60 fps (vsync) sem stutter. Custo por frame não medido
      formalmente (GL_TIME_ELAPSED fica pra fase de perf) — cena trivial, folga
      enorme na RTX.
- [x] `/c` selftest: **5 abas** (Conteúdo/Movimento/Material/Geometria/Efeitos), o
      diálogo alargou p/ caber a 5ª aba numa linha só, mini-preview renderiza
      pela mesma cadeia de pós (bloom ligado no mini-preview), `exit=0`.
- [x] Aba **Efeitos** (headless, `M3DT_TAB=4`): check **Bloom (glare)**, sliders
      **Limiar** 1.05 / **Intensidade** 0.60 / **Espalhamento** 0.55; desmarcar o
      check **desabilita** os 3 sliders (`docs/img/phase4a-effects-tab.png`).
- [x] Estabilidade: abrir/fechar `/c` 10× seguidas — todas `exit=0`, sem crash,
      sem erro de FBO no log (os FBOs do `Post` são recriados só quando muda o
      tamanho e liberados no `post_destroy` com o contexto corrente).

## Falta verificar (sessão interativa / olho humano)

- [ ] `/c` aba **Efeitos**: ligar/desligar **Bloom** → mini-preview muda na hora.
- [ ] Sliders **Limiar** / **Intensidade** / **Espalhamento** → o brilho responde
      ao vivo; com **Intensidade 0** a cadeia de bloom é pulada (só tonemap).
- [ ] `OK` grava (`regedit` → `HKCU\Software\Modern3DText`, valores REG_SZ legíveis);
      reabrir mostra os valores; `/s` usa o que foi salvo.
- [ ] **Preview do painel de controle** (`/p`): sem bloom (só resolve + tonemap),
      sem piscar, acompanha resize da caixinha.
- [ ] **Multi-monitor**: cada janela tem o seu `Post` (FBOs por janela) — os dois
      monitores com bloom, sem vazar VRAM.
- [ ] 100 / 150 / 200 % DPI: aba **Geometria** (a mais cheia) e aba **Efeitos**
      sem corte.
- [ ] ~5 min rodando `/s` — VRAM estável (sem crescer), sem perda de fps.
- [ ] Julgamento visual: o **preset padrão** (limiar 1.05, intensidade 0.6) é
      sutil de propósito; se preferir mais "glare" é só subir a intensidade.
      Comparar com material Clássico e Fosco (menos realce → menos bloom).

## Notas conhecidas (fases posteriores)

- **Fase 4b:** streaks de difração, aberração cromática, vinheta, FXAA,
  `render_tiers` / `autoQuality`, `perf.renderScale`. O `knee()` provisório do
  shader (fase 3a) foi removido — a cena agora emite HDR linear e o clamp é só no
  tonemap.
- `PrintWindow` não captura a janela-filha GL do mini-preview (aparece preta nos
  prints do diálogo). O mini-preview funciona — ver as capturas `/s` e o preview
  ao vivo.
- Hooks de teste novos: `M3DT_TAB=<0..4>` abre o `/c` selftest já naquela aba;
  `M3DT_HOLD_MS=<ms>` estende o timeout do `/c` selftest (para capturas externas).
