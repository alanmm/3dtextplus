# Fase 5b — Partículas — Documento de Design

## 1. Objetivo

Adicionar o último efeito visual do roadmap do spec principal (§6.4):
partículas, com 4 tipos — `dust`, `bokeh`, `sparks`, `stars`. Fecha a Fase 5
por completo (5a fundo + 5b partículas).

## 2. Dois grupos de comportamento

A conversa de design deixou clara uma distinção que o spec original não
explicitava: **`dust`/`bokeh`/`stars` são ambiente** (existem no espaço ao
redor da cena, independentes da rotação do texto 3D) e **`sparks` são
emitidas pelo objeto** (nascem na superfície do texto e se desprendem dele,
como fagulhas de solda).

- **Ambiente** (`dust`, `bokeh`, `stars`): confinadas numa caixa (AABB) em
  **espaço de mundo, fixa na origem** — não é multiplicada pela matriz
  `model` do texto (que gira/inclina). Como a câmera deste projeto nunca
  orbita (só o objeto gira via `model`; a câmera só faz pan), esse espaço de
  mundo fixo já corresponde exatamente a "ambiente que não acompanha o
  giro do texto".
- **Emitida** (`sparks`): cada partícula nasce em espaço local da malha
  (um vértice aleatório de `surfaceId == 2`, a parede extrudada do texto),
  é transformada para espaço de mundo pela matriz `model` **do instante em
  que nasce**, e a partir daí simula sua trajetória (gravidade) inteiramente
  em espaço de mundo — como uma fagulha real, que para de "pertencer" ao
  objeto assim que se desprende, independente do objeto continuar girando
  depois.

## 3. Arquitetura

- Módulo dedicado (diferente do Fundo, que é um shader sem estado): novo
  `src/particles.c` + `src/particles.h`, e `shaders/particle.vert` +
  `shaders/particle.frag`, exatamente como o spec principal já previa
  (§ "Estrutura de arquivos").
- Um `ParticleSystem` por `SceneRenderer` — nunca `static`/compartilhado
  entre janelas de monitores diferentes (mesma regra reforçada pelo bug de
  VAO corrigido na Fase 5a).
- Renderização: `GL_POINTS` reais (`glEnable(GL_PROGRAM_POINT_SIZE)`,
  `gl_PointSize` calculado no vertex shader por partícula), disco suave via
  `gl_PointCoord` no fragment shader, blend aditivo
  (`glBlendFunc(GL_SRC_ALPHA, GL_ONE)`), `glDepthMask(GL_FALSE)` mas
  `glEnable(GL_DEPTH_TEST)` mantido — partículas continuam sendo ocluídas
  pelo texto/fundo, só não escrevem no depth buffer entre si.
- Um VBO dinâmico, atualizado via `glBufferSubData` a cada frame a partir
  de um buffer de CPU reaproveitado (sem `malloc` por frame). Teto de 4000
  partículas simultâneas no total (varia por tipo, ver §5).
- **Simulação em CPU, decisão confirmada na conversa**: a 4000 partículas o
  custo de atualizar posição/velocidade/idade é da ordem de dezenas de
  microssegundos por frame — irrelevante perto do que a GPU já gasta
  renderizando o texto 3D + bloom + streaks. Rodar a simulação 100% na GPU
  exigiria transform feedback com buffers ping-pong (o projeto usa OpenGL
  3.3 core, sem compute shader — isso só existe a partir do 4.3): complexidade
  real para resolver um gargalo que não existe nesse volume. A preocupação de
  "não pesar no computador" já é coberta pelo que o projeto tem: `fps_cap`,
  `vsync`, e a escada de auto-qualidade (§7).
- Frame graph: partículas desenhadas em `scene_render()` **depois** do
  modelo 3D (vidro incluso), ainda dentro do alvo HDR — participam do
  bloom, como o spec original pedia.
- `scene_set_config` passa ao `ParticleSystem` os half-extents atuais
  (`hx, hy, hz`) sempre que a malha é reconstruída, para redimensionar a
  caixa-domínio do ambiente, e o buffer/contagem de vértices `surfaceId==2`
  do `MeshData` atual, para o spawn de `sparks`.

## 4. Config (novos campos, schema continua v2)

```c
int   particles_on;         /* 0/1 */
int   particles_kind;       /* 0 dust, 1 bokeh, 2 sparks, 3 stars */
float particles_density;    /* 0..1 */
float particles_speed;      /* 0..2 */
float particles_size_scale; /* 0..2 */
```

Sem campo de cor/domínio configurável — a caixa-domínio é derivada
automaticamente de `hx/hy/hz` (margem generosa ao redor do texto), e cada
tipo usa uma tinta fixa apropriada (branco/frio para `dust`/`stars`, morno
suave para `bokeh`, laranja-ember para `sparks`), como o spec original já
listava só `kind/density/speed/sizeScale` como campos.

## 5. Os 4 tipos

- **`dust`** (ambiente): caixa de mundo com meia-extensão
  `(hx*3+1, hy*3+1, hz*6+2)`; deriva lenta por curl noise (2D, reaproveitando
  o mesmo ruído de valor usado em `background.frag`); contagem
  `density * 800`; tamanho pequeno, `gl_PointSize` atenuado pela distância à
  câmera (mais perto = maior, leve paralaxe); ao sair da caixa por um lado,
  reentra pelo lado oposto (wrap).
- **`bokeh`** (ambiente): mesmo domínio/mecânica de `dust`; discos bem
  maiores (~4x); poucas partículas, `density * 60`; anel suave no fragment
  shader (mais claro na borda que no centro) em vez de disco cheio.
- **`stars`** (ambiente): mesmo domínio; posições geradas **uma vez** (na
  criação/mudança de config, não a cada frame — campo estático, não deve
  "nadar"); o campo inteiro gira bem devagar em torno do próprio eixo
  **independente do pêndulo do texto** (velocidade angular fixa e baixa —
  desvio deliberado do texto do spec principal, que dizia "acompanha o
  pêndulo"; a conversa de design confirmou que os 3 tipos de ambiente devem
  ser independentes do objeto); cintila por fase individual (seno com
  offset aleatório por partícula modulando o alfa); contagem
  `density * 300`.
- **`sparks`** (emitida, "fagulha de solda"): nasce num vértice aleatório de
  `surfaceId==2` do `MeshData` atual; posição e normal transformadas para
  mundo pela `model` do instante do nascimento; velocidade inicial = normal
  do vértice (para fora) escalada por `speed`, com um pequeno viés vertical
  extra; gravidade constante puxa a trajetória para baixo (arco de fagulha);
  cor quente (laranja/amarelo, tipo brasa); vida curta (0.5–1.5 s aleatório),
  fade + encolhe conforme envelhece; `density` controla a **taxa de
  emissão** (partículas/segundo), não uma contagem fixa simultânea.
- **Ideia anotada para o futuro, fora de escopo agora**: uma variante de
  "condensação de água" (gotas que crescem e escorrem) foi cogitada durante
  a conversa como um possível 5º comportamento, mas é visualmente muito
  diferente de fagulha (queda lenta vs. explosão + apagamento) — decidido
  não misturar os dois no mesmo tipo. Revisitar como um tipo novo numa fase
  futura, se o usuário quiser.

## 6. Interface (aba "Partículas")

Nova aba, 9ª, inserida no final (índice 8, depois de "Fundo") — decisão
confirmada explicitamente: uma aba dedicada em vez de lotar mais controles
na já cheia aba "Efeitos". Controles: checkbox liga/desliga, combo Tipo
(Poeira/Bokeh/Faíscas/Estrelas), sliders Densidade/Velocidade/Tamanho —
todos desabilitados quando o checkbox está desligado, mesmo padrão de
`effects_enable()`/`post_enable()`/`bg_enable()` já usado nas outras abas.

## 7. Preview ao vivo e performance

- Preview ao vivo automático (o mini-preview já roda `scene_render()`
  inteiro) — sem trabalho extra.
- Partículas entram na escada de auto-qualidade como o **novo primeiro
  degrau** (mais caras entre os efeitos hoje toggleáveis = primeiras a
  cair): `0 topo → 1 partículas off → 2 streaks off → 3 bloom off → 4..8
  msaa/escala` (a escada do tier FULL passa de 8 para 9 passos;
  `RenderQuality` ganha um campo `particles`). REDUCED e `/p` continuam sem
  partículas, como já é a regra geral do spec para todo efeito não-essencial.

## 8. Testes

- `test_config.c`: novos campos, defaults, round-trip, clamps (padrão já
  usado nas Fases 4b-5a).
- `test_render_tiers.c`: novo degrau do ladder (9 passos no tier FULL),
  reordenação step→campo (`particles` cai no step 1, `streaks` sobe pro
  step 2, `bloom` pro step 3, etc.).
- Sem teste unitário de física da simulação (não é determinístico o
  suficiente pra valer TDD puro) — verificação visual via `M3DT_SHOT` dos 4
  tipos no preview do diálogo, mesmo padrão da Fase 5a.
- Sem necessidade de um novo teste dual-monitor dedicado (nenhum recurso GL
  novo é compartilhado entre contextos — `ParticleSystem` segue a mesma
  regra de posse-por-`SceneRenderer` já testada na Fase 5a), mas vale
  reconferir visualmente nos dois monitores como checagem rápida.

## 9. Fora de escopo desta fase

- Presets prontos (Cinema/Néon/Suave etc., spec §10) e import/export `.ini`
  (spec §9) — nenhum dos dois existe ainda nem para bloom/CA; ficam para
  quando o resto do spec chegar lá.
- Interação do vidro com WBOIT real — TODO já rastreado à parte, sem
  interação nova com partículas.
- Uma 5ª variante "condensação de água" — anotada em §5 para o futuro.
- Simulação de partículas 100% na GPU (transform feedback) — decisão
  explícita de manter CPU nesta fase (§3); revisitar só se um perfilamento
  real mostrar necessidade.
