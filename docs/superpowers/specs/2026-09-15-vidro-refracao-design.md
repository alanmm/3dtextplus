# Vidro — Refração Real (Distorção) — Documento de Design

## 1. Objetivo

O material Vidro hoje não mostra o que está atrás do texto — a cor
"vista através" é sempre `base_color * 0.6`, uma cor fixa, misturada
com reflexo de ambiente via fresnel. Usuário testou e sentiu falta de
"algum elemento" pra parecer vidro de verdade; decidido em conversa
que o elemento que falta é **distorção real do fundo através do
vidro** (efeito lupa/lente), não só um ajuste de reflexividade.

Esta fase implementa refração real em screen-space: captura o que já
foi desenhado atrás do texto (o fundo) numa textura, e o fragment
shader do Vidro amostra essa textura com um deslocamento de UV
baseado na normal da superfície — o fundo "dobra" ao atravessar o
vidro, como uma lente.

## 2. Novo campo `Config`

```c
float refraction;   /* 0..1 - so' Vidro. 0 = desliga por completo
                        (comportamento identico a antes desta fase,
                        sem custo de captura); >0 = fundo real
                        visivel, distorcido proporcionalmente */
```

Roundtrip completo (mesmo padrão dos outros campos de material):
`config_defaults` (default **0.4** — não-zero de propósito, pra que o
Vidro já saia visivelmente melhor sem precisar abrir a aba Material),
`config_load_from`/`reg_get_f`, sanitize (`clampf 0..1`),
`config_save_to`/`set_f`.

**Decisão confirmada com o usuário**: `refraction == 0` desliga a
funcionalidade por completo (mesma semântica de "amount 0 = inerte"
já usada em `emissive_amount`) — nesse caso o shader nem tenta
amostrar `uGrabTex`, e `scene_render()` pula a captura do fundo
inteira (custo zero). Não existe um estado "olha através sem
distorcer" — o valor mínimo útil pra ver o efeito é um número pequeno
acima de 0 (ex.: 0.1).

## 3. Captura do fundo (`scene.c`)

`SceneRenderer` ganha um novo `GlFbo grab` (textura HDR RGBA16F, igual
padrão de `gl_fbo_color16f` já usado em `post.c`) **como campo da
struct, nunca `static`** — mesma lição do bug de VAO compartilhado
entre monitores (Fase 5a): cada janela/monitor tem seu próprio
contexto GL, texturas não podem ser compartilhadas entre eles sem
`wglShareLists` (não usado neste projeto).

Criada/redimensionada preguiçosamente (mesmo padrão de
`ensure_size`/`ensure_output_size` já existente em `post.c`) para o
tamanho `fb_w x fb_h` passado a `scene_render`. Além da textura de
cor, precisa de mipmaps habilitados (`GL_LINEAR_MIPMAP_LINEAR`) para
o blur baseado em rugosidade (seção 5).

Em `scene_render()`, logo depois do fundo ser desenhado
(`glDepthMask(GL_TRUE)` na linha que já existe hoje) e **antes** de
`material_begin`, se `s->material_mode == 2 && s->refraction > 0.0f`:

```c
GLint prev_fbo;
glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_fbo);
ensure_grab_size(s, fb_w, fb_h);           /* helper novo, preguicoso */
glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev_fbo);
glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s->grab.fbo);
glBlitFramebuffer(0, 0, fb_w, fb_h, 0, 0, fb_w, fb_h,
                   GL_COLOR_BUFFER_BIT, GL_LINEAR);
glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);   /* volta a desenhar aqui */
glBindTexture(GL_TEXTURE_2D, s->grab.color);
glGenerateMipmap(GL_TEXTURE_2D);
```

Importante: `scene.c` não precisa saber nada sobre a FBO HDR/MSAA que
`post.c` já tem montada — `GL_DRAW_FRAMEBUFFER_BINDING` pega
"qualquer que seja o destino atual" (funciona igual com ou sem MSAA,
já que `glBlitFramebuffer` resolve amostras múltiplas automaticamente
num blit pra um destino non-MS, mesmo mecanismo que `gl_blit_resolve`
já usa em `post.c`). Nenhuma dependência nova entre os dois arquivos.

Sem essa condição (`refraction <= 0` ou modo != Vidro), nada disso
roda - custo zero nos outros 3 materiais e no Vidro com o slider
zerado.

## 4. `material.h`/`.c`

Novo uniform `uGrabTex` (sampler2D, unidade de textura **1** — unidade
0 já é `uEnvTex`), fixado uma vez em `material_init` igual o padrão
existente (`glUniform1i(m->uGrabTex, 1)`). Novo uniform `uRefraction`
(float). `material_set_style` ganha mais um parâmetro `float
refraction`; `scene_render` passa `s->refraction` e faz o bind da
textura de grab em `GL_TEXTURE1` só quando for usá-la.

Também precisa de um uniform de tamanho de tela, `uScreenSize` (vec2)
— hoje `model.frag` não tem noção de resolução; necessário pra
transformar `gl_FragCoord` em UV 0..1. Fixado toda vez que muda
(`material_begin` recebe `fb_w`/`fb_h` ou um novo pequeno setter).

## 5. Shader (`model.frag`, ramo `uMode == 2`)

```glsl
vec3 refr = base * 0.6;
if (uRefraction > 0.0) {
    vec2 screenUV = gl_FragCoord.xy / uScreenSize;
    vec2 duv = clamp(screenUV + N.xy * uRefraction * 0.12, 0.002, 0.998);
    vec3 behind = textureLod(uGrabTex, duv, uRoughness * 5.0).rgb;
    refr = mix(refr, behind * mix(vec3(1.0), base, 0.4), 0.85);
}
```

- **Deslocamento de UV**: usa `N.xy` (a normal da superfície já
  calculada no topo de `main()`) como aproximação estilizada de
  "quanto essa face desvia o que está atrás" — não é refração
  fisicamente correta (sem índice de refração real, sem profundidade),
  mesma filosofia do resto do arquivo (`jitter_reflection` etc. já são
  estilizados, documentados como tal). Faces quase de frente pra
  câmera (N quase alinhado com a câmera, `N.xy` pequeno) bendem pouco;
  bordas/chanfros com normal mais inclinada bendem mais — dá a
  sensação de lente nas bordas, janela reta no meio, que é
  exatamente o "efeito lupa" pedido.
- **Rugosidade controla o blur**: `textureLod(..., uRoughness * 5.0)`
  — vidro liso (aspereza baixa) mantém a imagem atrás nítida; vidro
  áspero borra (jateado/fosco), reaproveitando os mipmaps gerados no
  passo 3. Combinado com o ajuste de reflexividade que a aspereza já
  ganhou nesta mesma sessão (`m = fres + 0.15 + (1-aspereza)*0.35`),
  a aspereza do Vidro passa a controlar tanto "quanto reflete" quanto
  "quão nítido é o que se vê através" — os dois efeitos que realmente
  definem liso vs. fosco num vidro de verdade.
- **Constantes exatas** (`0.12` do deslocamento, `5.0` do multiplicador
  de LOD, `0.4`/`0.85` da mistura de cor) são valores de partida,
  ajustados visualmente durante a implementação — mesmo processo já
  usado pra toda constante estilizada deste shader (ex.: os múltiplos
  ajustes de `jitter_reflection` nesta mesma sessão).
- Resto do ramo Vidro (reflexo especular direto, emissivo, alpha via
  `mix(0.35, 0.95, m)`) continua sem mudança.

## 6. UI (aba Material)

Novo bloco no sistema de reflow já existente (`g_mat_blocks`),
**visível só em Vidro** (`mode == 2`) — os outros 3 materiais nunca
veem esse controle. IDs novos em `resource.h`:
`IDC_REFRACT_LABEL` (1319), `IDC_REFRACT_VAL` (1320), `IDC_REFRACT`
(1321) — reaproveita a faixa que ficou livre com a remoção de
Verniz/Anisotropia nesta mesma sessão. Label "Distorção:" (PT) /
"Distortion:" (EN) — chaves i18n novas
`material.refraction_label`.

Posição no `.rc`: como só existe em Vidro (que já esconde
Metalness), o bloco entra na mesma "fatia" vertical que hoje é
ocupada por Metalização quando o modo não é Metálico — o reflow
dinâmico cuida de posicionar certo, só precisa existir no `.rc` como
um bloco a mais (trackbar + label + valor), fora da faixa hoje ocupada
por Emissivo/Ambiente.

## 7. Presets

`preset_scope_copy()` e `preset_dump_fields()` (backup/restauração em
lote) ganham `refraction`, mesmo padrão dos outros 6 campos de
material já tratados nesta sessão.

## 8. Multi-monitor

`s->grab` é campo de instância, criado lazily por janela/contexto —
cada monitor com Vidro selecionado tem sua própria textura de captura,
sem risco de reuso de handle entre contextos (mesma classe de bug já
corrigida na Fase 5a pro VAO de fullscreen-quad).

## 9. Fora de escopo

- Refração fisicamente correta (índice de refração, profundidade,
  múltiplas superfícies/espessura real do vidro) — fica estilizada de
  propósito, como o resto do shader.
- Aplicar a mesma técnica de grab-pass a outros materiais — só Vidro
  usa `uGrabTex`.
- Ajustar o layout geral do dialog (`IDD_CONFIG`/`IDC_TABS`) — o
  espaço sobrando da rodada anterior (remoção de Verniz/Anisotropia)
  já comporta o bloco novo sem precisar crescer de novo.
- Interação de partículas com a refração — partículas continuam
  desenhadas depois do texto (por cima), não fazem parte da captura
  do fundo, comportamento inalterado.

## 10. Testes

- `test_config.c`: default (`0.4`), roundtrip (save/load com valor
  diferente), clamp (`0..1`, fora da faixa).
- Verificação visual (build + `.exe` real, já que é puramente uma
  questão de aparência renderizada): Vidro com `refraction` em 0
  (idêntico a hoje), valor baixo (~0.1, distorção sutil), valor alto
  (~0.9, efeito lupa bem visível); aspereza baixa vs. alta com
  `refraction` fixo (nítido vs. borrado); trocar de monitor/redimensionar
  a janela de preview do dialog sem crash (a textura de grab precisa
  redimensionar junto).
