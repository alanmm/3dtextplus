# QA — Fase 2a (texto 3D extrudado)

Buildar (shell do w64devkit, na raiz): `mingw32-make -f build/Makefile`

## Verificado nesta máquina de dev (headless, RTX 5060 Ti, Win 11 26200)

- [x] `make`, `make debug`, `make test` verdes, sem warnings; `.scr` ~237 KB (< 3 MB).
- [x] Testes unitários: `mathx` (20 asserções — matrizes, pêndulo), `font_outline`
      ('A' ≥ 2 contornos, multi-linha, fallback de fonte), `contour_mesh`
      (quadrado / furo / "L" / degenerado — normais de tampa ±Z, paredes ⟂ Z).
- [x] `M3DT_SELFTEST=1 M3DT_SHOT=out.png [M3DT_SHOT_T=<s>] Modern3DText.scr /s`
      gera um PNG. Conferido visualmente em 3 fases do pêndulo:
      "Modern 3D Text" em relevo 3D, glifos corretos (furos de o/d/e/R/D),
      material especular azulado, enquadrado com folga para o giro de ±42°.
      Sem `glError` no log.
- [x] Contexto GL 3.3 core via glad; loader com fallback para `opengl32.dll`.

## Falta verificar (sessão interativa / olho humano)

### /s (saver) — real, botão "Visualizar"
- [ ] Texto 3D girando em pêndulo suave, nunca mostra o verso invertido.
- [ ] Enquadramento correto em cada monitor (proporções diferentes → auto-fit).
- [ ] Nas extremidades do arco o texto pode chegar perto das bordas mas não é cortado.
- [ ] Sai no input (mouse > 4 px / tecla / clique / roda). Cursor some/volta.
- [ ] Multi-monitor: os dois monitores com o texto, cada um enquadrado.

### /p (preview) — painel de Proteção de Tela
- [ ] A mini-tela mostra o texto 3D girando (não só a cor de fundo).
- [ ] Fechar o painel não deixa `Modern3DText.scr` órfão.

### /c (config)
- [ ] Ainda é o diálogo stub da Fase 1 (a config real é a Fase 2b). OK/Cancelar fecham.

### Robustez
- [ ] 5 min rodando: memória e handles GDI/USER estáveis (Process Explorer).
- [ ] `%LOCALAPPDATA%\Modern3DText\log.txt` registra `scene: mesh hx/hy/hz` e
      a versão do contexto GL; trunca sozinho ao passar de ~200 KB.
- [ ] GPU fraca / RDP / VM: hoje, se o contexto 3.3 falhar, a cena não desenha
      (o fallback GDI 2D real é a Fase 3). Registrar o que acontece.

## Notas conhecidas (endereçadas em fases posteriores)

- Sem antialiasing → bordas serrilhadas (MSAA entra na Fase 4).
- Sem bevel → quinas vivas (bevel SDF/geometria + micro-bevel na Fase 3).
- Faixa clara na base das letras: iluminação two-sided das paredes; ajuste fino
  do material vem com os outros modos (Fase 3+).
- Fundo fixo azul-escuro; fundos configuráveis na Fase 5.
