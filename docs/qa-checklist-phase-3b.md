# QA — Fase 3b (bevel + casca oca)

Buildar (shell do w64devkit, na raiz): `mingw32-make -f build/Makefile`

## Verificado nesta máquina de dev (headless, RTX 5060 Ti, Win 11 26200)

- [x] `make`, `make debug`, `make test` verdes, sem warnings; `.scr` ~383 KB (< 3 MB).
- [x] `test_config`: round-trip de `bevel_mode` / `bevel_size` / `bevel_depth` /
      `bevel_segments` / `shell` / `wall_thickness` / `quality`; clamps
      (bevel_mode "9"→0, segments "1"→2, quality "5"→1, size "9"→≤0.2).
      `reg_get_f` tolera vírgula decimal (locale pt-BR).
- [x] `test_sdf`: círculo (centro ≈ -R, borda ≈ 0, banda externa, gradiente radial);
      quadrado com furo (furo lê como fora); contorno degenerado → falha.
- [x] `test_contour_mesh`: modo sombreado gera SDF + anel de micro-bevel; modo
      geométrico gera a faixa multi-segmento (contagem coerente); "L" côncavo em
      ambos os modos não crasha; casca oca gera anel + paredes internas; parede
      maior que a meia-largura → shell pulado sem crash.
- [x] Capturas (`text=ProArt`, metálico) — `docs/img/phase3b-*`:
  - **bevel-off**: quinas vivas de prisma;
  - **bevel-shading**: realce rolando pela quina (chanfro sombreado via SDF),
    silhueta ainda reta;
  - **bevel-geometry**: chanfro geométrico real, silhueta arredondada;
  - **shell**: texto vazado, cavidades visíveis nas pontas.
  Sem `glError`, sem "contornos clampados" no "ProArt".
- [x] `/c` selftest: 4 abas (Conteúdo/Movimento/Material/Geometria), mini-preview
      renderiza com SDF, `exit=0`, sem `glError`.

## Falta verificar (sessão interativa / olho humano)

- [ ] `/c` aba **Geometria**: trocar o modo de bevel → mini-preview muda na hora.
- [ ] Sliders **Tamanho** / **Profundidade do chanfro** → o relevo da quina muda ao vivo;
      **Segmentos** só habilita no modo Geométrico.
- [ ] **Casca oca** → o texto vaza; slider **Espessura da parede** só habilita com o check.
- [ ] Combo **Qualidade**: Alta deixa o SDF/curvas mais finos (pode demorar no load).
- [ ] **Compare os dois modos de bevel no seu texto real** — em fontes/textos
      específicos o geométrico pode ficar estranho em cantos apertados; o sombreado
      nunca quebra. Escolha o que ficar melhor pra cada conteúdo.
- [ ] `OK` grava; reabrir mostra os valores; `/s` (Visualizar) usa a geometria salva.
- [ ] Tempo de `scene_create` no log — a Qualidade Alta com texto longo deve ficar
      abaixo de ~150 ms.
- [ ] 100 / 150 / 200 % DPI: aba Geometria sem corte (é a aba mais cheia).

## Notas conhecidas (fases posteriores)

- Bevel **sombreado**: a silhueta continua uma quina reta (o SDF só inclina a normal
  das faces). O micro-bevel geométrico suaviza um pouco. Aceito para screensaver
  visto ~de frente.
- Bevel **geométrico**: usa **outset** do contorno (robusto em cantos côncavos, ao
  contrário do inset do spec §5.5). Letra fica ~`bevel_size` maior.
- Casca oca: um offset só; auto-interseção em traço fino → shell pulado naquela peça.
  Interior aberto nas pontas é o efeito pretendido.
