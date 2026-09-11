# QA — Fase 4d (aberração cromática + vinheta + FXAA)

Buildar (shell do w64devkit, na raiz): `mingw32-make -f build/Makefile`

## Verificado nesta máquina de dev (headless, RTX 5060 Ti, Win 11 26200)

- [x] `make` (release, clean antes), `make debug` (clean antes), `make test`
      verdes, **sem warnings**; `.scr` release **414208 bytes** (~405 KB,
      < 3 MB); `.scr` debug 526336 bytes (< 3 MB).
- [x] `test_config`: round-trip de `chroma_on` / `chroma_strength` /
      `vignette_on` / `vignette_amount` / `fxaa_on` + `version == 2` (sem bump
      — mesmo padrão da 4c). Clamps de registro cru: `chroma_on` "7" → 1 (≠0),
      `chroma_strength` "9" → clamp ≤ 1.0, `vignette_amount` "-3" → clamp ≥ 0,
      `fxaa_on` "5" → 1 (≠0). Registro sem essas chaves carrega com os
      defaults (`chroma_on=0`, `vignette_on=0`, `fxaa_on=1`).
- [x] Cadeia de pós reestruturada em 3 passes (era 1 — `post_composite.frag`
      virou `post_combine.frag`, sem tonemap): **combinar** (cena+bloom+
      streaks, HDR, `p->comp` em resolução de *saída*) → **finalizar** (CA +
      vinheta + ACES + gamma, `post_finish.frag`, HDR→LDR) → **FXAA**
      (opcional, `post_fxaa.frag`, LDR→LDR, só roda se `fxaa_on`). Sem CA/
      vinheta/FXAA a imagem é **visualmente idêntica** à Fase 4c — o mesmo
      cenário capturado antes (`t3_none.png`, Task 3) e depois (Task 4/5) da
      aba nova é indistinguível a olho; não é bit-a-bit igual às capturas
      *da 4c* porque o cenário de teste (texto/cor/ângulo) mudou entre fases,
      só a reestrutura interna (combinar→finalizar→FXAA) foi conferida como
      um no-op quando os 3 novos efeitos estão desligados.
- [x] Capturas `/s` (metálico, `env_path` = gradiente de teste, bloom on) —
      `docs/img/phase4d-*`:
  - **none** (tudo desligado): idêntico ao baseline 4c.
  - **chroma** (`chroma_on=1 strength=0.9`): franjas vermelho/azul nas bordas
    das letras, quase imperceptível no centro (o deslocamento escala com `r²`
    a partir do centro da tela).
  - **vignette** (`vignette_on=1 amount=0.7`): cantos visivelmente mais
    escuros, centro inalterado.
  - **all** (CA + vinheta + FXAA juntos): os dois efeitos + bordas do texto
    ligeiramente mais suaves (FXAA).
  - **post-tab**: aba **Pós** (7ª aba) com os 3 controles.
- [x] Gate de qualidade: `pr.chroma_on`/`vignette_on`/`fxaa_on` só passam se
      `!g->preview && g->tier != M3DT_TIER_REDUCED` (`gl_window.c`) —
      **direto**, sem passar por `RenderQuality`/ladder (CA/vinheta/FXAA são
      baratos, não entram na auto-qualidade adaptativa; o ladder continua com
      8 passos da Fase 4c).
- [x] `M3DT_FORCE_TIER=reduced` + os 3 ligados no registro → captura **sem**
      CA/vinheta/FXAA (mesmo comportamento de bloom/streaks); log `tier:
      reduced (M3DT_FORCE_TIER)`. Sem `glError` / FBO incompleto.
- [x] `/c` selftest: **7 abas** (Conteúdo/Movimento/Material/Geometria/
      Efeitos/Desempenho/**Pós**), `exit=0` nas 7. `TCS_MULTILINE` quebra
      automaticamente em 2 linhas (4 abas numa linha, 3 na outra) — sem
      mudança de tamanho do diálogo, confirmado por captura
      (`docs/img/phase4d-post-tab.png`): mini-preview e OK/Cancelar/Aplicar
      no lugar de sempre, nada cortado.
- [x] Sem `glError` / FBO incompleto no log. 10× `/s` + 10× `/c` — todas
      `exit=0`, sem crash.

## Falta verificar (sessão interativa / olho humano)

- [ ] `/c` aba **Pós**: ligar/desligar CA e Vinheta → mini-preview muda na
      hora; sliders de intensidade respondem ao vivo; desmarcar o check
      desabilita o slider correspondente; FXAA sem slider (só toggle).
- [ ] `OK` grava (`regedit` → `HKCU\Software\Modern3DText`, valores REG_SZ
      legíveis: `chroma_on`/`chroma_strength`/`vignette_on`/`vignette_amount`/
      `fxaa_on`); reabrir mostra os valores; `/s` usa o salvo.
- [ ] `/p` no diálogo real de Proteção de Tela do Windows — anima **sem**
      CA/vinheta/FXAA (modo preview), acompanha o resize da caixinha.
- [ ] **Multi-monitor**: 2 telas, cada janela com seus 2 FBOs novos (`comp`/
      `ldr`, em resolução de *saída* — maiores que os de bloom/streak quando
      `render_scale < 1`); sem vazar VRAM.
- [ ] 100 / 150 / 200 % DPI: 7 abas sem corte (a mais provável de quebrar
      linha diferente agora — conferir em DPI alto).
- [ ] 5 min rodando `/s` com CA + vinheta + FXAA ligados — VRAM/handles
      GDI/USER estáveis.
- [ ] Julgamento visual: comparar de perto (zoom) as bordas do texto com FXAA
      ligado vs desligado — deve suavizar serrilhado residual sem borrar o
      relevo do bevel.

## Notas conhecidas (fases posteriores)

- O comprimento do streak (Fase 4c) continua absoluto em texels do buffer, não
  relativo à tela/`render_scale` — decisão do usuário 2026-09-11: continuar
  adiado (ver `docs/qa-checklist-phase-4c.md`).
- **§8.1 do spec está fechado com esta fase** — os 10 passos do frame graph
  de pós-processamento (cena → bloom → streaks → combinar → CA → vinheta →
  tonemap → FXAA) estão todos implementados. As próximas fases (5+) não
  tocam mais a cadeia de pós — mexem em conteúdo (SVG/relógio), fundo,
  partículas, malha importada, i18n e distribuição.
- A constante de escala do deslocamento de CA (`0.02` em `post_finish.frag`)
  é uma decisão de implementação — o spec dá a fórmula (`dir * strength * r²`)
  mas não a constante; ajustável se o efeito parecer forte/fraco demais no
  julgamento visual.
- Hooks de teste (todos pré-existentes, nenhum novo nesta fase):
  `M3DT_SELFTEST`, `M3DT_SHOT`/`M3DT_SHOT_T`, `M3DT_LOG_APPEND`,
  `M3DT_TAB=<0..6>` (subiu de `<0..5>`), `M3DT_HOLD_MS`, `M3DT_FORCE_TIER`,
  `M3DT_AQ_FORCE_MS`/`M3DT_AQ_FAST`.
- `PrintWindow` captura a janela-filha GL do mini-preview como preta (mesma
  limitação da Fase 4a) — as capturas `/s` mostram o efeito real.
