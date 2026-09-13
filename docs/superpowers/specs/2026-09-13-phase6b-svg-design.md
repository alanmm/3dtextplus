# Fase 6b — Conteúdo SVG — Documento de Design

## 1. Objetivo

Adicionar um terceiro modo de conteúdo, `svg`, que extrude um arquivo SVG
fornecido pelo usuário, reaproveitando 100% do pipeline de extrusão já
existente (bevel, profundidade, casca oca) — a mesma lógica que o spec
original já previa para esta fase (§9, fase 6 do faseamento).

## 2. Arquitetura

- `ContentMode` (`src/config.h`) ganha `CONTENT_SVG = 2`, ao lado de
  `CONTENT_TEXT = 0` e `CONTENT_CLOCK = 1`.
- Novo módulo `src/geometry/svg_shapes.c`/`.h`, usando a biblioteca
  `nanosvg.h` (single-header, domínio público/zlib, mesmo padrão de
  vendoring já usado para `stb_truetype.h`/`stb_image.h` em
  `third_party/`).
- `nsvgParseFromFile(path, "px", 96.0f)` retorna uma lista encadeada de
  `NSVGshape`. **Cada shape visível vira uma "peça" independente**: seu
  próprio `ContourSet` (um `Contour` por subpath/`NSVGpath`, cúbicas já
  fornecidas pelo nanosvg e achatadas na tolerância de qualidade
  corrente, mesmo helper de flatten usado em `font_outline.c`) e sua
  própria cor de preenchimento (`shape->fill`, quando
  `NSVG_PAINT_COLOR`; gradiente linear/radial vira a média das cores dos
  stops; `NSVG_PAINT_NONE` ou shape só com stroke → sem preenchimento,
  não gera peça nesta fase).
- Cada peça passa pelo **mesmo `contour_mesh_build()`** que já extrude
  texto e relógio — bevel, profundidade e casca oca funcionam
  automaticamente, sem nenhuma mudança nesse código. `SceneRenderer`
  ganha um array de peças SVG (mesh + cor), paralelo ao campo `mesh`
  único já usado por texto/relógio (que continua exatamente como está,
  zero risco de regressão nesses dois modos).
- Ao desenhar: para conteúdo SVG, laço sobre as peças, ajustando a cor
  de cada uma (`material_set_piece_color()`, nova função pequena que só
  troca o uniforme de cor — `material_begin`/`material_set_style`/
  `material_set_bevel` continuam chamados uma vez por frame, como hoje)
  antes do draw call de cada peça. Texto/relógio continuam com um único
  draw call, como hoje.
- Modo de cor (`svgColorMode`): **preservar** (cada peça usa a cor do
  próprio shape do arquivo) ou **cor única** (todas as peças usam
  `material.baseColor`, igual texto/relógio). O material em si (Clássico/
  Metálico/Vidro/Fosco, metalness, roughness) continua sendo o mesmo
  campo global já controlado pela aba Material — sem controle dedicado
  para SVG.
- Sistema de coordenadas do SVG é Y-para-baixo → inverte Y. Normaliza a
  bbox da união de todos os shapes pro tamanho alvo, centralizado
  (mesmo espírito do texto).
- **Sem stroke nesta fase** — só preenchimento. Um shape que só tem
  stroke (sem fill) simplesmente não gera peça e não aparece.
- SVGs "sujos" (pontos muito próximos, preenchimentos não fechados,
  sobreposições) são responsabilidade de quem fornece o arquivo — sem
  tratamento especial de robustez além do que o nanosvg + libtess2 já
  garantem.
- Erro ao carregar (arquivo ausente, parse falhou, nenhuma peça
  resultante) → fallback para um texto curto de aviso (localizado),
  reaproveitando o pipeline de texto existente (`font_build_contours`
  direto no campo `mesh` único, zero peças SVG) — o saver não encerra
  por isso, só loga.

## 3. Campos de configuração

```c
/* content_mode ja existe (ContentMode), so ganha um 3o valor valido */
wchar_t svg_path[512];   /* caminho do arquivo .svg escolhido */
int     svg_color_mode;  /* 0 preservar cores do arquivo, 1 cor unica do material */
```

Default: `svg_path` vazio, `svg_color_mode = 0` (preservar).

## 4. Interface (aba "Conteúdo")

- Combo "Modo" ganha uma 3ª opção: "SVG".
- Modo **SVG**: aparecem um campo de caminho (somente leitura) + botão
  "Procurar..." (`GetOpenFileNameW`, filtro `*.svg`) + um combo "Cores:
  Preservar do arquivo / Cor única do material". Campo de Texto, combo
  de Fonte e os checkboxes de data/segundos do relógio ficam
  desabilitados nesse modo (mesmo padrão de `*_enable()` já usado para
  o modo Relógio). Negrito/Itálico e o botão de Cor continuam visíveis
  mas sem efeito prático em SVG (Negrito/Itálico não se aplicam; Cor só
  importa se `svg_color_mode = cor única`) — não escondidos, para não
  complicar o layout, só sem efeito perceptível quando irrelevantes.
- A aba cresce para 3 modos de conteúdo; layout exato (altura da
  aba, posição dos novos controles) fica pra fase de implementação.

## 5. Testes

- Fixtures `.svg` pequenas e controladas — preenchimento sólido, furo
  via `fill-rule="evenodd"`, `transform` — checando contagem de
  contornos, cor extraída e bbox resultante (`geometry/svg_shapes`,
  determinístico, sem depender de arquivo externo do usuário).
- `M3DT_SHOT` visual do preview com um SVG de exemplo simples (multi-
  shape, multi-cor) confirmando peças com cores distintas e bevel
  aplicado uniformemente.
- Teste de fallback: caminho inexistente → confirma que o texto de
  aviso aparece e o processo não encerra.

## 6. Fora de escopo desta fase

- Stroke (traço/contorno) extrudado.
- Gradientes reais (linear/radial) — viram cor média aproximada.
- `mask`, `clipPath`, `pattern`, filtros SVG, `dash`, `marker`, texto
  dentro do SVG.
- Robustez extra para SVGs malformados/mal-higienizados além do que o
  nanosvg e o libtess2 já oferecem.
- Malha 3D importada (`.obj`/`.glb`/`.gltf`/`.stl`) — fase própria mais
  à frente no spec principal (Fase 7).
