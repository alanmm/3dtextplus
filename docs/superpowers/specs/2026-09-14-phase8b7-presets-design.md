# Fase 8b-7 — Sistema de Presets — Documento de Design

## 1. Objetivo

Sétima e última sub-fase do Fase 8b. Rodapé fixo no dialog de config
com um dropdown de presets + Salvar/Apagar/Importar/Exportar. 4
presets embutidos (Clássico/Cinema/Néon/Suave, já descritos no spec
original §10.4) mais presets salvos pelo usuário, todos afetando só o
**visual** — Texto/Fonte/Conteúdo/Movimento do usuário nunca são
tocados por um preset.

## 2. Escopo — quais campos um preset toca

Um preset SEMPRE define os campos abaixo (nunca "deixa como estava" —
previsibilidade total: aplicar o mesmo preset duas vezes dá o mesmo
resultado). Todo campo de `Config` fora desta lista fica intocado.

- `material_mode`, `metalness`, `roughness`, `env_mode` (nunca mexe em
  `env_path` — se o preset usa `env_mode=1` sem um arquivo já
  escolhido, cai no fallback procedural existente, comportamento já
  correto)
- `bevel_mode` (NÃO mexe em `depth`/`bevel_size`/`bevel_depth`/
  `bevel_segments`/`shell`/`wall_thickness` — nenhuma das 4 descrições
  do spec original menciona esses campos)
- `background_type`, `bg_color1_r/g/b`, `bg_color2_r/g/b`,
  `bg_grad_angle`, `bg_neb_color1_r/g/b`, `bg_neb_color2_r/g/b` (não
  mexe em `bg_image_*` — nenhum preset usa fundo tipo Imagem)
- `bloom_on`, `bloom_threshold`, `bloom_intensity`, `bloom_radius`
- `streaks_mode`, `streaks_intensity`, `streaks_length`
- `chroma_on`, `chroma_strength`
- `vignette_on`, `vignette_amount`
- `fxaa_on`
- `particles_on`, `particles_kind`, `particles_density`,
  `particles_speed`, `particles_size_scale`, `particles_opacity` **e**
  o par de memória por tipo correspondente ao `particles_kind` do
  preset (ex.: preset Cinema usa kind=Bokeh → também grava
  `particles_bokeh_density/size/opacity` com os mesmos valores, pra
  não haver um valor "fantasma" antigo memorizado se o usuário depois
  navegar manualmente pra outro tipo e voltar)
- `base_r`, `base_g`, `base_b` (cor do texto)
- `quality`

## 3. Os 4 presets embutidos

`src/presets.c` novo: cada preset é uma função
`void preset_classico(Config *out)` etc., que parte de
`config_defaults(out)` e sobrescreve só os campos da lista da seção 2
(preserva o que já veio de `config_defaults()` pros campos fora do
escopo). Um array `const PresetEntry g_builtin_presets[4]` com
`{ nome, função }` pra popular o dropdown.

| Campo | Clássico | Cinema | Néon | Suave |
|---|---|---|---|---|
| material_mode | 0 Clássico | 1 Metálico | 0 Clássico | 3 Fosco |
| metalness/roughness | (n/a) | 0.9 / 0.5 | (n/a) | (n/a) |
| env_mode | (n/a) | 0 Embutida | (n/a) | (n/a) |
| bevel_mode | 0 Sombreado | 1 Geométrico (padrão) | 1 Geométrico | 1 Geométrico (padrão) |
| base_r/g/b | 0.72157/0.74118/0.78039 (padrão) | 0.72157/0.74118/0.78039 (padrão) | 1.0/0.1/0.6 (magenta) | 0.20/0.21/0.24 (ardósia escuro) |
| background_type | 1 Gradiente | 3 Nebulosa | 0 Sólido | 1 Gradiente |
| bg_color1_r/g/b | 0.06275/0.03922/0.03922 (padrão) | (n/a, nebulosa usa bg_neb_*) | 0.03/0.01/0.05 | 0.85/0.87/0.92 |
| bg_color2_r/g/b | 0.04706/0.07451/0.13725 (padrão) | (n/a) | (n/a, sólido) | 0.78/0.82/0.90 |
| bg_grad_angle | 101.0 (padrão) | (n/a) | (n/a) | 90.0 |
| bg_neb_color1/2 | (n/a) | 0.02745/0.01961/0.07843 e 0.24706/0.09804/0.34902 (padrão) | (n/a) | (n/a) |
| bloom_on/threshold/intensity/radius | 1 / 1.0 / 0.3 / 0.18 | 1 / 0.64 / 0.65 / 0.27 (padrão) | 1 / 0.4 / 1.3 / 0.4 | 1 / 1.6 / 0.12 / 0.12 |
| streaks_mode/intensity/length | 0 Desligado | 2 Anamórfico / 0.76 / 0.14 (padrão) | 1 Starburst / 1.2 / 0.2 | 0 Desligado |
| chroma_on/strength | 0 | 1 / 0.30 | 1 / 0.6 | 0 |
| vignette_on/amount | 0 | 1 / 0.35 (padrão) | 1 / 0.45 | 0 |
| fxaa_on | 1 | 1 | 1 | 1 |
| particles_on/kind | 0 | 1 / Bokeh | 1 / Faíscas | 1 / Poeira |
| particles_density/speed/size/opacity | (n/a, off) | 0.43/0.76/0.81/0.42 (padrão Bokeh) | 0.42/0.76/0.72/0.42 (padrão Faíscas) | 0.43/0.76/0.80/0.50 (padrão Poeira) |
| quality | 2 Alta (padrão) | 2 Alta (padrão) | 2 Alta (padrão) | 1 Média |

(campos "(n/a)" = o preset ainda define o valor pra consistência, mas
ele não tem efeito visível no modo escolhido — ex.: `metalness` é
definido mesmo em presets que usam material Clássico, que não lê esse
campo).

## 4. Presets salvos pelo usuário

Registro em `HKCU\Software\Modern3DText\Presets\<nome>`, mesmo
formato chave=valor da config principal, só os campos da seção 2
(reaproveita `config_save_to`/`config_load_from` numa subchave
dedicada — não precisa de um formato novo). Nome livre, até 64
caracteres, sem caracteres inválidos pra nome de subchave do registro
(`\`, e os que o Win32 já proíbe).

## 5. UI — rodapé fixo

Abaixo dos botões OK/Cancelar/Aplicar (que já são fixos, fora das
abas): um combo com os presets (4 fixos primeiro, depois os salvos,
em ordem alfabética) + 4 botões — **Salvar**, **Apagar**,
**Importar...**, **Exportar...**. "Apagar" fica desabilitado quando o
preset selecionado é um dos 4 fixos.

### Fluxos

- **Selecionar um preset no combo** → `CBN_SELCHANGE` dispara um
  `MessageBoxW` de confirmação ("Aplicar o preset '<nome>'? As
  alterações não salvas serão perdidas." Sim/Não). Não → o combo
  volta pra seleção anterior (sem aplicar nada). Sim → carrega os
  campos da seção 2 em `g_work`, chama o mecanismo de recarregar as
  9 abas (seção 6), `preview_dirty`.
- **Salvar** → abre um dialog pequeno novo (`IDD_PRESET_NAME`, 1 campo
  de texto + OK/Cancelar) pedindo o nome. Nome vazio → não faz nada.
  Nome de um dos 4 fixos, ou de um preset já salvo → confirma
  sobrescrever (Sim/Não); nome de um fixo especificamente **não pode
  ser sobrescrito** — mensagem explicando e pedindo outro nome. Grava
  só os campos da seção 2 (lidos de `g_work` no momento do clique) na
  subchave, atualiza o combo.
- **Apagar** → confirma (Sim/Não: "Apagar o preset '<nome>'? Essa ação
  não pode ser desfeita."), remove a subchave, atualiza o combo,
  seleciona o 1º item (Clássico).
- **Importar...** → `GetOpenFileNameW` filtro `*.ini`, lê o arquivo
  como texto, grava seu conteúdo bruto numa subchave temporária
  (`Presets\_import_tmp`), chama `config_load_from` nela (reaproveita
  toda sanitização/clamp já existente), apaga a subchave temporária,
  pede um nome (mesmo dialog do Salvar), grava como preset novo. Não
  aplica sozinho.
- **Exportar...** → `GetSaveFileNameW` filtro `*.ini`, grava os campos
  da seção 2 do preset selecionado (fixo ou salvo) em formato
  `[Preset]\nchave=valor\n...` (mesmo texto que uma subchave do
  registro teria, com um cabeçalho de seção pra ficar um `.ini`
  válido de verdade).

## 6. Recarregar as 9 abas a partir de `g_work`

Hoje cada aba só lê `g_work` uma vez, no próprio `WM_INITDIALOG`. Uma
nova mensagem `WM_RELOAD_FROM_CONFIG` (`WM_APP+2`) é postada pelo
dialog principal pra cada child (`g_content`, `g_motion`, ...,
`g_particles`) depois de aplicar um preset. Cada `*_proc` refatora a
lógica de setup do `WM_INITDIALOG` (sliders/radios/combos + labels)
pra uma função própria (ex.: `material_load_from_config(HWND h)`),
chamada tanto no `WM_INITDIALOG` quanto no novo
`WM_RELOAD_FROM_CONFIG` — sem duplicar código. Recarrega as 9 abas
sempre (mesmo as que um preset não toca, como Conteúdo/Movimento) -
mais simples e à prova de esquecimento do que rastrear seletivamente
quais abas um preset afeta.

## 7. Testes

- `test_config.c`: uma função por preset embutido conferindo os
  campos da tabela da seção 3; round-trip de salvar/carregar um
  preset numa subchave de teste.
- Captura real: aplicar cada um dos 4 presets (confirmando o dialog),
  conferindo visualmente Material/Fundo/Partículas mudando e
  Texto/Fonte permanecendo os mesmos; salvar um preset customizado,
  reabrir o dialog, confirmar que aparece no combo; apagar um preset
  salvo; exportar um preset e conferir o `.ini` gerado; importar esse
  mesmo `.ini` de volta com outro nome.

## 8. Fora de escopo

Editar um preset salvo (é apagar + salvar de novo); presets por
conteúdo (SVG/mesh específico); sincronizar presets entre máquinas.
