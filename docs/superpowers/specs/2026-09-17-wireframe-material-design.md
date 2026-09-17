# Material Wireframe (4º material) — Documento de Design

## 1. Objetivo

Um 4º modo de material (`material_mode == 3`, reaproveitando o índice
que era do Fosco, removido nesta mesma sessão) que desenha **só as
arestas reais da geometria** como faixas 3D de espessura constante em
pixels de tela, sem preencher nenhuma face. Funciona igual pra
qualquer modo de conteúdo (Texto/Relógio/SVG/Malha 3D importada), já
que a técnica opera sobre a malha triangulada final (`MeshData`), não
sobre como ela foi gerada.

Inspiração do usuário: visualização de wireframe do Blender (incluindo
o modo Raio-X, com arestas ocultas mais apagadas) e desenhos técnicos
estilo blueprint. Um preset "Blueprint" usando esse material é uma
ideia de aplicação futura - **fora de escopo aqui**, fica pro item 7
do roteiro (retomar presets).

## 2. Visão geral da arquitetura

Três peças novas, nenhuma reaproveitando o pipeline do Vidro (WBOIT) -
Wireframe não tem transparência de face nem múltiplas camadas
sobrepostas, é bem mais simples:

1. **Detecção de arestas** (`src/geometry/feature_edges.c`/`.h`, novo
   módulo puro): dado o `MeshData` final de cada malha/peça
   (posições + índices de triângulo), produz a lista de arestas que
   realmente marcam um contorno ou vinco - não toda aresta de todo
   triângulo. Roda uma vez por reconstrução de malha, junto com o
   `gl_mesh_upload` já existente.
2. **Passe de profundidade "invisível"** (novo, em `scene.c`): desenha
   a malha sólida (a mesma que Clássico/Metálico usam) numa FBO nova
   só de profundidade, com `glColorMask` desligado - não pinta nada,
   só registra "o que está na frente de quê".
3. **Passe de linhas** (`shaders/wireframe.vert`/`.geom`/`.frag`,
   novo): desenha a lista de arestas como faixas 3D de espessura
   constante em pixels via um estágio de geometry shader (primeira
   vez usado neste projeto - já suportado pelo piso OpenGL 3.3 core).
   Consulta o passe de profundidade pra decidir oclusão/opacidade
   (seção 6).

## 3. Detecção de arestas (`feature_edges.c`)

```c
typedef struct { v3 a, b; } WireEdge;

/* md: malha final (posicoes + indices de triangulo), qualquer origem
   (texto/clock/svg extrudado, ou malha importada). crease_deg: acima
   desse angulo entre as normais dos 2 triangulos vizinhos, a aresta
   entra na lista. out/out_count: array alocado internamente (malloc),
   quem chama libera com free(). Retorna 0 se md nao tiver triangulos. */
int feature_edges_build(const MeshData *md, float crease_deg,
                         WireEdge **out, int *out_count);
```

Algoritmo: para cada triângulo (3 índices consecutivos em `md->idx`),
computa a normal geométrica plana (produto vetorial das arestas, NÃO
a normal suavizada `nx,ny,nz` de `MeshVertex` - essa é pra
sombreamento, não serve pra medir curvatura real entre triângulos
vizinhos). Para cada aresta (par de vértices, normalizado como
`min(i,j),max(i,j)` pra achar vizinhos), acumula as normais planas de
até 2 triângulos vizinhos numa tabela hash simples (chave = par de
índices). No final, por aresta:

- **1 triângulo vizinho** (borda aberta da malha) → sempre entra
  (silhueta real).
- **2 triângulos vizinhos** → entra se o ângulo entre as 2 normais
  passar de `crease_deg`.
- **3+ triângulos vizinhos** (malha não-manifold, incomum mas possível
  em `.obj` importado malformado) → sempre entra (caso degenerado,
  mais seguro mostrar que esconder).

`crease_deg = 35.0f` fixo por enquanto (constante nomeada, comentário
explicando o que subir/descer faz - mesmo padrão de outras constantes
estilizadas do projeto, ex. `BLUR_RAND_SPREAD`). Não é exposto como
slider - o pedido do usuário foi um toggle simples de estilo (limpo
vs. denso), não uma curva ajustável.

**Onde roda**: logo depois de cada `MeshData` ser montada em
`rebuild_mesh`/`rebuild_svg_mesh`/`rebuild_imported_mesh`
(`scene.c`), antes ou depois do `gl_mesh_upload` já existente (ordem
não importa, são independentes). Malhas com múltiplas peças (SVG,
`.obj` com vários objetos) chamam `feature_edges_build` uma vez por
peça e concatenam os resultados numa lista só. Resultado final vira
UM VBO novo (`SceneRenderer.wire_vbo`/`wire_vao`/`wire_edge_count`,
topologia `GL_LINES`, 2 vértices de posição por aresta) - reconstruído
junto com o resto da malha sempre que `mesh_dirty`.

## 4. `Config` - campos novos

```c
float wireframe_thickness;  /* 1..6 (px de tela), default 2 */
int   wireframe_xray;       /* 0/1, default 0 */
```

Roundtrip completo: `config_defaults`, `config_load_from`
(`reg_get_f`/`reg_get_i`), sanitize (`clampf`/booleano), `config_save_to`,
`preset_scope_copy`, `preset_dump_fields` - os mesmos 6 pontos de
contato já estabelecidos pra todo campo novo de material nesta sessão
(ex. `edge_bias`).

## 5. Passe de profundidade (`scene.c`)

O alvo HDR principal já tem seu próprio buffer de profundidade
(criado por `post_begin`, às vezes com MSAA dependendo da qualidade
escolhida) - pra **oclusão normal** isso já basta, sem FBO nova
nenhuma: desenha a malha sólida ali mesmo com `glColorMask` desligado
(passo 1 abaixo), depois as linhas por cima com teste de profundidade
ligado.

O **Raio-X** precisa de algo a mais: o shader das linhas precisa
*amostrar* essa profundidade (não só testar contra ela), e não dá pra
amostrar o anexo de profundidade de uma FBO multisample direto num
`sampler2D` comum - por isso ganha uma FBO própria, sempre de UMA
amostra só, texture-backed (os helpers existentes de `gl_core.h` só
criam profundidade como **renderbuffer**, não-amostrável - por isso
não dá pra reaproveitar `gl_fbo_color16f`; montada diretamente, mesmo
nível de código já usado pra FBO do WBOIT):

```c
unsigned wire_depth_fbo, wire_depth_tex;
int      wire_depth_w, wire_depth_h;
```

`glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0,
GL_DEPTH_COMPONENT, GL_FLOAT, NULL)` + `glFramebufferTexture2D(...,
GL_DEPTH_ATTACHMENT, ...)`, sem anexo de cor (`glDrawBuffer(GL_NONE);
glReadBuffer(GL_NONE);`). Criada/redimensionada preguiçosamente
(`ensure_wire_depth_target`, mesmo padrão de `ensure_wboit_targets`) -
só quando `wireframe_xray` está ligado, já que a oclusão normal nem
precisa dela.

Sequência quando `material_mode == 3`, dentro de `scene_render()`:

1. **Sempre** (os 2 modos): `glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE)`,
   desenha a malha sólida atual (o mesmo `draw_content()` que
   Clássico/Metálico já usam) com o programa de material já existente
   (a cor não importa, está mascarada; reaproveitar evita escrever um
   shader novo só pra isso) **no alvo HDR principal já ligado** -
   registra a profundidade real no depth buffer que já está lá.
   `glColorMask` de volta pro normal.
2. **Só se `wireframe_xray`**: repete o mesmo desenho (`draw_content()`,
   `glColorMask` desligado) uma segunda vez, agora dentro de
   `wire_depth_fbo` (a FBO nova, sempre de 1 amostra) - é essa cópia
   que o shader das linhas vai poder amostrar como textura no passo 4.
3. Continua desenhando no alvo HDR principal (já é o que está ligado -
   nenhum `glBindFramebuffer` extra necessário aqui).
4. Desenha o passe de linhas (seção 6): com `wireframe_xray` desligado,
   teste de profundidade ligado contra o buffer já preenchido no passo
   1 (oclusão de verdade, decidida pela placa de vídeo); ligado, teste
   de profundidade desligado + `wire_depth_tex` do passo 2 amostrado
   no shader pra decidir opacidade (seção 7).

## 6. Passe de linhas (`wireframe.vert`/`.geom`/`.frag`)

**Vertex**: só transforma a posição pra clip space (`gl_Position =
uProj * uView * uModel * vec4(aPos,1.0)`), passa adiante.

**Geometry** (`layout(lines) in; layout(triangle_strip, max_vertices
= 4) out;`): recebe os 2 vértices da aresta (`GL_LINES`), calcula a
direção em espaço de tela (usando `gl_Position.xy/gl_Position.w` dos
2 pontos), acha o vetor perpendicular, escala por
`uThicknessPx / uViewportSize` (convertendo pixels pra unidades de
clip space) e emite 2 triângulos (uma faixa) deslocando cada ponto
`+-metade` da espessura nessa perpendicular. Espessura constante em
pixels de tela, independente da distância da câmera (câmera mais
longe = clip space menor por pixel, mas o cálculo já compensa isso
puxando do `w` de cada vértice).

**Fragment**: cor sólida (`uLineColor` = `Cor:` atual) + emissivo
(reaproveita `uEmissiveColor`/`uEmissiveAmount`, mesma lógica de
Clássico/Vidro: `col = uLineColor + uEmissiveColor * uEmissiveAmount`).
Sem cálculo de luz nenhum (nem Fresnel, nem specular) - é exatamente o
"sem shading" pedido.

## 7. Oclusão vs. Raio-X

Mesmo passe de profundidade (seção 5) alimenta os 2 modos, só muda
como o passe de linhas o consome:

- **Oclusão normal** (`wireframe_xray == 0`, padrão): a profundidade da
  malha sólida já está no buffer do alvo HDR principal (passo 1 da
  seção 5 - nenhuma FBO nova envolvida). Desenha as linhas com
  `GL_DEPTH_TEST` ligado (`GL_LEQUAL`) e `glDepthMask(GL_FALSE)` (não
  precisa escrever, só testar) - qualquer trecho de aresta atrás da
  malha sólida simplesmente não passa no teste da placa de vídeo, sem
  custo de shader extra.
- **Raio-X** (`wireframe_xray == 1`): SEM blit, `wire_depth_tex` fica
  ligado como sampler (unidade de textura dedicada). Desenha as linhas
  com `GL_DEPTH_TEST` desligado e `GL_BLEND` ligado
  (`GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA`) - todo fragmento é desenhado,
  mas o fragment shader compara sua própria profundidade
  (`gl_FragCoord.z`) contra `texture(uSolidDepth, gl_FragCoord.xy /
  uViewportSize).r`: se estiver na frente/na superfície → alpha 1.0;
  se estiver atrás (oculta) → **alpha 0.6** (o efeito "fantasma" do
  Blender pedido pelo usuário).

## 8. Aba Material - controles novos

Combo "Material" ganha "Wireframe" (`STR_MATERIAL_MODE_WIREFRAME`,
`material.mode.wireframe`). Bloco novo, posição própria no `.rc`
(nunca compartilhando coordenadas com outro controle - lição já
estabelecida nesta sessão): slider "Espessura da linha:" (1-6,
default 2) + checkbox "Raio-X". Aparecem só com `mode == 3`.

`material_layout_apply()` (`config_dialog.c`) ganha:
```c
int vis_wireframe = (mode == 3);
```
e `vis_emissive` passa de `(mode == 0 || mode == 2)` pra
`(mode == 0 || mode == 2 || mode == 3)` - Emissivo passa a aparecer
também no Wireframe, conforme pedido. Metalização/Aspereza/Ambiente
continuam com suas regras atuais (nenhuma delas inclui `mode == 3` -
não fazem sentido sem shading).

## 9. Fora de escopo

- Preset "Blueprint" embutido (fica pro item 7 do roteiro).
- Slider de ângulo de vinco (`crease_deg` fica fixo em 35°).
- Qualquer gate de `RenderQuality`/autoQuality - a malha de arestas de
  um texto é pequena (poucos milhares de segmentos no pior caso), sem
  necessidade de degradar sob carga, mesmo padrão dos outros 3
  materiais (materiais não entram na escada de auto-qualidade, só
  efeitos como bloom/streaks/partículas entram).
- Visualização de debug (`uDebugView`) não ganha nada específico pro
  Wireframe - não há cálculo de material complexo pra inspecionar
  (sem Fresnel, sem reflexo). Debug view continua funcionando pros
  outros 3 modos como já funciona.

## 10. Testes

`feature_edges_build` é função pura, dá pra testar de verdade (TDD)
com uma malha simples e conhecida: um cubo construído à mão (8
vértices, 12 triângulos - 2 por face). Resultado esperado: exatamente
**12 arestas** (as arestas reais do cubo, ~90° entre faces vizinhas) -
as 6 arestas "diagonais" que dividem cada face quadrada em 2
triângulos são coplanares (0° de ângulo) e NÃO devem aparecer. Testa
também o caso de borda aberta (1 triângulo solto, sem vizinho) -
todas as 3 arestas devem entrar.

Sem teste automatizado possível pra corretude visual do passe de
linhas em si (mesma limitação de todo o resto deste shader - ver
spec do WBOIT). Build limpo + `M3DT_SELFTEST=1` confirma que os
programas novos (linhas + prepass) compilam e linkam sem erro.
Verificação real: capturar via `M3DT_SHOT` com `material_mode=3` em
letras côncavas/com buraco ("3", "D") nos 2 modos de oclusão, e nas
espessuras mínima/máxima do slider.
