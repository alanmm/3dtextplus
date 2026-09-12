# Fase 6a — Modo Relógio — Documento de Design

## 1. Objetivo

Adicionar um segundo modo de conteúdo ao lado de `text`: `clock`, exibindo a
hora (e opcionalmente a data) atual, no formato localizado do Windows.
Primeira metade da Fase 6 do spec principal (§9); SVG fica para a 6b,
deliberadamente adiada — reaproveita zero código novo de geometria, ao
contrário de SVG.

## 2. Arquitetura

- `ContentMode` (`src/config.h`) ganha `CONTENT_CLOCK = 1` ao lado do
  `CONTENT_TEXT = 0` já existente.
- **Nenhum código novo de geometria.** O modo relógio só decide *de onde
  vem a string* — a partir daí é exatamente o mesmo `font_build_contours()`
  que o modo texto já usa (multilinha via `\n`, mesma fonte configurada).
- Nova função pura e testável em `src/scene.c`:
  `clock_format(SYSTEMTIME st, int show_date, int show_seconds, char *out, int outsz)`
  (UTF-8, usando `GetTimeFormatEx`/`GetDateFormatEx` com locale `NULL` =
  `LOCALE_NAME_USER_DEFAULT`). Recebe o `SYSTEMTIME` como parâmetro em vez
  de chamar `GetLocalTime()` internamente — é o que permite testar com um
  horário fixo, sem depender do relógio real da máquina no momento do
  teste.
- `scene_render()` (chamado todo frame), logo no início, quando
  `s->content_mode == CONTENT_CLOCK`: chama `GetLocalTime()`, formata via
  `clock_format()`, compara com `s->text` atual; só se mudou, copia e
  chama o `rebuild_mesh()` já existente — mesmo caminho que qualquer
  mudança de texto já usa hoje (SDF, cache de parede pra faíscas, tudo
  reaproveitado sem duplicação). Isso naturalmente reconstrói uma vez por
  minuto (sem segundos) ou uma vez por segundo (com segundos), sem
  precisar de um timer dedicado — só uma comparação de string barata a
  cada frame.
- `clockShowSeconds` usa a flag `TIME_NOSECONDS` do `GetTimeFormatEx`
  quando desligado (a maioria dos locales do Windows já inclui segundos
  no formato padrão de hora). `clockShowDate` prefixa a data formatada
  (`GetDateFormatEx`, formato padrão do locale) seguida de `\n` antes da
  hora — reaproveitando o suporte a multilinha que o texto já tem.
- **Sem toggle de 12h/24h explícito** — o Windows decide isso pelas
  configurações regionais do usuário, exatamente como o spec pede
  ("formato localizado do Windows").

## 3. Campos de configuração

```c
/* content_mode ja existe (ContentMode), so ganha um 2o valor valido */
int  clock_show_date;      /* 0/1 */
int  clock_show_seconds;   /* 0/1 */
```

Ambos default `0` (comportamento mínimo: só a hora, sem data/segundos) —
schema continua v2, mesmo padrão de "campo ausente usa o default".

## 4. Interface (aba "Conteúdo")

- Novo combo "Modo" (Texto/Relógio) no topo da aba, antes do campo de
  texto.
- Modo **Texto** (atual): campo de texto multilinha + combo de fonte
  visíveis, exatamente como hoje.
- Modo **Relógio**: campo de texto multilinha **fica desabilitado**
  (a fonte continua vindo de `font.family`/`bold`/
  `italic`, só o campo de *texto* deixa de fazer sentido); aparecem dois
  checkboxes novos: "Mostrar data" e "Mostrar segundos".
- Mesmo padrão de `*_enable()` já usado nas outras abas (ex.:
  `geometry_enable()`, `bg_enable()`).
- `scene_set_config()` passa a considerar mudança de `content_mode` (além
  da comparação de texto já existente) no cálculo de `mesh_dirty`, para
  garantir pelo menos uma reconstrução ao trocar de modo mesmo que
  `s->text` e `cfg->text` coincidam por acaso.

## 5. Preview ao vivo

Automático — o mini-preview já chama `scene_render()` a cada tick do
timer de 33ms, então o relógio atualiza sozinho no preview, exatamente
como no `/s` real.

## 6. Testes

- `test_config.c`: novos campos, defaults, round-trip, clamps (padrão já
  usado nas fases anteriores).
- **Novo teste unitário determinístico** para `clock_format()`: passa um
  `SYSTEMTIME` fixo (não usa o relógio real da máquina) e verifica:
  - `show_date=0` → uma linha só (sem `\n`), `show_date=1` → duas linhas
    (contém `\n`).
  - alternar `show_seconds` produz strings diferentes para o mesmo
    `SYSTEMTIME` (não dá pra travar o conteúdo exato, que depende do
    locale da máquina que roda o teste, mas dá pra garantir que o toggle
    tem efeito).
  - nunca retorna string vazia nem estoura o buffer de saída.
- Verificação visual: `M3DT_SHOT` do modo relógio no preview do diálogo
  (com e sem data/segundos), e uma checagem manual simples — duas
  capturas separadas por mais de 1 segundo devem mostrar segundos
  diferentes quando `clockShowSeconds` está ligado.

## 7. Fora de escopo desta fase

- SVG (Fase 6b, ordem escolhida pelo usuário).
- Malha 3D importada (`.obj`/`.glb`/`.gltf`/`.stl`) — fase própria mais à
  frente no spec principal.
- Fuso horário customizável / relógio de outro país — sempre a hora local
  da máquina, no formato do locale do usuário, como o spec pede.
