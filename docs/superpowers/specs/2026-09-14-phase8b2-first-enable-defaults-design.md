# Fase 8b-2 — Valor Padrão ao Ligar um Recurso — Documento de Design

## 1. Objetivo

Segunda de 7 sub-fases do trabalho de "presets e valores padrão"
(brainstorming de 2026-09-13/14). Quando o usuário liga um recurso
opcional que nunca foi ajustado manualmente, os sub-parâmetros daquele
recurso recebem um valor padrão sensato em vez de continuarem com
qualquer valor residual que estivesse nos controles — sem nunca
sobrescrever um ajuste manual já feito antes.

## 2. Arquitetura

- **6 campos novos na `Config`** (`int`, `0/1`, padrão `0`,
  persistidos no registro como qualquer outro booleano):
  `bloom_customized`, `chroma_customized`, `vignette_customized`,
  `particles_customized`, `shell_customized`, `streaks_customized`.
- **Regra de "customizado"**: mexer manualmente num slider do recurso
  marca o campo como `1` — checado com precisão via `(HWND)l` dentro
  do `WM_HSCROLL` de cada aba, comparando contra o `HWND` exato daquele
  slider (não "qualquer slider da aba", já que várias abas hospedam
  sliders de mais de um recurso ao mesmo tempo — ex.: Efeitos tem
  Bloom e Streaks, Pós tem Aberração e Vinheta). Escolher um "Tipo" de
  partícula específico (combo, não slider) também conta como
  customizar Partículas.
- **Regra de "aplicar padrão"**: no exato momento em que o checkbox
  liga (ou, pra Streaks, o modo sai de "Desligado" pra Starburst/
  Anamórfico) **e** o campo `customized` correspondente ainda é `0`,
  os sub-parâmetros daquele recurso recebem os valores de
  `config_defaults()` (os mesmos definidos na Fase 8b-1) — a posição
  visual do(s) slider(s) e o rótulo numérico são atualizados na hora,
  igual a uma troca normal de valor. O campo `customized` continua
  `0` depois disso (só vira `1` quando o usuário de fato arrasta um
  slider) — então religar o mesmo recurso de novo, sem tocar em nada,
  reaplica o mesmo padrão de novo, de forma idempotente.
- **Já customizado** (`customized == 1`): ligar o recurso não mexe em
  nenhum sub-parâmetro, mantém o que o usuário deixou da última vez.

## 3. Mapeamento recurso → controles

| Recurso | Checkbox/gatilho | Sliders (marcam customizado) | Sub-campos aplicados no 1º ligar |
|---|---|---|---|
| Bloom | `IDC_BLOOM` | `IDC_BTHRESH`, `IDC_BINT`, `IDC_BRAD` | `bloom_threshold`, `bloom_intensity`, `bloom_radius` |
| Streaks | `IDC_STREAKMODE` (Desligado→outro) | `IDC_SINT`, `IDC_SLEN` | `streaks_intensity`, `streaks_length` |
| Aberração cromática | `IDC_CHROMA` | `IDC_CSTR` | `chroma_strength` |
| Vinheta | `IDC_VIGNETTE` | `IDC_VAMT` | `vignette_amount` |
| Partículas | `IDC_PARTON` | `IDC_PARTDENS/SPEED/SIZE/OPACITY` + combo `IDC_PARTKIND` | `particles_density/speed/size_scale/opacity` |
| Casca oca | `IDC_SHELL` | `IDC_WALL` | `wall_thickness` |

## 4. Campos de configuração

6 campos novos, listados em §2. Clamp de sanitização: qualquer valor
fora de `{0,1}` vira `0` (mesmo padrão já usado pros outros booleanos
da `Config`).

## 5. Testes

Round-trip + clamp dos 6 campos novos em `test_config.c` (mesmo padrão
já usado pros outros booleanos). A lógica de "aplicar padrão ao ligar"
vive em `config_dialog.c` (WinAPI, sem cobertura de teste automatizado
headless, mesmo padrão já estabelecido pro resto do diálogo) —
verificação via captura real: ligar Casca oca do zero (deve assumir
`wall_thickness=0.015`), mexer no slider, desligar, religar (deve
manter o valor mexido, não voltar pro padrão).

## 6. Fora de escopo

Ocultar/reorganizar controles por modo (Material/Fundo — fases 3 e 4),
padrão por tipo de partícula (fase 5, mecanismo parecido mas com 4
variantes em vez de 1), mapa de ambiente embutido (fase 6), sistema de
presets (fase 7).
