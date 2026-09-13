# Fase 7a — Malha 3D Importada (OBJ + STL) — Documento de Design

## 1. Objetivo

Adicionar um quarto modo de conteúdo, `mesh`, que carrega um arquivo
`.obj` ou `.stl` fornecido pelo usuário e o exibe diretamente (sem
tesselação/extrusão — a malha já é 3D), sempre com o material único do
screensaver (Clássico/Metálico/Vidro/Fosco). Primeira metade da Fase 7
do spec principal (§9); materiais/texturas próprias do arquivo (`.mtl`
do OBJ) e o formato `.glb`/`.gltf` ficam para a 7b, deliberadamente
adiados — são a parte mais trabalhosa (texturas, PBR) e o usuário
confirmou que dá pra ver a malha básica funcionando primeiro.

## 2. Arquitetura

- `ContentMode` (`src/config.h`) ganha `CONTENT_MESH = 3`, ao lado de
  `CONTENT_TEXT/CLOCK/SVG`.
- Novo módulo `src/geometry/mesh_import.c`/`.h` com dois parsers:
  - **OBJ**: via `fast_obj.h` (vendorizado em `third_party/`, licença
    MIT, mesmo padrão de single-header já usado pra `nanosvg.h`/
    `stb_truetype.h`). `fast_obj_read()` recebe caminho estreito
    (`char*`) — convertido de `wchar_t` via `WideCharToMultiByte(CP_UTF8,
    ...)`, **mesmo padrão já usado hoje** em `env_load_texture()`/
    `bg_load_texture()` pra `stbi_load()` (aceita a mesma limitação já
    existente no projeto: caminhos fora da codepage ANSI ativa podem
    falhar — não é uma regressão nova). Extrai `positions`/`normals`/
    `indices`/`face_vertices` de `fastObjMesh`; faces com mais de 3
    vértices (n-gons) são trianguladas por fan simples (`v0,vi,vi+1`);
    materiais/texturas do arquivo (`fastObjMesh.materials`,
    `face_materials`, `map_Kd` etc.) são **ignorados nesta fase**.
  - **STL**: parser próprio em `mesh_import.c` (sem biblioteca externa —
    formato simples o bastante). Detecção binário-vs-ASCII **por
    tamanho do arquivo**, não só pelo prefixo "solid" (alguns STL
    binários também começam com essas 5 letras no cabeçalho de 80
    bytes, um erro clássico de detecção): lê o cabeçalho + contagem de
    triângulos assumindo binário, calcula o tamanho esperado
    (`84 + N*50` bytes) e compara com o tamanho real do arquivo; só
    trata como ASCII se não bater. STL não tem material — sempre usa o
    material compartilhado.
- **Sem tesselação/extrusão**: ao contrário de texto/SVG, a malha
  importada vai direto para o mesmo `MeshData` que `contour_mesh_build()`
  produz pros outros modos (posições, normais, índices, bbox) — sem
  passar por `ContourSet`/libtess2. Como esta fase usa **um único
  material pra malha inteira** (sem cor por submesh), o resultado é
  **uma peça só**, populando o mesmo campo único `s->mesh` que
  texto/relógio já usam — não precisa do esquema de múltiplas peças
  com cor própria criado pro SVG (isso fica pra 7b, quando materiais
  por submesh entrarem).
- `surf` (o campo usado pelo shader pra AO de parede/bevel) é sempre
  `0` pra vértices importados — não há parede/bevel/casca nessa malha,
  então o valor neutro evita qualquer escurecimento indevido.
  Bevel/SDF continuam desligados (mesmo mecanismo que já desliga
  sozinho quando não há textura SDF, reaproveitado sem mudança).
- **Normais ausentes no arquivo** (raro em OBJ, mais comum em STL
  binário mal-formado): calculadas **por face, sem suavização** —
  aparência levemente facetada nesses casos específicos. Suavização
  por limiar de ângulo (como já existe pras paredes do texto extrudado)
  fica documentada como possível melhoria futura, não bloqueia esta
  fase.
- **Normalização**: translada pelo centroide, escala uniformemente
  pra caber numa esfera de raio-alvo, multiplicado por
  `mesh_size_scale` (novo campo, ajuste fino de tamanho, mesmo espírito
  do `particles_size_scale` já existente).
- Erro ao carregar (arquivo ausente, parse falhou, malha vazia/sem
  triângulos válidos) → mesmo fallback de texto de aviso curto já
  usado pro SVG, reaproveitando o pipeline de texto existente sem
  duplicar lógica.

## 3. Campos de configuração

```c
/* content_mode ja existe (ContentMode), so ganha um 4o valor valido */
wchar_t mesh_path[512];       /* caminho do .obj ou .stl escolhido */
float   mesh_size_scale;      /* 0..2, ajuste fino sobre o tamanho normalizado */
```

Default: `mesh_path` vazio, `mesh_size_scale = 1.0`.

## 4. Interface (aba "Conteúdo")

- Combo "Modo" ganha uma 4ª opção: "Malha 3D".
- Modo **Malha 3D**: aparecem um campo de caminho (somente leitura) +
  botão "Procurar..." (`GetOpenFileNameW`, filtro `*.obj;*.stl`) +
  slider de escala (`mesh_size_scale`) — mesmo padrão visual/posição
  do seletor de arquivo do SVG. Texto/Fonte/checkboxes de relógio/
  controles de SVG ficam escondidos (mesmo mecanismo de
  `ShowWindow`/`content_enable()` já usado pros outros 3 modos).

## 5. Testes

- `geometry/mesh_import` — fixtures mínimas geradas em memória (sem
  depender de arquivo externo do usuário, mesmo padrão usado pro SVG:
  escreve um `.obj`/`.stl` pequeno num arquivo temporário e chama o
  parser real): um triângulo simples, um quad (n-gon, prova a
  triangulação por fan), um `.obj` sem normais (prova o cálculo por
  face), um `.stl` binário e um `.stl` ASCII com o mesmo triângulo
  (provam que os dois formatos produzem geometria equivalente) —
  checando contagem de vértices/índices e que a normalização deixa o
  centroide perto de zero e o raio da bbox perto do alvo.
- Teste de fallback: caminho inexistente → confirma que o texto de
  aviso aparece e o processo não encerra (mesmo padrão do SVG).
- `M3DT_SHOT` visual do preview com um `.obj` e um `.stl` de exemplo.

## 6. Fora de escopo desta fase (fica pra 7b ou depois)

- Materiais/texturas próprias do arquivo (`.mtl` do OBJ) e o toggle
  `meshUseFileMaterials`.
- Formato `.glb`/`.gltf` (materiais PBR, texturas embutidas, hierarquia
  de nós) — fica inteiro pra 7b, já que seu modelo de material é
  intrínseco ao formato (não dá pra separar "só geometria" de forma
  natural como em OBJ+.mtl).
- WBOIT pro material Vidro — mantém o blend simples já existente;
  revisita se aparecer um artefato visual real com malhas complexas.
- Suavização de normais por limiar de ângulo quando ausentes no
  arquivo — normais planas por enquanto.
- Animação de malha importada (skinning/morph) — nunca prevista, só
  gira com o pêndulo como qualquer outro conteúdo.
