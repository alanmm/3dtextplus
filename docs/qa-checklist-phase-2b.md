# QA — Fase 2b (configuração)

Buildar (shell do w64devkit, na raiz): `mingw32-make -f build/Makefile`

## Verificado nesta máquina de dev (headless, RTX 5060 Ti, Win 11 26200)

- [x] `make`, `make debug`, `make test` verdes, sem warnings; `.scr` ~262 KB (< 3 MB).
- [x] `test_config`: defaults sensatos, round-trip real no registro (`HKCU\...\Modern3DText_test`),
      clamp de valor fora de faixa (depth "999" → 2.0), valor inválido → default,
      chave limpa no fim.
- [x] Gravar um Config na chave real (`text=ProArt`, `font_family=Arial`, `depth=0.6`,
      `base_color=#E0C060`) e rodar `/s` com `M3DT_SHOT`: o PNG mostra **ProArt em
      Arial**, mais profundo, com a cor quente — a cena lê o registro.
- [x] `M3DT_SELFTEST=1 ...scr /c`: diálogo abre (abas Conteúdo + Movimento, combo de
      fontes populado, sliders), cria o mini-preview GL (`scene: mesh` no log),
      renderiza no timer de 33 ms, e fecha sozinho — `exit=0`, sem `glError`.
- [x] `M3DT_SHOT` no `/c` captura o mini-preview: PNG mostra o texto 3D renderizando
      dentro da área de preview do diálogo.
- [x] Abrir/fechar `/c` 10× seguidas: sem processo órfão, sem vazamento.
- [x] Extração do `gl_window`: `/s` e `/p` renderizam idêntico à Fase 2a (regressão ok).

## Falta verificar (sessão interativa / olho humano)

### /c pelo painel do Windows
- [ ] Botão **Configurações...** abre o diálogo com as duas abas.
- [ ] Aba **Conteúdo**: digitar texto → o mini-preview atualiza em ~0,1 s.
- [ ] Trocar a fonte no combo → preview muda a tipografia.
- [ ] **Negrito** / **Itálico** → preview reflete.
- [ ] **Escolher cor...** → `ChooseColorW` abre; a nova cor aparece no preview.
- [ ] Aba **Movimento**: arrastar **Profundidade** → o relevo do texto muda ao vivo;
      **Ângulo máx.** / **Inclinação X** / **Período** → o pêndulo do preview muda;
      os rótulos numéricos acompanham.
- [ ] **OK** grava em `HKCU\Software\Modern3DText` (conferir no `regedit`); **Aplicar**
      grava sem fechar; **Cancelar** / `X` / `Esc` descartam.
- [ ] Reabrir `/c` → mostra os valores salvos (ou os defaults na 1ª vez).
- [ ] `/s` (botão **Visualizar**) usa a config salva; muda de verdade ao salvar outra.
- [ ] 100 / 150 / 200 % DPI: layout do diálogo sem corte (abas, sliders, botões).

## Notas conhecidas (fases posteriores)

- Só as abas **Conteúdo** e **Movimento** — Material / Fundo / Efeitos / Desempenho
  entram com as respectivas features.
- Sem import/export `.ini`, sem presets, sem i18n PT/EN (Fase 8).
- Preview: o timer de 33 ms roda enquanto o diálogo está aberto; ao trocar de aba
  o preview continua. Debounce = "aplica no próximo tick".
