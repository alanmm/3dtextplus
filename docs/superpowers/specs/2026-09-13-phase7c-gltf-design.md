# Fase 7c — Malha glTF/GLB — Documento de Design

## 1. Objetivo

Adicionar suporte a `.glb`/`.gltf` como um terceiro formato de malha
importada, completando a Fase 7 do spec principal. Fecha o conjunto de
formatos previsto (`.obj`/`.stl`/`.glb`/`.gltf`), reaproveitando toda a
infraestrutura já construída nas Fases 7a/7b (normalização, fallback de
erro, o toggle `mesh_use_file_materials`, o esquema de múltiplas
peças).

## 2. Arquitetura

- Novo parser em `src/geometry/mesh_import.c`, usando `cgltf.h`
  (vendorizado, MIT, mesmo padrão de single-header já usado pra
  `nanosvg.h`/`fast_obj.h`). `cgltf_parse_file()` + `cgltf_load_buffers()`
  aceitam tanto `.glb` (binário auto-contido) quanto `.gltf` (JSON +
  `.bin` externo, resolvido automaticamente pela própria biblioteca a
  partir do caminho do arquivo principal) — **sem distinção de código**
  entre os dois: a biblioteca decide sozinha, mesmo caminho estreito
  (`WideCharToMultiByte` + `char*`) já usado pro OBJ/STL.
- Percorre `data->scene->nodes` recursivamente (função interna, não da
  biblioteca); para cada nó com `mesh`, usa
  `cgltf_node_transform_world()` — função pronta da biblioteca que já
  calcula a transformação de mundo compondo toda a cadeia de nós pais,
  sem precisar implementar multiplicação de matrizes na mão — e aplica
  essa transformação em cada vértice de cada `primitive` do mesh antes
  de gerar a geometria final.
- Só `cgltf_primitive_type_triangles` — qualquer outra topologia
  (`triangle_strip`, `triangle_fan`, `points`, `lines`) é ignorada
  (mesmo recorte já documentado no spec principal).
- Posições e normais lidas via `cgltf_accessor_unpack_floats()`
  (converte automaticamente pra float, trata acessores esparsos);
  índices via `cgltf_accessor_unpack_indices()`. Sem leitura manual de
  stride/buffer view — a biblioteca cuida disso.
- **Sem normais no arquivo** → mesmo fallback já usado no OBJ/STL:
  calcula por face, sem suavização.
- Material: só `pbr_metallic_roughness.base_color_factor` vira a cor
  sólida da peça — **sem texturas** (`base_color_texture` ignorada,
  confirmado com o usuário) e **sem `metallic_factor`/
  `roughness_factor`** (o material único configurado na aba Material
  já controla isso, mesma decisão já tomada pro `Ks`/`Ns` do `.mtl` na
  Fase 7b). Um `primitive` sem `has_pbr_metallic_roughness` usa branco
  (`1,1,1`) como fallback de cor — equivalente ao material "fallback"
  branco do `fast_obj.h` na Fase 7b, mesmo espírito.
- Reaproveita o **mesmo toggle** `mesh_use_file_materials`: desligado →
  funde toda a geometria (de todos os nós/primitives) numa malha só
  com o material compartilhado do screensaver — estende
  `mesh_import_load()` (Fase 7a) pra também reconhecer `.glb`/`.gltf`;
  ligado → uma peça por `primitive`, com sua cor extraída do material —
  estende o esquema de peças já criado na Fase 7b
  (`mesh_import_load_pieces()`/`MeshPieceSet`), sem introduzir um tipo
  novo.
- Normalização: mesma função já existente (`normalize_mesh()` pro
  caminho fundido, `normalize_pieces()` pro caminho de peças) — nenhuma
  mudança nelas, só mais uma fonte de dados de entrada.
- Erro ao carregar (arquivo ausente, parse falhou, nenhum triângulo
  válido) → mesmo texto de aviso já usado pros outros formatos.

## 3. Campos de configuração

Nenhum campo novo — reaproveita `mesh_path`, `mesh_size_scale` e
`mesh_use_file_materials`, todos já existentes desde as Fases 7a/7b.

## 4. Interface (aba "Conteúdo")

- O filtro do seletor de arquivo (`GetOpenFileNameW`) ganha mais duas
  entradas: "Arquivos glTF (*.gltf)" e "Arquivos GLB (*.glb)", e a
  entrada combinada passa a ser "Malha 3D (*.obj, *.stl, *.glb,
  *.gltf)".
- Nenhum controle novo — o checkbox "Usar materiais do arquivo" já
  existente (Fase 7b) passa a valer também pra `.glb`/`.gltf`.

## 5. Testes

- `geometry/mesh_import` — fixtures `.gltf` (JSON, mais simples de
  escrever à mão que binário `.glb`) com posições/índices embutidos
  via `data:` URI base64: um triângulo simples (sem material real →
  malha fundida com fallback branco), um arquivo com 2 nós/primitives
  com `base_color_factor` diferentes (→ 2 peças com cores distintas),
  um nó filho com transformação própria (translação) aplicada
  corretamente (verifica `cgltf_node_transform_world`).
- `M3DT_SHOT` visual: um `.gltf`/`.glb` de exemplo com múltiplos
  materiais, toggle ligado e desligado.

## 6. Fora de escopo desta fase

- Texturas (`base_color_texture` e qualquer outra).
- `metallic_factor`/`roughness_factor`/normal maps/emissive/qualquer
  outro canal PBR além da cor base.
- Animações, skinning, câmeras, luzes do arquivo (documentado desde o
  spec principal).
- Topologias além de `TRIANGLES` (`TRIANGLE_STRIP`/`TRIANGLE_FAN`/
  `POINTS`/`LINES`).
- `cgltf_validate()` — confia no parse; um arquivo malformado que passe
  no parse mas falhe semanticamente cai no mesmo fallback de erro
  (proteção suficiente, sem precisar de uma segunda passada de
  validação).
