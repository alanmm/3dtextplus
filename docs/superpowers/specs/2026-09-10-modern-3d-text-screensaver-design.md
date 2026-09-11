# Modern 3D Text — Screensaver — Documento de Design

- **Data:** 2026-09-10
- **Repositório:** https://github.com/alanmm/modern3dtext
- **Nome de exibição:** Modern 3D Text
- **Arquivo final:** `Modern3DText.scr`
- **Chave de registro:** `HKCU\Software\Modern3DText`
- **Status:** aprovado no brainstorming; próximo passo é o plano de implementação (`writing-plans`).

---

## 1. Objetivo

Reescrever e ampliar o clássico screensaver "Texto 3D" do Windows (`sstext3d.scr`) como um `.scr` nativo, leve e distribuível, com:

- Nível de **extrusão** configurável (profundidade + bevel + casca oca).
- Um **terceiro modo de conteúdo** além de texto e relógio: **arquivo SVG** extrudado — e, por extensão, **malha 3D importada** (`.obj` / `.glb` / `.gltf` / `.stl`).
- **Rotação limitada** (pêndulo com ângulo máximo configurável), que nunca inverte o texto.
- **Fundo** configurável: cor sólida, gradiente, imagem ou gradiente procedural animado ("nebulosa").
- **Efeitos visuais**: bloom/glare, aberração cromática, difração/starburst, partículas — cada um como toggle independente com intensidade, mais 3–4 presets prontos.
- **Materiais** escolhíveis: especular clássico, metálico (matcap/env), vidro, fosco.
- Janela de configuração clássica Win32 com **mini-preview ao vivo**.
- Suporte multi-monitor (uma cena independente por monitor).
- Fallback automático para GPUs fracas / RDP / VM.
- Interface de configuração em **português e inglês**, detectada pelo idioma do Windows.

## 2. Contexto e não-objetivos

### Máquina de desenvolvimento (verificado em 2026-09-10)

| Ferramenta | Situação |
|---|---|
| Visual Studio / MSVC / Windows SDK | ausente |
| MinGW / gcc / clang / Rust | ausente |
| .NET SDK | ausente (só runtimes 8/9/10) |
| Node.js | v24.18 |
| Python | 3.14.2 |

O build **não** usará nenhum desses. Usará o **w64devkit** (MinGW-w64 portátil, extração via zip, sem instalador, sem admin).

### Não-objetivos do v1

- Não é um motor 3D genérico nem editor.
- Sem áudio.
- Sem animação de malhas importadas (skinning, morph). A malha apenas gira com o pêndulo.
- Sem HDR de saída (displays HDR recebem imagem SDR tonemapada).
- Sem sombras projetadas (usa rim light no lugar).
- SVG: sem suporte a `marker`, `dash`, `pattern`, `clipPath`, filtros SVG, texto dentro de SVG, `mask`. Traço só com junção redonda e largura uniforme.
- glTF: só primitivas `TRIANGLES`; sem animação, câmeras ou luzes do arquivo.
- Sem assinatura de código no v1 (documentado; ver §13).

## 3. Stack e ferramentas

- **Linguagem:** C11.
- **Toolchain:** w64devkit (versão fixada em `toolchain.txt`). `gcc`, `mingw32-make`, `windres`, `gdb`.
- **API gráfica:** OpenGL 3.3 core (Opção A do brainstorming — sem camada de abstração de GPU). Carregamento de funções via `glad` gerado.
- **Bibliotecas de terceiros** (todas single-header ou poucos arquivos, licenças permissivas, vendorizadas em `third_party/`):
  - `glad` — loader OpenGL 3.3 core.
  - `stb_truetype.h` — contornos de glifos (TTF e CFF/OTF).
  - `stb_image.h` — carregar imagens (fundo, texturas de glTF, env maps).
  - `stb_image_write.h` — usado só pelos testes golden-image e pelo `tools/make_icon`.
  - `nanosvg.h` — parse de SVG.
  - `libtess2` — tesselação de polígonos com furos e regras de winding (sucessor do tesselador GLU).
  - `fast_obj.h` — `.obj` + `.mtl`.
  - `cgltf.h` — `.glb` / `.gltf`.
- **Empacotamento:** Inno Setup (`ISCC.exe`, ~3 MB, documentado no README) + zip portátil.
- **Sem dependência de runtime** além de DLLs que já vêm no Windows: `opengl32`, `gdi32`, `user32`, `kernel32`, `comdlg32`, `comctl32`, `shell32`, `ole32`, `advapi32`, `dwmapi`.

## 4. Arquitetura — modos e host

### 4.1 Um executável, 4 modos

`WinMain` (`src/main.c`) faz parse da linha de comando (`src/cmdline.c`) e despacha:

| Invocação | Modo | Comportamento |
|---|---|---|
| `/s` | **Saver** | Enumera monitores → 1 janela borderless topmost fullscreen + contexto GL **por monitor** → loop de render em todas → encerra no 1º input real |
| `/p <HWND>` | **Preview** | 1 janela GL **filha** do `<HWND>` da mini-tela do diálogo do Windows → render **simplificado** (§8, "modo preview") → encerra quando o pai deixa de existir |
| `/c` ou `/c:<HWND>` | **Config** | Diálogo Win32 (§10) com mini-preview ao vivo → grava no registro no OK/Aplicar |
| (sem argumentos) | **Config** | idem `/c` |

Regras de parse:
- Case-insensitive (`/S`, `-s`, `/s` equivalentes).
- `/p` e `/c:` aceitam o HWND como argumento seguinte separado por espaço **ou** após `:` (ambas as convenções existem na natureza).
- HWND parseado como inteiro sem sinal (decimal). Inválido → trata como Config.
- Combinações contraditórias (`/s /p 1`) → o primeiro modo reconhecido vence.

### 4.2 Host de plataforma (`src/host_win32.c`)

Responsável por:
- `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)` no início.
- **Enumeração de monitores** via `EnumDisplayMonitors` → retângulo virtual de cada um.
- **Criação de janela**:
  - Saver: `WS_POPUP | WS_VISIBLE`, `WS_EX_TOPMOST`, posição/tamanho = retângulo do monitor. Uma window class registrada uma vez.
  - Preview: `WS_CHILD | WS_VISIBLE`, `SetParent(child, previewHwnd)`, tamanho = client rect do pai.
  - Config preview: `WS_CHILD` dentro da área de preview do diálogo.
- **Contexto WGL**: janela dummy oculta → pixel format básico → `wglCreateContext` temporário → carrega `wglChoosePixelFormatARB` / `wglCreateContextAttribsARB` → cria o contexto real 3.3 core (fallback 3.1 → 2.1 → GDI, ver §11) → destrói a dummy. Cada monitor tem seu **próprio contexto com recursos próprios** (VBOs, texturas, programas) — as cenas são independentes, então não há compartilhamento via `wglShareLists`. Shaders são compilados uma vez por contexto a partir das mesmas fontes embutidas.
- **Watcher de input** (saver): primeiro `WM_MOUSEMOVE` cujo deslocamento acumulado passe de um limiar (~4 px, para ignorar tremor/POR), qualquer `WM_KEYDOWN`/`WM_SYSKEYDOWN`, qualquer `WM_LBUTTONDOWN`/`WM_RBUTTONDOWN`/`WM_MBUTTONDOWN`, `WM_MOUSEWHEEL` → sinaliza saída **de todas** as janelas.
  - Ignora o `WM_MOUSEMOVE` inicial sintético (guarda a primeira posição e compara).
- **Loop**: `PeekMessage` não-bloqueante + render de todas as janelas por frame; `timeBeginPeriod(1)` durante a execução; respeita `perf.fpsCap` e `perf.vsync` (`wglSwapIntervalEXT`).
- **Cursor**: `ShowCursor(FALSE)` em saver; restaurado na saída.
- **Preview lifetime**: a cada frame checa `IsWindow(parentHwnd)`; em `WM_DESTROY` do pai ou falha, encerra limpo (destrói contexto GL, libera recursos).
- **Saída limpa**: destrói contextos, `wglMakeCurrent(NULL,NULL)`, `timeEndPeriod(1)`, restaura cursor.

## 5. Pipeline de geometria

### 5.1 Representação comum

```c
typedef struct { float x, y; } Vec2;
typedef struct {
    Vec2*   pts;        // polilinha fechada (último != primeiro; fechamento implícito)
    int     count;
    int     winding;    // +1 externo, -1 furo (ou detectado por área assinada)
} Contour;
typedef struct {
    Contour* contours;
    int      count;
    uint32_t fillColor; // 0xAARRGGBB; usado só quando svgColorMode = preserve
    int      fillRule;   // 0 nonzero, 1 evenodd
    float    flattenTol; // derivado de geometry.quality
} ContourSet;
```

Uma "peça" = um `ContourSet` que vira **uma** malha: um bloco de texto inteiro é uma peça; cada shape do SVG é uma peça.

### 5.2 Fonte → contornos (`src/geometry/font_outline.c`)

1. `CreateFontW(-emUnits, 0, 0, 0, bold?FW_BOLD:FW_NORMAL, italic, ...)` → `SelectObject` num DC de memória.
2. `GetFontData(hdc, 0, 0, buf, size)` devolve os bytes do arquivo da face selecionada. Para `.ttc`, também `GetFontData` com a tag `ttcf` e resolve o índice via `stbtt_GetFontOffsetForIndex`. Falha → carrega Segoe UI de `%WINDIR%\Fonts\segoeui.ttf`.
3. `stbtt_InitFont`. Por codepoint (UTF-8 decodificado): `stbtt_GetCodepointShape` → comandos move/line/curve (quadráticas em TTF, cúbicas em CFF) → achata na tolerância `flattenTol` → `Contour`s. `stbtt_GetCodepointHMetrics` + `stbtt_GetCodepointKernAdvance` para avanço/kerning.
4. **Multi-linha**: quebra em `\n`; cada linha posicionada; avanço vertical = `(ascent - descent + lineGap) * escala`; bloco centralizado horizontal e verticalmente.
5. Emite `ContourSet` em unidades de em (`1/unitsPerEm`), centrado na bbox da união.

### 5.3 SVG → contornos (`src/geometry/svg_shapes.c`)

1. `nsvgParseFromFile(path, "px", 96.0f)`. nanosvg já converte arcos e quadráticas em cúbicas.
2. Para cada `NSVGshape` visível: um `ContourSet`. Cada `NSVGpath` (subpath) → um `Contour`, cúbicas achatadas na tolerância. `shape->fill` → `fillColor` (quando `NSVG_PAINT_COLOR`; gradientes → cor média no v1). `shape->fillRule` → `fillRule`.
3. **Traço** (`shape->stroke` presente, sem fill): converte a polilinha em polígono de contorno via offset ±`strokeWidth/2` com **junção e cap redondos** (arcos aproximados). Largura uniforme. `dashArray`, `miter`, `marker` ignorados.
4. Sistema de coordenadas SVG é y-down → inverte Y. Normaliza a bbox da união de todos os shapes para o tamanho alvo, centrado.

### 5.4 Import de malha (`src/geometry/mesh_import.c`)

- `.obj` (+`.mtl`): `fast_obj_read`. Submeshes por material. `Kd`→baseColor, `Ks`/`Ns`→spec, `map_Kd`→textura difusa (via `stb_image`). Sem normais → calcula por face + suaviza por limiar de ângulo.
- `.glb` / `.gltf`: `cgltf_parse_file` + `cgltf_load_buffers`. Percorre `scene->nodes`, baка a transform mundial de cada nó, um submesh por `primitive`. Material = `pbr_metallic_roughness` (`base_color_factor`/`_texture`, `metallic_factor`, `roughness_factor`), `emissive_factor`. Imagens embutidas (`buffer_view`) ou por URI relativa via `stb_image`. Só `cgltf_primitive_type_triangles`.
- `.stl`: binário (detecção por tamanho/header) e ASCII. Normais do arquivo ou recalculadas. Material único (usa o material compartilhado).
- **Normalização** (todas): translada por `-centroide`, escala uniforme para `raio da esfera envolvente = geometry.sizeScale * RAIO_ALVO`. Guarda bounds originais para o auto-fit da câmera (§6.1).
- Toggle `content.meshUseFileMaterials`: usa os materiais do arquivo **ou** aplica o material compartilhado do screensaver (§7) na malha inteira.
- O import **não passa** por tesselação/extrusão — já é 3D.

### 5.5 `ContourSet` → malha 3D (`src/geometry/contour_mesh.c`) — núcleo

Saída: malha indexada com atributos `position` (vec3), `normal` (vec3), `surfaceId` (uint: 0 frente, 1 verso, 2 lateral, 3 bevel), e — quando `bevelMode = shading` — coordenada para amostrar o SDF (ver abaixo). Também produz uma **lista de arestas do contorno** (pares de vértices no plano frontal) para o emissor de faíscas (§6.4).

Parâmetros (de `Config.geometry`):
- `depth` — profundidade total da extrusão.
- `bevelMode` — `shading` (padrão) | `geometry` | `off`.
- `bevelSize` — largura do chanfro no plano da tampa.
- `bevelDepth` — o quanto o chanfro recua em Z.
- `bevelSegments` — só no modo `geometry` (2–8).
- `shell` — casca oca on/off.
- `wallThickness` — espessura da parede no modo shell.
- `quality` — tolerância de achatamento de curva + limiar de suavização de canto.

#### Passos comuns

1. **Tampa**: todos os contornos → `libtess2` (`tessAddContour` por contorno, `tessTesselate(TESS_POLYGONS, ...)` com a regra de winding de `fillRule`) → triângulos 2D.
   - Frente: em `z = +depth/2 - bevelDepth` (recuada para dar lugar ao chanfro), normal `+Z`, `surfaceId = 0`.
   - Verso: mesmos triângulos, winding invertido, `z = -depth/2 + bevelDepth`, normal `-Z`, `surfaceId = 1`.
   - Se `shell`: em vez da tampa cheia, faz **inset** de todos os contornos por `wallThickness` (offset único; se auto-intersectar numa peça, `shell` é ignorado nessa peça e registrado no log), adiciona os contornos internos como furos e triangula só o anel. Adiciona paredes internas (`surfaceId = 2`, normal para dentro).
2. **Paredes laterais**: quads ligando a borda da tampa frontal à da tampa traseira ao longo de `depth - 2*bevelDepth`. Normal = normal da aresta do contorno (para fora). Normais **divididas** em cantos com ângulo acima do limiar de `quality`; suaves abaixo. `surfaceId = 2`.
3. **Micro-bevel geométrico** (presente **apenas** no modo `bevelMode = shading`; no modo `geometry` a faixa de bevel real já cuida da silhueta, no modo `off` não há chanfro nenhum): 1 anel de quads entre a borda da tampa e o topo da parede lateral, recuo fixo `= min(bevelSize, depth*0.01)` em Z e no plano. Pequeno demais para auto-intersectar; existe só para suavizar a silhueta. `surfaceId = 3`.
4. Solda vértices coincidentes, calcula normais suaves onde não foram explicitamente divididas, sobe **um** VBO + EBO interleaved por peça.

#### Modo `bevelMode = shading` (padrão) — bevel via SDF

- No load da peça, gera um **SDF assinado** dos contornos 2D num textura R16F (512–1024 px, resolução conforme `quality`), via **8SSEDT** (Danielsson) na CPU: distância não-assinada dos dois lados + teste dentro/fora pela regra de winding → distância assinada em unidades de em. Canais G,B guardam o gradiente `∇dist` normalizado (direção da borda mais próxima). Cacheado; regenerado só quando conteúdo/fonte/`quality` mudam.
- No **fragment shader das tampas** (frente/verso, `surfaceId 0/1`): amostra o SDF na posição local. Se `dist < bevelSize`:
  `t = falloff(dist / bevelSize)` (curva quarto-de-círculo),
  `edgeDir3D = vec3(gradient.xy, ±k)` (aponta para fora e para trás),
  `N = normalize(mix(faceNormal, edgeDir3D, t))`.
  Resultado: o realce especular rola pela quina como num chanfro real. Silhueta continua reta (aceito — ver §16).
- Na **última faixa da parede lateral** (topo, `surfaceId 2` com `v` de altura perto de 1): blend simétrico da normal da parede para a normal da face, fechando o quarto-de-círculo na junção.
- `bevelSegments` não tem efeito neste modo.

#### Modo `bevelMode = geometry` — bevel real

- **Faixa de bevel**: offset de cada contorno para dentro por `bevelSize` usando **bissetriz de ângulo** por vértice. `bevelSegments` anéis interpolando posição da borda original (`z = ±(depth/2 - bevelDepth)`... na verdade da borda da tampa) até o contorno offsetado (`z = ±depth/2`), com perfil de **interpolação cosseno** (quarto-de-círculo) e normais suaves ao longo da faixa. Duas faixas: frente e verso. `surfaceId = 3`.
- **Cantos côncavos / traços finos**: se o offset de um vértice cruzar uma aresta vizinha (auto-interseção detectada por teste de orientação), aquele vértice é **clampado** à interseção (colapso local). Pode gerar artefato visual — é a razão de o modo ser opcional. Registrado no log em nível debug.
- É o mesmo comportamento problemático de bevel do Blender; oferecido para o caso de, no conteúdo específico do usuário, ficar melhor que o modo `shading`. O mini-preview do diálogo permite comparar lado a lado.

#### Config e UX do bevel

- Diálogo (aba **Movimento**/**Geometria**): dropdown "Bevel: **Sombreado (recomendado)** / Geometria / Desligado", sliders `bevelSize` e `bevelDepth`, slider `bevelSegments` (habilitado só no modo Geometria), checkbox "Casca oca" + slider `wallThickness`.
- Preset "Clássico" usa `shading`.

### 5.6 Cache de geometria

Recalcular a geometria (fonte, SVG, mesh, SDF) só quando um parâmetro que a afeta muda. Chave de cache = hash de (modo de conteúdo, texto/caminho, fonte, `geometry.*`). Em saver mode a geometria é montada uma vez no startup. No diálogo, um debounce (~150 ms) evita reconstruir a cada tick de slider.

## 6. Cena (`src/scene.c`)

### 6.1 Câmera

- Perspectiva, FOV vertical ~35°.
- **Auto-fit**: dado o raio da esfera envolvente do modelo e o aspect do monitor, posiciona a câmera na distância em que o modelo ocupa ~70% da menor dimensão. Recalculado quando o modelo ou o aspect mudam.
- Sem controle de usuário; olhar fixo no centro do modelo.

### 6.2 Movimento — pêndulo limitado (`src/scene.c`, animador)

- Ângulo Y oscila entre `-motion.maxAngleY` e `+motion.maxAngleY` (graus; padrão 45°, faixa 5–170°) com **easing ease-in-out** (curva suave, C1 nos extremos — sem "batida").
- `motion.tiltX` (padrão ~8°, faixa 0–30°): pequeno balanço no eixo X, **defasado** ~90° em relação ao Y, para dar organicidade.
- `motion.period` (padrão ~8 s, faixa 2–30 s): tempo de um ciclo Y completo (ida e volta).
- `motion.easing`: `smooth` (padrão) | `linear` | `sine`.
- Nunca rotaciona além de `maxAngleY` → o verso do texto nunca fica de frente (que é o pedido original).
- Opção de órbita de câmera (em vez de girar o objeto) fica **fora do v1** (a pergunta do brainstorming escolheu pêndulo do objeto).

### 6.3 Iluminação

- 3 luzes direcionais fixas em espaço de mundo: **key** (frente-cima-direita, quente, intensa), **fill** (frente-baixo-esquerda, fria, fraca), **rim** (atrás-cima, branca, média — separa o modelo do fundo sem sombra projetada).
- Parâmetros afinados para reproduzir o visual do `sstext3d` no material "especular clássico".
- Não configuráveis no v1 (só o `material.mode` e `baseColor`).

### 6.4 Partículas (`src/particles.c`)

Um sistema, simulação em CPU (limite ~4000 partículas), um VBO dinâmico atualizado por frame, desenhado como **point sprites aditivos** com textura de disco suave, renderizado **dentro do buffer HDR** (então recebe bloom).

`effects.particles.kind`:
- `dust` — deriva lenta + curl noise; wrap numa caixa em espaço de view; paralaxe por profundidade; tamanho ∝ 1/z.
- `bokeh` — como dust, discos grandes e suaves, contagem baixa, leve anel de desfoque na textura.
- `sparks` — nascem em pontos aleatórios da lista de arestas do modelo (§5.5), velocidade inicial para fora + cima, gravidade leve, vida ~1–2 s, fade + encolhe.
- `stars` — campo quase estático, gira devagar acompanhando o pêndulo, cintila por fase individual.

Config: `kind`, `density` (0–1 → contagem), `speed`, `sizeScale`.

### 6.5 Fundo (`src/scene.c` + `shaders/background.frag`)

Triângulo fullscreen com `depth = 1.0` (atrás de tudo). `background.type`:
- `solid` — `color1`.
- `gradient` — 2–4 stops entre `color1` e `color2`, ângulo `gradAngle`, com dithering ordenado para não fazer banda no HDR→LDR.
- `image` — `stb_image` carrega `imagePath`; modo `imageFit` = `cover` | `contain` | `tile`; `panSpeed` desloca a UV lentamente (ping-pong nas bordas quando não é tile).
- `nebula` — fBm com domain-warp (4–5 oitavas de value noise) no fragment shader, 2 rampas de cor (`nebulaColors[2]`), evolução lentíssima (`time * 0.01`). Sem arquivo.

## 7. Materiais (`src/material.c` + `shaders/model.frag`)

Um über-shader, uniform `int uMode`. Entrada comum: normal de mundo (já com o ajuste de bevel do §5.5), direção de view, as 3 luzes, `material.baseColor`, `surfaceId`.

| `uMode` | Material | Modelo |
|---|---|---|
| 0 | **Especular clássico** (padrão) | Lambert × baseColor + Blinn-Phong (shininess alto, specular branco) + termo de rim. Aproxima o `sstext3d`, um pouco mais rico. |
| 1 | **Metálico** | Amostra um **matcap** embutido (ou um env lat-long de `material.envImagePath` se fornecido) por `reflect(view, N)`; tinta por `baseColor`; boost de fresnel na borda. |
| 2 | **Vidro** | Fresnel entre reflexão (matcap/env) e uma tinta refratada (amostra do buffer de fundo deslocada por `N.xy`); glints especulares; opacidade de borda. Renderizado com **WBOIT** (weighted-blended OIT, McGuire & Bavoil) — order-independent, sem passo de ordenação, funciona com shell. |
| 3 | **Fosco** | Lambert "wrapped" + specular baixíssimo + AO sutil derivado de `surfaceId` (escurece `surfaceId 2` de shell e paredes internas). |

`material.metalness` e `material.roughness` modulam os modos 1 (mistura reflexão↔baseColor, nitidez do reflexo) e 3 (força do specular residual); nos modos 0 e 2 são ignorados.

Cores por peça: `material.baseColor` global, **exceto** quando `content.svgColorMode = preserve` (aí cada peça usa o `fillColor` do seu `ContourSet`) ou quando a malha importada usa materiais próprios (`content.meshUseFileMaterials`).

`surfaceId` também é usado para manter o bevel (`3`) sempre no máximo de brilho especular — é onde o glint "lê".

## 8. Pós-processamento (`src/post.c` + `shaders/post_*.frag`)

### 8.1 Frame graph — modo saver, qualidade cheia

```
1. Passo de cena  → RT HDR MSAA (RGBA16F + depth24, MSAA = perf.msaa)
      a. fundo (fullscreen, depth=1)
      b. modelo opaco (model.frag, uMode)
      c. modelo transparente (vidro) via WBOIT — 2 alvos aux (accum + revealage)
      d. partículas (aditivo)
   resolve MSAA + resolve WBOIT → RT HDR (single-sample)
2. Bright-pass (threshold + soft knee)                → bloomTex (meia resolução)
3. Pirâmide de downsample (filtro 13-tap)             → mip 1..5
4. Upsample + tent-blur aditivo (mip5→mip1)           → bloom combinado
5. Streaks de difração (só se effects.streaks.on):
      a partir do bright buffer, `count` direções (4/6/8) a 0/45/90/135/...,
      2–3 iterações de blur de passo crescente por direção                → streaksTex
6. Composição:  scene + bloom * effects.bloom.intensity
                       + streaks * effects.streaks.intensity              → compTex
7. Aberração cromática (só se effects.chroma.on):
      amostra compTex em R/G/B com deslocamento de UV
      = dir_do_centro * effects.chroma.strength * r²                      → caTex
8. Vinheta (multiplicativa, effects.vignette.amount)
9. Tonemap ACES filmic + gamma 2.2
10. FXAA (só se effects.fxaa)                          → framebuffer default do monitor
```

- **Modo preview** (`/p`): só passos 1 (sem MSAA ou MSAA 2) + 9. Sem bloom/streaks/CA/partículas/vidro-WBOIT (vidro cai para alpha simples). O **mini-preview do diálogo** roda a cadeia completa (bloom, streaks etc.) para dar feedback ao vivo dos controles — só `/p` (a caixinha de Proteção de Tela do Windows) usa o caminho reduzido.
- **Modo reduzido** (fallback, §11): igual ao preview; pode subir para incluir bloom se a medição de desempenho permitir.
- `perf.renderScale` (0.5–1.0): a cena e o pós rodam nessa fração da resolução do monitor; o passo final faz upscale.

### 8.2 Alvos de render

Gerenciador simples de RTs em `src/gl_core.c`: aloca/realoca conforme `(resolução do monitor * renderScale)` e o nível de qualidade. Todos liberados na troca de monitor / saída. `perf.msaa` é limitado ao `min(valor, GL_MAX_SAMPLES, GL_MAX_COLOR_TEXTURE_SAMPLES)` — GPUs que não suportam o nível pedido em RGBA16F caem para o maior suportado.

## 9. Modos de conteúdo

`content.mode`:

- **`text`** — `content.text` (UTF-8, multilinha via `\n`), fonte `font.family` + `bold` + `italic`. Pipeline §5.2 → §5.5.
- **`clock`** — string gerada a cada segundo via `GetTimeFormatEx` / `GetDateFormatEx` com `LOCALE_NAME_USER_DEFAULT` (formato **localizado do Windows**). `content.clockShowDate` (bool), `content.clockShowSeconds` (bool). A string passa pelo mesmo pipeline de texto §5.2; a geometria é reconstruída quando a string muda (uma vez por minuto, ou por segundo se `clockShowSeconds`). Cache do §5.6 evita custo perceptível.
- **`svg`** — `content.svgPath`; `content.svgColorMode` = `preserve` | `single`. Pipeline §5.3 → §5.5 (uma peça por shape).
- **`mesh`** — `content.meshPath` (`.obj`/`.glb`/`.gltf`/`.stl`); `content.meshUseFileMaterials` (bool). Pipeline §5.4.

Erro de carga (arquivo ausente, parse falhou) → mostra um texto de aviso curto (localizado) no lugar, com o mesmo material, e registra no log. O saver **não** encerra por isso.

## 10. Configuração

### 10.1 Struct `Config` (`src/config.c`)

```
content:    mode(text|clock|svg|mesh), text(utf8), clockShowDate(bool),
            clockShowSeconds(bool), svgPath, svgColorMode(preserve|single),
            meshPath, meshUseFileMaterials(bool)
font:       family, bold(bool), italic(bool)
geometry:   depth, bevelMode(shading|geometry|off), bevelSize, bevelDepth,
            bevelSegments, shell(bool), wallThickness, sizeScale, quality
motion:     maxAngleY(deg), tiltX(deg), period(s), easing(smooth|linear|sine)
material:   mode(0..3), baseColor(rgb), metalness, roughness, envImagePath
background: type(solid|gradient|image|nebula), color1, color2, gradAngle,
            imagePath, imageFit(cover|contain|tile), panSpeed, nebulaColors[2]
effects:
  bloom:     on, threshold, intensity, radius
  chroma:    on, strength
  streaks:   on, intensity, length, count(4|6|8)
  particles: on, kind(dust|bokeh|sparks|stars), density, speed, sizeScale
  vignette:  on, amount
  fxaa:      on
perf:       fpsCap(30|60|120|off), vsync(bool), msaa(0|2|4|8),
            renderScale(0.5..1.0), autoQuality(bool)
ui:         language(auto|pt|en)
preset:     name
version:    int (do schema; para migração)
```

### 10.2 Armazenamento (`src/config.c`)

- Fonte da verdade: `HKCU\Software\Modern3DText`, um valor por campo (nomes achatados: `geometry.depth` etc.). `REG_DWORD` para int/bool/float-como-bits, `REG_SZ` para strings.
- **Load**: campo ausente ou fora da faixa → valor padrão. `version` menor que o atual → aplica migrações. Chave inexistente → todos os padrões (primeiro uso) + grava o preset "Clássico".
- **Import/Export `.ini`**: botão no diálogo; formato `[secao]\nchave=valor`; mesma validação. Facilita compartilhar configs e versionar presets do usuário.
- Sem escrita fora de `HKCU` e `%LOCALAPPDATA%\Modern3DText\` (log, cache de SDF).

### 10.3 Diálogo (`res/screensaver.rc` `DIALOGEX` + `src/config_dialog.c`)

- ~520×380 dip, DPI-aware, manifest com comctl v6.
- **Esquerda:** `SysTabControl32` com abas: **Conteúdo · Movimento · Material · Fundo · Efeitos · Desempenho**.
- **Direita:** janela-filha ~220×220 com contexto GL rodando **a cena real** (mesmo código de render, modo preview) em baixa resolução, pêndulo curto, atualizando ao vivo (debounce §5.6) conforme os controles mudam.
- **Rodapé:** dropdown **Preset ▾** · `Importar…` · `Exportar…` · `OK` · `Cancelar` · `Aplicar`.
- Controles: `msctls_trackbar32` com rótulo numérico ao lado; `ChooseColorW` atrás de botões de amostra owner-drawn; `GetOpenFileNameW` para SVG (`*.svg`), malha (`*.obj;*.glb;*.gltf;*.stl`), imagens (`*.png;*.jpg;*.jpeg;*.bmp;*.tga`); combos para enums; enumeração de fontes via `EnumFontFamiliesExW`.
- `Aplicar` grava sem fechar. `OK` grava e fecha. `Cancelar` descarta.
- Trocar de preset com alterações não salvas → aviso.

### 10.4 Presets (`src/presets.c`)

Literais `Config` estáticos, também usados como default de fábrica:

| Preset | Ideia |
|---|---|
| **Clássico** | Especular clássico, fundo gradiente escuro, bloom sutil, sem CA/streaks/partículas, bevel sombreado. Aproxima o `sstext3d`. Default no 1º uso. |
| **Cinema** | Metálico, fundo nebulosa, bloom + streaks anamórficos + CA leve + vinheta, partículas bokeh. |
| **Néon** | Especular clássico com `baseColor` saturada, fundo sólido escuro, bloom forte + streaks 6 pontas + CA média, partículas sparks. |
| **Suave** | Fosco, fundo gradiente claro, bloom baixíssimo, sem mais nada, partículas dust. Leve para máquinas fracas. |

### 10.5 Internacionalização (`src/i18n.c`)

- Tabela `const wchar_t* STR[LANG_COUNT][STR_COUNT]` (PT e EN). Nenhuma string de UI hardcoded fora dela.
- Idioma efetivo: `ui.language`; se `auto`, `GetUserDefaultUILanguage() & 0x3FF == LANG_PORTUGUESE` → PT, senão EN.
- Só o diálogo de config é traduzido (o saver não tem texto de UI, exceto mensagens de erro de carga, que também saem da tabela).

## 11. Fallback / robustez (`src/host_win32.c` + `src/render_tiers.c`)

Ordem no startup:

1. **Contexto GL**: tenta 3.3 core → 3.1 → 2.1. Falhou tudo → **fallback GDI 2D**: `src/fallback_gdi.c` desenha `content.text` (ou a string do relógio) com `ExtTextOutW` + sombra falsa deslocada, fundo `background.color1`, ainda encerra no input. Sem SVG/mesh/efeitos.
2. **Renderer suspeito**: `GL_RENDERER` contém `GDI Generic`, `llvmpipe`, `softpipe`, `Microsoft Basic Render`, `D3D12 (WARP)` → força **modo reduzido** (sem pós, sem MSAA, partículas off, `renderScale = 0.75`).
3. **Sessão remota**: `GetSystemMetrics(SM_REMOTESESSION)` → modo reduzido por padrão.
4. **Medição adaptativa** (só se `perf.autoQuality`): nos primeiros ~30 frames mede o tempo de GPU (via `GL_TIME_ELAPSED` query). Sustentado acima de ~22 ms (≈45 fps no alvo) → degrada em degraus, cada um remedido, com **histerese** (não volta a subir por 5 s):
   `streaks off → bloom off → MSAA 4→2→0 → renderScale 1.0→0.75→0.5 → modo reduzido`.
5. **Falha dura de GL** em qualquer ponto do init (shader não compila, FBO incompleto, out of memory) → log em `%LOCALAPPDATA%\Modern3DText\log.txt` e cai para o próximo nível em vez de crashar.
6. O **mini-preview do diálogo** faz sua própria checagem de nível, independente — GPU fraca ainda dá um diálogo utilizável.

`perf.*` são sobrescrevíveis pelo usuário; `autoQuality = on` é o padrão.

## 12. Build (`build/Makefile`)

### 12.1 Setup do toolchain (README + `toolchain.txt`)

1. Baixar o release do w64devkit (versão fixada), extrair em `C:\w64devkit`.
2. `C:\w64devkit\bin` no `PATH`, ou usar o shell `w64devkit.exe`.
3. Verificar: `gcc --version`, `mingw32-make --version`, `windres --version`.

### 12.2 Alvos

| Alvo | Efeito |
|---|---|
| `make` / `make release` | `.scr` otimizado em `dist/Modern3DText.scr` |
| `make debug` | `-g -O0 -DDEBUG`, subsistema console, logs em stderr |
| `make run` | build debug + roda `dist\Modern3DText.scr /s` (override de janela via env `M3DT_WINDOWED=1`) |
| `make config` | roda `/c` |
| `make test` | compila e roda os testes unitários (`build/tests`) |
| `make test-visual` | golden-image (precisa de GPU) |
| `make bless` | regenera as referências golden-image |
| `make installer` | chama `ISCC.exe installer/setup.iss` → `dist/Modern3DText-Setup.exe` |
| `make zip` | monta o zip portátil |
| `make clean` | limpa `build/obj`, `dist`, `generated` |

### 12.3 Passos do build release

1. Compila `build/tools/embed.c` → `build/embed.exe` (ferramenta de host).
2. `embed.exe shaders/*.vert shaders/*.frag res/fallback-font.ttf res/matcaps/*.png` → `generated/embedded.h` (arrays de bytes + tamanhos + índice).
3. Compila cada `src/**/*.c` e `third_party/**/*.c` → `build/obj/*.o`.
   Flags: `-std=c11 -O2 -municode -fno-strict-aliasing -Wall -Wextra -Isrc -Ithird_party -Igenerated`.
   (`-ffast-math` **não** usado — evita instabilidade nos testes de math; shaders GLSL não são afetados de qualquer forma.)
4. `windres res/screensaver.rc build/obj/screensaver.res` — template do diálogo, `icon.ico`, `VERSIONINFO`, manifest (DPI per-monitor v2, comctl v6).
5. Link:
   `gcc build/obj/*.o build/obj/screensaver.res -o dist/Modern3DText.scr -mwindows -municode -static -static-libgcc -s -lopengl32 -lgdi32 -luser32 -lkernel32 -lcomdlg32 -lcomctl32 -lshell32 -lole32 -ladvapi32 -ldwmapi`
   - `-mwindows` = subsistema GUI (sem console). `-static` = sem DLL do libgcc/winpthread. `-s` = strip.
   - O `.o`/`.exe` resultante é renomeado `.scr` (é só um PE — a extensão é o truque inteiro).
6. **Checagem de tamanho**: falha o build se `dist/Modern3DText.scr` > 3 MB. Esperado ~0,7–1,5 MB.

### 12.4 `tools/make_icon`

Pequeno programa C (compilado com o mesmo gcc) que gera `res/icon.ico` proceduralmente (um prisma / "3D" estilizado, com difração) em 16/32/48/256 px, usando `stb_image_write` para os PNGs internos e um writer de container ICO. Placeholder; trocável por arte real depois.

## 13. Empacotamento e distribuição

### 13.1 Inno Setup (`installer/setup.iss`)

- Instala `Modern3DText.scr` em `{sys}` (modo x64 — `ArchitecturesInstallIn64BitMode=x64compatible`).
- `PrivilegesRequired=admin` (escrita em System32).
- Atalho no Menu Iniciar: "Configurar Modern 3D Text" → `Modern3DText.scr /c`.
- Checkbox opcional "Definir como proteção de tela agora" → grava `HKCU\Control Panel\Desktop\SCRNSAVE.EXE`, `SCRNSAVE.EXE` ativo, timeout padrão.
- Desinstalador: remove o `.scr`; pergunta se deve remover `HKCU\Software\Modern3DText` e `%LOCALAPPDATA%\Modern3DText`.

### 13.2 Zip portátil

`Modern3DText.scr` + `LEIAME.txt` / `README.txt` (PT + EN): "clique com o botão direito → Instalar, ou copie para `C:\Windows\System32`". Sem admin se o usuário só quer testar via botão direito → "Testar".

### 13.3 SmartScreen / assinatura

- Sem certificado Authenticode, o instalador e o `.scr` disparam "O Windows protegeu o seu PC". Documentado no README com captura e o passo "Mais informações → Executar assim mesmo".
- Opções registradas para o futuro (fora do v1): (a) certificado OV/EV (~US$200–400/ano); (b) submeter o binário ao Microsoft para reputação; (c) manifest `winget`; (d) build reproduzível + `SHA256SUMS.txt` no release (este entra já no v1).

### 13.4 Release

Cada release do GitHub: `Modern3DText-Setup.exe`, `Modern3DText-portable.zip`, `SHA256SUMS.txt`, notas de versão.

## 14. Testes

### 14.1 Unitários (`build/tests/`, runner C com asserts, `make test`)

TDD nas partes determinísticas — escrever o teste antes da implementação.

- **`util/math`** — vec/mat; curva do pêndulo: `angle(t) ∈ [-max, max]`, simétrica em torno de `t=0.5`, derivada ≈ 0 nos extremos.
- **`cmdline`** — `/s`, `/S`, `-s`, `/p 12345`, `/p:12345`, `/c`, `/c:12345`, `""`, `/s /p 1`, lixo → `(modo, hwnd)` esperado.
- **`config`** — round-trip struct↔registro (backend KV em memória no teste); parse/escrita `.ini`; campo ausente/fora de faixa → default; `version` antiga → migração; chave desconhecida no `.ini` → ignorada sem erro.
- **`geometry/contour_mesh`** — contornos-fixture (quadrado; quadrado com furo; "L" côncavo; dois quadrados separados):
  - estanqueidade: toda aresta compartilhada por exatamente 2 triângulos (exceto pontas abertas do shell, por design);
  - normais: frente ≈ `+Z`, verso ≈ `-Z`, laterais ⟂ `Z`;
  - contagens de vértice/triângulo dentro de faixas esperadas para `bevelSegments` dado;
  - entrada degenerada (vazia, 1 ponto, colinear) → malha vazia, sem crash;
  - modo `geometry` num "L" côncavo com `bevelSize` grande → não crasha, produz malha (mesmo que clampada).
- **`geometry/sdf`** — SDF de um círculo conhecido: `dist` no centro ≈ `-raio`, na borda ≈ 0, gradiente aponta radialmente; erro < 1 texel.
- **`geometry/svg_shapes`** — fixtures `.svg` (fill sólido; furo via evenodd; stroke-only; `transform`) → contagem de contornos, `fillColor`, bbox.
- **`geometry/font_outline`** — com `res/fallback-font.ttf` (determinística): 'A' → 2 contornos (externo + furo); "AV" → avanço com kerning < soma dos avanços isolados.
- **`geometry/mesh_import`** — fixtures mínimas: `.obj` (triângulo + quad, 2 materiais), `.glb` (idem), `.stl` (bin + ascii) → contagens; normalização: `|centroide| < ε`, `raio ≈ alvo`.

### 14.2 Golden-image (`make test-visual`, opt-in, precisa de GPU)

- ~10–12 cenas fixas: cada `content.mode` × 2 materiais × (efeitos on/off) × (bevel shading/geometry). Render headless (janela oculta + `glReadPixels`) em tamanho e seed fixos → PNG.
- Compara com referências commitadas em `tests/golden/` por métrica perceptual (SSIM ou ΔE médio) com tolerância. `make bless` regenera deliberadamente.

### 14.3 QA manual (checklist em `docs/qa-checklist.md`)

- `/c`: 100 / 150 / 200 % DPI; todas as abas; todos os pickers; troca de preset; `Aplicar` seguido de `Cancelar`; import/export `.ini`.
- `/p`: no diálogo real de Proteção de Tela do Windows — anima, não crasha, libera limpo ao fechar.
- `/s`: 1 monitor; 2 monitores (mesma e diferentes resoluções/orientações); desconexão de monitor a quente; `Win+L` durante execução; limiar de mouse (tremor não deve encerrar); clique e teclado encerram; `Alt+Tab`.
- Fallback: forçar via `M3DT_FORCE_TIER=reduced|gdi`; sessão RDP real; `GL_RENDERER` de WARP.
- Instalação: Inno (install + uninstall, com e sem "definir agora"); zip portátil (botão direito → Instalar / Testar); texto do prompt do SmartScreen.
- Estabilidade: 30 min rodando — RAM estável, handles `GDI`/`USER` estáveis (Process Explorer), contexto GL destruído na saída (sem `wglMakeCurrent` pendente).
- iGPU Intel real, se disponível.

### 14.4 CI (`.github/workflows/ci.yml`, opcional mas recomendado)

- Runner `windows-latest`: instala w64devkit (cache), `make test`, `make release`, publica `Modern3DText.scr` como artefato.
- Job de release em tag: `make installer`, `make zip`, gera `SHA256SUMS.txt`, cria o GitHub Release.
- Golden-image só se um runner com GPU for configurado; senão fica manual.

## 15. Layout do projeto

```
modern3dtext/
  src/
    main.c                  # WinMain, dispatch de modo
    cmdline.c/.h             # parse de /s /p /c
    host_win32.c/.h          # janelas, contexto WGL, monitores, watcher de input, loop
    gl_core.c/.h             # glad + helpers (shader, VBO, RT, passe fullscreen, queries)
    render_tiers.c/.h        # detecção e degraus de qualidade
    fallback_gdi.c/.h        # saver 2D sem GL
    scene.c/.h               # câmera, pêndulo, luzes, fundo
    material.c/.h
    post.c/.h                # bloom, streaks, CA, vinheta, tonemap, fxaa
    particles.c/.h
    config.c/.h              # struct Config + load/save registro + .ini
    config_dialog.c/.h       # lógica do diálogo /c + mini-preview
    presets.c/.h
    i18n.c/.h
    geometry/
      font_outline.c/.h
      svg_shapes.c/.h
      mesh_import.c/.h
      contour_mesh.c/.h       # tesselação + extrusão + micro-bevel + shell
      sdf.c/.h                # 8SSEDT + gradiente
    util/
      log.c/.h  mathx.c/.h  arena.c/.h  fileio.c/.h  str.c/.h
  shaders/
    model.vert  model.frag
    background.vert  background.frag
    post_brightpass.frag  post_downsample.frag  post_upsample.frag
    post_streaks.frag  post_composite.frag  post_chroma.frag
    post_tonemap.frag  post_fxaa.frag
    particle.vert  particle.frag
    fullscreen.vert
  third_party/
    glad/            stb_truetype.h  stb_image.h  stb_image_write.h
    nanosvg.h        fast_obj.h      cgltf.h      libtess2/
  res/
    screensaver.rc   icon.ico        fallback-font.ttf   matcaps/*.png
  assets/            svg/*.svg  mesh/*.{obj,glb,stl}   (fixtures + exemplos)
  build/
    Makefile
    tools/  embed.c   make_icon.c
    tests/  *.c   test_main.c
  tests/golden/       *.png
  installer/          setup.iss
  docs/
    superpowers/specs/2026-09-10-modern-3d-text-screensaver-design.md
    qa-checklist.md
  toolchain.txt       README.md       LICENSE (MIT)      .gitignore
```

## 16. Riscos e mitigações

| # | Risco | Mitigação |
|---|---|---|
| 1 | Bevel geométrico auto-intersecta em cantos côncavos / traços finos | Padrão é o modo **shading** (SDF-normal), que nunca quebra. Modo `geometry` é opt-in; auto-interseção é clampada; mini-preview permite comparar. |
| 2 | Modo shading não arredonda a **silhueta** (quina viva contra fundo claro / ângulo raso) | Micro-bevel geométrico fixo suaviza a silhueta. Aceitável para screensaver visto ~de frente com pêndulo. Documentado. |
| 3 | `.ttc` / fontes variáveis: `GetFontData` devolve a coleção; offset errado | `GetFontData('ttcf')` + `stbtt_GetFontOffsetForIndex`. Casos raros → fallback Segoe UI. |
| 4 | Vidro + shell: muitas superfícies transparentes | **WBOIT** — order-independent, sem passo de ordenação, funciona com shell. Sem restrição de UI. |
| 5 | Tempo de vida do `/p`: HWND pai morre de repente | `IsWindow(parent)` a cada frame + `WM_DESTROY` → teardown limpo do contexto GL. Item no QA. |
| 6 | Multi-monitor com DPI/orientação mistos | Render por monitor na resolução nativa dele; contextos independentes. |
| 7 | Install em System32 exige admin | Zip portátil (botão direito → Instalar) cobre quem não quer admin. |
| 8 | SDF de SVG muito detalhado: 8SSEDT custa no load | Textura SDF em 512–1024 px conforme `quality`; cache em `%LOCALAPPDATA%`; regenera só quando muda. |
| 9 | Contextos GL por monitor multiplicam uso de VRAM | RTs alocadas por `renderScale`; liberadas na troca de monitor; `autoQuality` degrada se apertar. |
| 10 | Nanosvg com gradientes/estilos avançados | v1: gradiente → cor média; recursos não suportados listados em §2; erro de parse → texto de aviso, saver não encerra. |

## 17. Faseamento sugerido da implementação

Ordem pensada para ter algo rodando cedo e reduzir risco de integração. Cada fase termina com um `.scr` que instala e roda.

1. **Esqueleto `.scr`** — `cmdline` + `host_win32` (janela saver por monitor, contexto GL 3.3, watcher de input, saída limpa) + `/p` + `/c` abrindo um diálogo vazio. Render: só um `glClear` colorido. Testes: `cmdline`. Entregável: instala, "protege a tela", sai no input.
2. **Texto extrudado básico** — `font_outline` + `contour_mesh` (tampa + paredes + micro-bevel, **sem** SDF, **sem** shell) + `scene` (câmera auto-fit, pêndulo) + `material` modo 0 + `config` (registro + struct) ligado ao diálogo (aba Conteúdo + Movimento). Testes: `contour_mesh`, `config`, `mathx`.
3. **SDF + bevel shading + geometry + shell** — `sdf` (8SSEDT) + os dois modos de bevel + casca oca + cache de geometria. Testes: `sdf`, casos de `contour_mesh` para os modos.
4. **Pós-processamento** — RT HDR, tonemap, bloom, streaks, aberração cromática, vinheta, FXAA + aba Efeitos + `render_tiers`/`autoQuality` + modo preview. Golden-image começa aqui.
5. **Fundos + partículas** — `background` (solid/gradient/image/nebula) + `particles` (4 tipos) + abas Fundo/Efeitos completas.
6. **SVG + relógio** — `svg_shapes` + traço simples + `svgColorMode` + modo `clock` (formato localizado). Testes: `svg_shapes`, `font_outline`.
7. **Malha importada** — `mesh_import` (`.obj/.glb/.gltf/.stl`) + `meshUseFileMaterials` + materiais 1–3 + WBOIT para o vidro. Testes: `mesh_import`.
8. **Acabamento de distribuição** — i18n PT/EN, presets, import/export `.ini`, fallback GDI 2D, `tools/make_icon`, `installer/setup.iss`, zip portátil, `SHA256SUMS`, CI, `docs/qa-checklist.md`, README. QA manual completo.

## 18. Escopo do v1

**Dentro:**
- 4 modos (`/s /p /c` + sem args), multi-monitor (cena por monitor), watcher de input.
- Conteúdo: texto (multilinha, qualquer fonte instalada + bold/italic), relógio (formato localizado, data opcional), SVG (fill + traço simples), malha (`.obj/.glb/.gltf/.stl`).
- Extrusão: profundidade + bevel (shading/geometry/off) + micro-bevel + casca oca.
- Movimento: pêndulo Y limitado + tilt X + easing.
- Materiais: 4 modos + baseColor; cores de SVG preservadas ou únicas; materiais de glTF.
- Fundo: sólido / gradiente / imagem / nebulosa.
- Efeitos: bloom, aberração cromática, streaks, partículas (4 tipos), vinheta, FXAA — cada um toggle + parâmetros.
- 4 presets. Config Win32 com mini-preview ao vivo. i18n PT/EN auto.
- Fallback em degraus + GDI 2D. Log. Cache de SDF.
- Build w64devkit + Makefile. Instalador Inno + zip portátil + SHA256SUMS.
- Testes unitários + golden-image + checklist de QA. CI opcional.

**Fora (pós-v1):**
- Órbita de câmera como alternativa ao pêndulo.
- Assinatura de código / manifest winget.
- Animação de malhas importadas; primitivas glTF não-triângulo.
- SVG: dash, marker, pattern, clipPath, filtros, `mask`, texto.
- Luzes configuráveis; env maps HDR; saída HDR.
- Editor de presets do usuário na UI (o import/export `.ini` cobre o caso por enquanto).
- Sombras projetadas.

## 19. Glossário rápido

- **`.scr`** — executável PE do Windows com a extensão trocada; o Windows o chama com `/s`, `/p <hwnd>` ou `/c`.
- **w64devkit** — distribuição MinGW-w64 portátil (zip, sem instalador).
- **SDF** — signed distance field; textura onde cada texel guarda a distância assinada até o contorno da forma.
- **8SSEDT** — 8-points Signed Sequential Euclidean Distance Transform (Danielsson); algoritmo rápido de SDF em CPU.
- **WBOIT** — weighted-blended order-independent transparency; transparência sem ordenar por profundidade.
- **matcap** — material capture; textura esférica que codifica iluminação + material, amostrada pela normal em espaço de view.
- **Micro-bevel** — chanfro geométrico minúsculo e de tamanho fixo, só para suavizar a silhueta.
