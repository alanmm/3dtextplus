# Fase 7b — Materiais do Arquivo OBJ (.mtl) — Documento de Design

## 1. Objetivo

Adicionar suporte a cores próprias por submesh vindas do `.mtl` de um
arquivo `.obj` importado (Fase 7a), com um toggle pra escolher entre
usar essas cores ou o material único do screensaver. Segunda metade da
Fase 7 do spec principal; `.glb`/`.gltf` fica pra 7c (parser
inteiramente novo, adiado deliberadamente); texturas de imagem
(`map_Kd`) ficam de fora — só cor sólida por submesh, confirmado com o
usuário.

## 2. Arquitetura

- Novo campo de config: `mesh_use_file_materials` (0/1, default 0).
- `src/geometry/mesh_import.c` ganha `mesh_import_load_pieces()`, que
  reaproveita o parse do `.obj` já feito na Fase 7a (`fast_obj.h`) mas
  agrupa as faces por `face_materials[]` em vez de fundir tudo numa
  malha só. Cada grupo vira uma peça independente (`MeshPiece`: um
  `MeshData` + uma cor RGB), extraindo `Kd` do material correspondente
  — mesmo espírito do esquema "peça com cor própria" já criado pro SVG
  na Fase 6b (`SvgShapeSet`/`SvgPiece`), com um tipo próprio
  (`MeshPieceSet`/`MeshPiece`) em vez de reaproveitar o do SVG (os dois
  ficam ativos em modos de conteúdo mutuamente exclusivos, mas nomear
  algo "svg_*" e usar pra malha seria confuso — reavaliar unificação
  só se um terceiro consumidor aparecer, ex. o glTF da 7c).
- **Materiais "fallback" são ignorados**: quando um `.obj` não
  referencia nenhum `.mtl` de verdade, o `fast_obj.h` cria
  internamente um material branco marcado como `fallback` (confirmado
  lendo o header vendorizado). Se TODOS os materiais do arquivo forem
  fallback (ou não houver nenhum), `mesh_import_load_pieces()` retorna
  falha — o chamador cai automaticamente no caminho já existente da
  Fase 7a (malha única, material compartilhado), evitando uma cor
  branca arbitrária aparecer quando o arquivo simplesmente não tem
  material de verdade.
- `.stl` nunca tem material (formato não suporta) — o toggle não tem
  nenhum efeito nesse formato; sempre usa o caminho de malha única já
  existente.
- `scene.c`: novo array de peças específico pra malha (paralelo ao já
  existente pro SVG, não reaproveitado — ver nota acima), populado só
  quando `mesh_use_file_materials=1` **e** `mesh_import_load_pieces()`
  tiver sucesso. Ao desenhar, mesmo laço já usado pro SVG: troca a cor
  antes de cada peça via `material_set_piece_color()`. Fora desse caso
  (toggle desligado, `.stl`, ou OBJ sem material real), continua
  exatamente como a Fase 7a já funciona hoje — sem nenhuma mudança
  nesse caminho.
- Sem texturas (`map_Kd`), sem influência de `Ks`/`Ns` (specular do
  arquivo) — só a cor difusa `Kd` vira a cor da peça, aplicada através
  do material único configurado na aba Material (Clássico/Metálico/
  Vidro/Fosco), do mesmo jeito que a cor do SVG já funciona.

## 3. Campos de configuração

```c
/* mesh_path e mesh_size_scale ja existem (Fase 7a) */
int mesh_use_file_materials;   /* 0/1, default 0 */
```

## 4. Interface (aba "Conteúdo")

- Modo **Malha 3D**: ganha um checkbox "Usar materiais do arquivo",
  logo abaixo do slider de escala. Sem efeito visível imediato quando
  o arquivo é `.stl` (documentado, não precisa de aviso na UI).

## 5. Testes

- `geometry/mesh_import` — fixtures `.obj` com `.mtl` inline (2
  materiais reais, cores diferentes) → confirma 2 peças com as cores
  certas; fixture sem nenhum `mtllib`/`usemtl` → confirma que
  `mesh_import_load_pieces()` retorna falha (só material fallback).
- `M3DT_SHOT` visual: um `.obj` com 2+ materiais, toggle ligado (peças
  com cores distintas) e desligado (tudo na cor do material único).

## 6. Fora de escopo desta fase

- Texturas de imagem (`map_Kd` e afins) — só cor sólida.
- `Ks`/`Ns` (specular do arquivo) — ignorados, o material único do
  screensaver já define o comportamento especular.
- `.glb`/`.gltf` — fica pra Fase 7c, parser novo do zero.
- WBOIT pro vidro — mantém o blend simples já existente.
