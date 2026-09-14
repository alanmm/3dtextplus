# Fase 8b-1 — Novos Valores Padrão — Documento de Design

## 1. Objetivo

Substituir os valores atuais de `config_defaults()` (o estado inicial do
screensaver na primeira instalação, e o valor que qualquer campo ausente
do registro recebe) pela configuração que o usuário definiu como o
"visual ideal" do projeto. Primeira de 7 sub-fases planejadas pro
trabalho de "presets e valores padrão" (§17 do spec original,
retomado 2026-09-14) — as próximas (valor padrão ao ligar um recurso,
ocultar/reorganizar por modo em Material/Fundo, padrão por tipo de
partícula, mapa de ambiente embutido, sistema de presets) ficam pra
depois, cada uma com seu próprio ciclo spec→plano→execução.

## 2. Escopo

Puramente numérico — **nenhuma mudança de UI, nenhum campo novo na
`Config`**. Só os valores literais dentro de `config_defaults()` mudam,
mais os testes que hoje verificam os valores antigos.

Cores em RGB 0-255 fornecidas pelo usuário são convertidas pra 0..1 com
5 casas decimais (`R/255`).

| Campo | Valor atual | Valor novo | Origem |
|---|---|---|---|
| `text` | `"Modern 3D Text"` | `"3D Text+"` | Conteúdo |
| `base_r/g/b` | `0.72/0.74/0.78` | `0.72157/0.74118/0.78039` | Conteúdo, Cor (184,189,199) |
| `material_mode` | `0` (Clássico) | `1` (Metálico) | Material, opção padrão |
| `roughness` | `0.25` | `0.5` | Material (Metálico) |
| `depth` | `0.30` | `0.10` | Geometria (corrigido 2026-09-14) |
| `bevel_mode` | `0` (Sombreado) | `1` (Geométrico) | Geometria |
| `bevel_size` | `0.035` | `0.005` | Geometria |
| `bevel_depth` | `0.035` | `0.007` | Geometria |
| `bevel_segments` | `4` | `6` | Geometria |
| `wall_thickness` | `0.06` | `0.015` | Geometria |
| `quality` | `1` (Média) | `2` (Alta) | Geometria |
| `bloom_threshold` | `0.75` | `0.64` | Efeitos |
| `bloom_intensity` | `0.6` | `0.65` | Efeitos |
| `bloom_radius` | `0.55` | `0.27` | Efeitos |
| `streaks_mode` | `0` (Desligado) | `2` (Anamórfico) | Efeitos |
| `streaks_intensity` | `0.5` | `0.76` | Efeitos |
| `streaks_length` | `0.5` | `0.14` | Efeitos |
| `chroma_on` | `0` | `1` | Pós |
| `chroma_strength` | `0.4` | `0.52` | Pós |
| `vignette_on` | `0` | `1` | Pós |
| `background_type` | `0` (Sólido) | `1` (Gradiente) | Fundo, opção padrão |
| `bg_color1_r/g/b` | `0.02/0.03/0.05` | `0.06275/0.03922/0.03922` | Fundo (Gradiente), Cor 1 (16,10,10) |
| `bg_color2_r/g/b` | `0.05/0.08/0.14` | `0.04706/0.07451/0.13725` | Fundo (Gradiente), Cor 2 (12,19,35) |
| `bg_grad_angle` | `90.0` | `101.0` | Fundo (Gradiente) |
| `particles_on` | `0` | `1` | Partículas |
| `particles_density` | `0.5` | `0.43` | Partículas (Poeira) |
| `particles_speed` | `1.0` | `0.76` | Partículas (Poeira) |
| `particles_size_scale` | `1.0` | `0.8` | Partículas (Poeira) |
| `particles_opacity` | `1.0` | `0.5` | Partículas (Poeira) |

**Já batem com o pedido, sem mudança**: `content_mode` (texto),
`font_family` ("Segoe UI"), `font_bold` (1), `font_italic` (0),
`max_angle_y` (42), `tilt_x` (8), `period` (9), `metalness` (0.9, só
usado no Metálico — já era o valor certo por coincidência),
`shell` (0), `bloom_on` (1), `fps_cap` (60), `vsync` (1), `msaa` (4),
`render_scale` (1.0), `auto_quality` (1), `vignette_amount` (0.35),
`fxaa_on` (1), `particles_kind` (0, Poeira), `ui_language` (0,
automático).

**Fora de escopo desta sub-fase** (viram as próximas 6): os valores
de Material/Fundo/Partículas pros OUTROS modos/tipos que não o padrão
(ex.: Clássico, Sólido, Bokeh) — esses só importam quando a Fase 3/4/5
implementar o mecanismo de "aplicar o pacote daquele modo ao
selecioná-lo"; o mapa de ambiente embutido; o sistema de presets.

## 3. Testes

`build/tests/test_config.c` já tem um bloco de asserts sobre
`config_defaults()` checando os valores ANTIGOS (`nearf(d.depth,
0.30f)`, etc.) — precisam ser atualizados pros novos números, senão
quebram assim que `config_defaults()` mudar. Mesmo padrão de sempre:
atualizar as asserções pra refletir o novo comportamento esperado, não
apagar a cobertura.

## 4. Fora de escopo

Todo o resto do trabalho de "presets e valores padrão" listado no
brainstorming de 2026-09-13/14 (valor padrão ao ligar um recurso,
ocultar/reorganizar Material e Fundo por modo, padrão por tipo de
partícula, mapa de ambiente embutido, sistema de presets) — cada um
vira sua própria sub-fase.
