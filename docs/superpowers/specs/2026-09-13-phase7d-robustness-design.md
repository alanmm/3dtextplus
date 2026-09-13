# Fase 7d — Limite de Tamanho + Placa de Erro 2D — Documento de Design

## 1. Objetivo

Dois ajustes de robustez/UX que surgiram do uso real da Fase 7c:

1. Um arquivo de malha muito grande (testado: ~400MB `.obj`) quase travou o
   app ao ser carregado sem nenhum aviso — falta um limite de tamanho.
2. A mensagem de erro exibida quando o SVG/malha configurado está ausente
   ou inválido usa o pipeline de texto 3D extrudado normal, o que fica
   praticamente ilegível (a mesma limitação que motivou o polimento de
   texto da Fase 6a) — precisa virar algo legível.

## 2. Limite de tamanho de arquivo de malha

- Constante única `MESH_IMPORT_MAX_BYTES = 10 MB`, definida uma vez em
  `mesh_import.h`, reaproveitada em todos os pontos de checagem abaixo.
  Não vira campo de configuração (YAGNI — se precisar ser ajustável no
  futuro, adiciona-se depois).
- **Camada autoritativa** (`mesh_import.c`): `mesh_import_load()` e
  `mesh_import_load_pieces()` checam o tamanho do arquivo em `path` via
  `GetFileAttributesExW` (sem abrir o arquivo) antes de chamar qualquer
  parser. Acima do limite, retornam 0 imediatamente — mesmo contrato de
  falha que já existe hoje (arquivo ausente/extensão desconhecida), então
  o erro segue o MESMO caminho de exibição já usado por essas falhas (que,
  após esta fase, é a placa 2D descrita na seção 3 — não precisa de
  tratamento especial).
- **Caso `.gltf` específico**: como o `.bin` é externo, o `.gltf`
  principal pode ser pequeno mesmo com um buffer gigante. `cgltf_parse_file()`
  já lê o `byteLength` declarado de cada buffer no JSON antes de carregar
  os bytes de verdade (`cgltf_buffer.size`) — soma-se esses tamanhos
  declarados e recusa-se (sem nunca abrir o `.bin`) se passar do limite,
  ANTES de chamar `cgltf_load_buffers()`. `.glb` não precisa dessa camada
  extra porque já é um único arquivo, coberto pela checagem autoritativa.
- **Feedback imediato na UI** (`config_dialog.c`): uma função exportada
  `mesh_import_file_too_big(const wchar_t *path)` (mesma constante, sem
  duplicar lógica) é chamada logo após `GetOpenFileNameW` retornar sucesso
  no seletor de malha (`IDC_MESHPICK`). Se o arquivo exceder o limite,
  mostra um `MessageBoxW` explicando o motivo e NÃO atualiza
  `g_work.mesh_path` (mantém a seleção anterior) — evita que o preview
  sequer tente processar o arquivo grande.

## 3. Placa de erro 2D (texto + caixa achatados)

Não é uma sobreposição de tela (HUD) — é um objeto "placa" comum na cena
3D (posicionado, rotacionado e enquadrado pela câmera como qualquer outro
conteúdo), só que achatado no eixo Z, replicando o efeito que já apareceu
nos testes de SVG achatado (fill sem extrusão).

- **Onde entra**: os 3 pontos que já existem em `scene.c` — SVG sem forma
  preenchida, SVG sem geometria válida, malha ausente/inválida. Em vez de
  `strncpy(s->text, "...", ...); return rebuild_mesh(s);`, cada um chama
  uma nova função `build_error_plaque(s, "mensagem\ncom quebras")`.
- **Texto (preto)**: construído pelo MESMO pipeline de sempre
  (`font_build_contours()` → `contour_mesh_build()`), mas com
  `MeshParams.depth = 0` e `bevel_mode` desligado (valor 2 — "desligado"),
  reproduzindo o efeito de placa achatada. Duas diferenças em relação ao
  texto normal: (a) a família de fonte é fixada em `L"Segoe UI"`
  (a mesma que `font_outline.c` já usa como fallback quando nenhuma é
  escolhida), independente da fonte configurada pelo usuário para o
  conteúdo normal; (b) a quebra de linha é fixa, embutida na própria
  string de cada mensagem (reaproveitando o suporte a `\n` que o pipeline
  multi-linha já tem desde o polimento da Fase 6a) — só existem 3
  mensagens conhecidas, não precisa de wrap automático.
- **Caixa (cinza-claro)**: um quad achatado simples, 4 vértices / 2
  triângulos (sem tessellation, sem biblioteca), dimensionado para cobrir
  a bounding box do texto (minx/maxx/miny/maxy do `MeshData` do texto) mais
  uma margem, posicionado ligeiramente atrás no Z (`z = -epsilon`, um
  valor pequeno o bastante para não competir com o Z do texto mas evitar
  z-fighting). Cor e margem exatas ficam sujeitas a ajuste visual (mesmo
  processo iterativo já usado para `LINE_HEIGHT_MULT` na Fase 6a) —
  ponto de partida: cinza `(0.85, 0.85, 0.85)`, texto preto `(0, 0, 0)`,
  margem ~25% da altura do texto.
- **Armazenamento e renderização**: um novo par dedicado
  `GlMesh error_pieces[2]; v3 error_colors[2]; int have_error_plaque;` em
  `SceneRenderer` (nomeado à parte de `svg_*`/`mesh_*` porque o estado de
  erro não pertence a nenhum dos dois — pode ser disparado tanto por SVG
  quanto por malha). Desenhado com o MESMO padrão já usado 3x no projeto
  (SVG multi-cor, materiais OBJ, glTF): loop chamando
  `material_set_piece_color(&s->mat, s->error_colors[i])` +
  `gl_mesh_draw(&s->error_pieces[i])`, caixa primeiro, texto depois.
- **Enquadramento de câmera**: automático, reaproveitando a MESMA lógica
  de `hx/hy/hz` que já centraliza qualquer conteúdo — a placa vira só mais
  uma fonte de bounds, sem nenhuma lógica de câmera nova. Na prática, o
  guard de early-return de `scene_render()` (hoje
  `if (!s->have_mesh && s->svg_mesh_count == 0 && s->mesh_piece_count == 0) return;`)
  ganha mais uma condição (`&& !s->have_error_plaque`), e o dispatch de
  desenho (o `if/else if/else` que já escolhe entre SVG/malha/texto normal)
  ganha um novo ramo de prioridade máxima para `have_error_plaque` — o
  resto da função (setup de câmera, luz, matrizes) continua exatamente
  igual, sem duplicação.
- **Limpeza**: uma nova `free_error_plaque(s)` (mesmo padrão de
  `free_svg_pieces`/`free_mesh_pieces`) chamada no início de TODAS as
  funções de rebuild (`rebuild_mesh`, `rebuild_svg_mesh`,
  `rebuild_imported_mesh`) — garante que trocar de modo de conteúdo ou
  carregar um arquivo válido sempre limpa um estado de erro anterior antes
  de decidir se precisa recriar um novo. Também chamada em
  `scene_destroy()`.

## 4. Campos de configuração

Nenhum campo novo para nenhuma das duas partes desta fase.

## 5. Interface (aba "Conteúdo")

Nenhum controle novo. Único efeito de UI: o seletor de malha
(`IDC_MESHPICK`) recusa um arquivo acima de 10MB na hora, via
`MessageBoxW`, sem alterar a seleção atual.

## 6. Testes

- **Limite de tamanho** (`build/tests/test_mesh_import.c`): um arquivo
  `.obj`/`.glb` de ~11MB de conteúdo de preenchimento pra confirmar que
  `mesh_import_load()`/`mesh_import_load_pieces()` recusam antes de
  tentar parsear; um `.gltf` pequeno com `byteLength` declarado acima do
  limite (sem precisar de um `.bin` de verdade) pra confirmar a checagem
  de buffer externo; teste direto de `mesh_import_file_too_big()` com um
  arquivo pequeno normal (aceita).
- **Placa de erro**: não dá pra cobrir com os testes automatizados
  headless de sempre (é geometria 3D real, mas a verificação visual É o
  ponto). Verificação via capturas reais (`M3DT_SHOT`) dos 3 casos de
  erro (SVG sem forma, SVG vazio, malha inválida), confirmando: caixa
  cinza + texto preto legíveis, fundo/partículas ainda visíveis por trás,
  e que a placa some assim que um arquivo válido é configurado.

## 7. Fora de escopo

- Tornar o limite de 10MB configurável.
- Aplicar a mesma checagem de tamanho ao seletor de SVG (não foi
  relatado como problema).
- Wrap automático de texto na placa de erro (só quebras fixas nas 3
  mensagens conhecidas).
- Qualquer sobreposição de tela fixa (HUD 2D em screen-space) — descartada
  em favor da placa 3D achatada.
- Unificar `svg_*`/`mesh_*`/`error_*` num tipo único de "conjunto de
  peças coloridas" (candidato já anotado desde a Fase 7c, permanece
  adiado).
