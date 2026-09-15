# Vidro — Transparência Independente de Ordem (WBOIT) — Documento de Design

## 1. Objetivo

O material Vidro mistura suas faces (tampa da frente, tampa de trás,
paredes do chanfro/extrusão - tudo desenha dos dois lados, ver
`glDisable(GL_CULL_FACE)` em `scene.c`) na ordem em que a malha está
armazenada, não na ordem real de profundidade. Já foram tentadas 2
correções por fragmento (orientação do triângulo, depois normal
autoral) e as duas falharam em geometria côncava/com buraco (ex.: o
vão do "D") - pedaços de casca legítima sumindo, pedaços de parede
interna sobrando. Essa fase substitui a mistura simples por
**Weighted Blended Order-Independent Transparency (WBOIT)**
(McGuire & Bavoil, 2013): em vez de decidir "o que esconder", cada
face contribui com um peso pra composição final - a ordem de desenho
deixa de importar.

**O cálculo de cor do Vidro em si (reflexo, aspereza, emissivo) não
muda nada** - só a forma como o resultado final é composto com o que
está atrás.

## 2. Por que não a forma "padrão" de WBOIT

A implementação mais comum usa 2 alvos de renderização com **2
funções de mistura diferentes simultâneas** (`GL_ONE,GL_ONE` aditivo
num alvo, `GL_ZERO,GL_ONE_MINUS_SRC_ALPHA` multiplicativo no outro) -
isso exige mistura por-anexo (`glBlendFunci`/`GL_ARB_draw_buffers_blend`),
um recurso do OpenGL 4.0 não garantido no piso 3.3 core que este
projeto assume.

**Solução**: reformular o termo multiplicativo (`revealage = produto
de (1-alpha) de cada face`) usando logaritmo: `log(produto) = soma
dos logs`. Assim os DOIS alvos podem usar a MESMA mistura aditiva
simples (`glBlendFunc(GL_ONE, GL_ONE)`, disponível em qualquer GL
3.3), e o passe de resolução desfaz o log com `exp()` no final. Custo
extra: uma chamada de `log`/`exp` por fragmento - irrelevante.

## 3. Novos alvos de renderização (`SceneRenderer`, `scene.c`)

Dois anexos de cor numa ÚNICA FBO nova (MRT via `glDrawBuffers`),
criada preguiçosamente (mesmo padrão da textura de captura da fase
anterior, removida) só quando o Vidro está selecionado, no tamanho
`fb_w x fb_h`:

```c
unsigned wboit_fbo;             /* uma FBO, 2 anexos de cor */
unsigned wboit_accum_tex;       /* RGBA16F - cor*alpha*peso acumulada, e alpha*peso no canal A */
unsigned wboit_reveal_tex;      /* R16F - soma de log(1 - alpha) de cada face */
int      wboit_w, wboit_h;
```

Não usa o helper `GlFbo` existente em `gl_core.h` (esse struct só
guarda 1 textura de cor) - montada diretamente com `glGenFramebuffers`
+ 2 `glGenTextures` + `glDrawBuffers(2, {COLOR_ATTACHMENT0,
COLOR_ATTACHMENT1})`, mesmo nível de código que os helpers de
`gl_core.c` já usam por baixo.

## 4. Sequência em `scene_render()`, quando `material_mode == 2`

Substitui o desenho direto do Vidro (hoje: `material_begin` +
`material_set_style` + `gl_mesh_draw`, indo direto pro alvo HDR
principal) por 3 passos:

1. **Limpar e desenhar na FBO WBOIT**: `glBindFramebuffer` na FBO
   nova, `glClearColor(0,0,0,0)` no anexo 0 (acumulação) e
   `glClearColor(0,0,0,0)` no anexo 1 (log de revelação - zero, já
   que `exp(0)=1` = "nada ainda ocultando"). `glEnable(GL_BLEND);
   glBlendFunc(GL_ONE, GL_ONE);` (aditivo, vale pros 2 anexos ao
   mesmo tempo - ver seção 2). `glDepthMask(GL_FALSE)` (Vidro nunca
   escreve profundidade, como já é hoje) **e `glDisable(GL_DEPTH_TEST)`**
   durante esse passo - a FBO nova não tem anexo de profundidade
   (só os 2 de cor), e como o Vidro é o único material desenhado
   quando está ativo (`material_mode` vale pra cena inteira, não por
   objeto), não existe primeiro-plano opaco simultâneo pra testar
   contra. Reabilita `GL_DEPTH_TEST` depois deste passo (o resto do
   pipeline espera que esteja ligado). Desenha a malha **uma vez
   só** (nada de 2 passadas) - o shader (seção 5) escreve nos 2
   anexos em vez de `fragColor`.
2. **Resolver**: volta pro alvo HDR original (mesma técnica de
   `glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, ...)` usada na fase
   anterior, sem precisar que `scene.c` saiba nada da FBO que
   `post.c` montou). `glBlendFunc(GL_SRC_ALPHA,
   GL_ONE_MINUS_SRC_ALPHA)` (mistura normal, dessa vez). Desenha um
   triângulo de tela cheia (`gl_fullscreen_draw`, mesmo helper já
   usado em `post.c`) com um shader novo (seção 6) que lê os 2
   anexos e escreve a cor final composta.
3. Resto do frame (partículas etc.) continua sem mudança.

Quando `material_mode != 2`, nada disso roda - zero custo extra,
mesmo padrão da fase anterior.

## 5. Mudança em `shaders/model.frag`

Dois novos `out` (além do `fragColor` já existente, usado só por
Clássico/Metálico):

```glsl
layout(location = 1) out vec4  oAccum;
layout(location = 2) out float oRevealLog;
```

O ramo `uMode == 2` (Vidro) já calcula `col` (cor) e o alpha final
(hoje `mix(0.35, 0.95, m)`) - a única mudança é o que acontece com
esses dois valores no final do ramo: em vez de `fragColor =
vec4(col, alpha); return;`, calcula o peso e escreve nos 2 anexos
novos:

```glsl
float a = mix(0.35, 0.95, m);           // alpha, igual hoje
float linearDepth = length(uCamPos - vWorld);
float weight = a * clamp(0.4 / (1e-5 + pow(linearDepth / 8.0, 4.0)), 1e-2, 3000.0);
oAccum = vec4(col * a * weight, a * weight);
oRevealLog = log(max(1.0 - a, 1e-4));
```

(A constante `8.0` no divisor da profundidade escala a curva de peso
pra distância típica de câmera deste projeto - ajustada visualmente
durante a implementação, mesmo processo já usado em toda constante
estilizada deste shader.)

Clássico/Metálico continuam escrevendo só `fragColor` (localização
0) - as 2 saídas novas ficam sem efeito nenhum quando desenhados na
FBO principal (que só tem 1 anexo), exatamente como hoje.

## 6. Novo shader de resolução (`shaders/wboit_resolve.frag`)

Fragment shader pequeno, pareado com o `shaders/fullscreen.vert` já
existente (mesmo padrão dos outros passes de `post.c`):

```glsl
#version 330 core
in vec2 vUV;
uniform sampler2D uAccum;
uniform sampler2D uRevealLog;
out vec4 fragColor;
void main()
{
    vec4  accum    = texture(uAccum, vUV);
    float revealage = exp(texture(uRevealLog, vUV).r);
    float alpha = clamp(1.0 - revealage, 0.0, 1.0);
    vec3 avgColor = accum.rgb / max(accum.a, 1e-5);
    fragColor = vec4(avgColor, alpha);
}
```

Programa compilado uma vez em `scene_create()` (`SceneRenderer` ganha
`unsigned wboit_resolve_prog`), destruído em `scene_destroy()`, mesmo
padrão de `Material.prog`.

## 7. Fora de escopo

Não traz de volta distorção/refração real (fase abandonada
anteriormente - decisão separada). Não muda Clássico/Metálico.
`GL_CULL_FACE` continua desligado globalmente (necessário pra malhas
importadas com winding arbitrário) - WBOIT não depende de winding
pra nada, é imune a isso por construção. Multi-monitor: os 3 recursos
novos (`wboit_fbo`/texturas/programa) são campos de instância de
`SceneRenderer`, nunca `static` - mesma disciplina já estabelecida
neste projeto.

## 8. Testes

Sem teste automatizado possível pra corretude visual (mesma limitação
de todo o resto deste shader). Build limpo + `M3DT_SELFTEST=1` (já
confirma que o shader compila/linka, sem crash). Verificação real:
"3" e "D" (as mesmas letras côncavas/com buraco que expuseram o bug)
em Vidro, de vários ângulos de rotação - conferir que nenhuma parte
da casca externa desaparece e nenhuma parede interna vaza por cima
do que deveria estar na frente. Redimensionar a janela / trocar de
aba sem crash (a FBO nova precisa redimensionar corretamente).
