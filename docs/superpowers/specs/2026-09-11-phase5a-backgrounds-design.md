# Fase 5a — Fundos configuráveis — Documento de Design

## 1. Objetivo

Substituir a cor de fundo fixa (`0.02, 0.03, 0.05`, hardcoded em `post_begin`)
por um fundo configurável com 4 tipos: sólido, gradiente, imagem e nebulosa
procedural. Fecha a primeira metade da Fase 5 do roadmap (§17 do spec
principal); partículas ficam para a Fase 5b, deliberadamente adiada.

## 2. Arquitetura

- Sem arquivo novo: a lógica de fundo entra em `scene.c`, como o spec
  principal já previa (§6.5) — é um triângulo fullscreen com seu próprio
  über-shader, do mesmo porte que o resto do que `scene.c` já orquestra
  (câmera, pêndulo, material).
- Novo par de shader: `shaders/background.frag` (reaproveita o
  `shaders/fullscreen.vert` já existente, usado pelos passes de
  `post.c`). Adicionado a `EMBED_INPUTS` no `build/Makefile` — mesmo
  mecanismo de embed de todos os outros shaders do projeto.
- `SceneRenderer` ganha: tipo de fundo, as cores/parâmetros de cada tipo,
  uma textura de imagem de fundo carregada sob demanda (dono da própria
  `SceneRenderer`, liberada ao trocar de imagem ou destruir a cena — nunca
  um `static`/global, para não repetir o bug de VAO compartilhado entre
  contextos corrigido nesta sessão) e um acumulador de tempo monotônico
  (`bg_time`) para a evolução da nebulosa e o pan da imagem.
- `scene_render()` desenha o fundo **primeiro**, logo após o
  `glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)` que já roda em
  `post_begin`: com `glDepthMask(GL_FALSE)` (sem escrever profundidade,
  buffer de profundidade permanece no valor limpo — 1.0, o mais distante)
  desenha o triângulo fullscreen do fundo; restaura `glDepthMask(GL_TRUE)`
  antes do desenho do modelo 3D normal, que passa no teste de profundidade
  em qualquer ponto e portanto sempre fica por cima, sem exceção.
- Isso acontece **dentro do mesmo alvo HDR** que bloom/streaks/tonemap já
  usam — um fundo claro (uma imagem ensolarada, um pico de brilho na
  nebulosa) participa do bloom como qualquer outra coisa na cena. É a
  opção mais simples e consistente com o pipeline atual; a alternativa
  (uma camada de fundo isolada, sem pós-processamento) sairia do escopo
  desta fase.

## 3. Campos de configuração

Seguindo a convenção já usada no `Config` real (campos "achatados", nomes
`snake_case`, não a notação `secao.campo` do spec principal, que era só
organização conceitual):

```c
int      background_type;      /* 0 solido, 1 gradiente, 2 imagem, 3 nebulosa */
float    bg_color1_r, bg_color1_g, bg_color1_b;
float    bg_color2_r, bg_color2_g, bg_color2_b;
float    bg_grad_angle;        /* graus, 0-360 */
wchar_t  bg_image_path[512];
int      bg_image_fit;         /* 0 cobrir, 1 conter, 2 repetir */
float    bg_pan_speed;         /* unidades de UV/seg, so' com type=imagem */
float    bg_neb_color1_r, bg_neb_color1_g, bg_neb_color1_b;
float    bg_neb_color2_r, bg_neb_color2_g, bg_neb_color2_b;
```

Schema continua na v2 — mesmo padrão de migração por "campo ausente usa o
padrão" já usado desde a Fase 4b (nenhum destes é obrigatório num registro
antigo). Persistidos como REG_SZ como todo o resto (`config_save`/
`config_load_from`).

## 4. Os 4 tipos de fundo

- **Sólido** — `fragColor = uColor1`. O caso trivial.
- **Gradiente** — interpolação linear ao longo de `uGradAngle` entre
  `uColor1`/`uColor2`, com um leve dither ordenado antes de escrever no
  alvo HDR (evita banding perceptível depois do tonemap, preocupação já
  levantada no spec principal §6.5).
- **Imagem** — `sampler2D uBgTex`, carregada com o mesmo padrão de loader
  já usado em `env.c` (stb_image, mas amostragem 2D direta, não
  equiretangular). UV calculada conforme `uBgFit`: *cobrir* (escala
  preenchendo e corta o excesso), *conter* (escala cabendo inteira,
  faixas na cor 1), *repetir* (UV em wrap). Desloca `UV.x` por
  `uBgTime * uPanSpeed`; quando `fit != repetir`, o deslocamento faz
  ping-pong nas bordas em vez de saltar.
- **Nebulosa** — fBm (4-5 oitavas) de ruído de valor com domain-warp (2
  passes de distorção), remapeado por uma rampa entre `uNebColor1`/
  `uNebColor2`, evoluindo bem devagar via `uBgTime * 0.01`. Sem arquivo
  nenhum — 100% procedural no shader.

## 5. Interface (aba "Fundo")

- Nova aba, **inserida no final** da ordem atual (índice 7, depois de
  "Pós") — evita renumerar as 7 abas existentes, cujos índices aparecem
  espalhados em `config_dialog.c`, nos scripts de teste automatizado desta
  sessão e em `M3DT_TAB`.
- `M3DT_TAB` passa a aceitar `0..7` (8 abas).
- Controles: combo Tipo (Sólido/Gradiente/Imagem/Nebulosa); botões Cor 1 /
  Cor 2 (mesmo padrão `IDC_COLOR` já usado na aba Conteúdo); slider Ângulo
  (só gradiente); botões Escolher imagem/Limpar + combo Ajuste
  (Cobrir/Conter/Repetir) + slider Velocidade de pan (só imagem); Cor 1 /
  Cor 2 próprias para a nebulosa (independentes das cores do gradiente,
  para não perder uma configuração ao trocar de tipo no combo).
- Habilita/desabilita os controles de cada tipo conforme a seleção atual,
  no mesmo padrão de `geometry_enable()`/`post_enable()` já usado nas
  outras abas.

## 6. Preview ao vivo

Nenhum trabalho extra: o mini-preview do diálogo já roda o `scene_render()`
inteiro, então o fundo aparece ao vivo automaticamente, incluindo por cima
do zoom por aba, giro automático e navegação manual por mouse implementados
nesta sessão.

## 7. Testes

- Não é prático testar um shader visual com asserts unitários; a
  verificação é pelo padrão já estabelecido nesta sessão:
  `M3DT_SHOT`/`M3DT_SHOT_T` para captura headless confiável dos 4 tipos.
- Dado o bug recente de estado compartilhado entre contextos GL (VAO do
  segundo monitor), incluir explicitamente uma captura real (não-selftest)
  de `/s` em 2 monitores com uma imagem de fundo carregada, confirmando que
  a textura/estado de fundo não vaza nem falta entre contextos.
- QA manual: alternar os 4 tipos no preview ao vivo; conferir que o
  gradiente não banda visivelmente; conferir cobrir/conter/repetir com uma
  imagem de teste não-quadrada; conferir que a nebulosa evolui devagar e
  não tem custura (não é UV-tileável, é um campo fBm contínuo, então não
  deveria ter costura por natureza).

## 8. Fora do escopo desta fase (adiado deliberadamente)

- Partículas (Fase 5b, ordem escolhida pelo usuário).
- Qualquer interação do fundo com a refração WBOIT do material Vidro (o
  spec principal menciona o vidro amostrando o "buffer de fundo" para
  refração — isso já é um TODO existente do material Vidro, rastreado à
  parte, não faz parte da 5a).
- i18n, presets, instalador (fases futuras).
