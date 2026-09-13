# Fase 8a — Internacionalização (PT/EN) — Documento de Design

## 1. Objetivo

Implementar a última peça pendente do design original (§10.5) que ainda
não existe: suporte PT/EN na interface de configuração, com
auto-detecção pelo idioma do Windows. Diferença em relação ao design
original: em vez de uma tabela única `const wchar_t* STR[LANG_COUNT][STR_COUNT]`
compilada dentro de `i18n.c`, cada idioma vive no seu próprio arquivo de
texto simples, editável sem tocar em código C.

## 2. Arquitetura

- **Chaves em C, valores em arquivo.** Um novo `src/i18n.h` declara
  `typedef enum { STR_TAB_CONTENT, ..., STR_COUNT } StrId;` — uma entrada
  por string visível na UI (~110-130 chaves, ver §4 Escopo). O código
  sempre referencia `i18n_str(STR_TAB_CONTENT)`, nunca uma string crua —
  erro de digitação vira erro de compilação, não um rótulo faltando em
  produção.
- **Arquivos:** `res/lang/pt.txt` e `res/lang/en.txt`, texto puro UTF-8,
  uma linha por chave: `nome.da.chave=Texto traduzido`. `#` no início da
  linha (após espaços) é comentário; linhas em branco são ignoradas. Um
  array privado `KEY_NAMES[STR_COUNT]` dentro de `src/i18n.c` (na MESMA
  ordem do enum) faz a ponte entre o enum e o nome usado no arquivo — é
  a única peça que amarra as duas coisas, então testada explicitamente
  (§6).
- **Escape dentro do valor:** a sequência de duas letras `\n` vira
  quebra de linha real (necessário pros avisos de erro multi-linha e
  pelo menos 2 dicas de texto longo já existentes no diálogo); `\\` vira
  uma barra invertida. Qualquer outra sequência `\X` é mantida literal
  (sem surpresa silenciosa em caso de erro de digitação).
- **Empacotamento:** os dois arquivos são embutidos no `.scr` no build
  via o `build/tools/embed.c` já existente (mesmo mecanismo dos
  shaders) — vira `EMBED_pt_txt`/`EMBED_en_txt` em
  `generated/embedded.h`. O `.scr` final continua um único arquivo
  portátil.
- **Fallback em 2 níveis:** chave ausente no inglês → cai pro
  português (fonte da verdade, texto atual portado 1:1). Ausente nos
  dois (não deveria acontecer, mas é defendido) → placeholder
  `[nome.da.chave]` + log de aviso, nunca crash nem caixa em branco.
- **Filtros de seletor de arquivo:** o formato nativo do `OPENFILENAMEW`
  intercala rótulo e padrão de extensão com `\0` (ex.:
  `L"SVG\0*.svg\0Todos\0*.*\0"`), que não cabe limpo numa única chave de
  texto. Só o RÓTULO de cada entrada vira chave traduzível (ex.:
  `filter.svg`, `filter.images`, `filter.all_short`); o padrão de
  extensão (`*.svg`, `*.obj;*.stl`, etc.) fica fixo em C, já que
  wildcard de arquivo não é texto de idioma. Um pequeno helper
  `filter_append()` monta a string final concatenando
  `i18n_str(...)` + padrão fixo + `\0`, um par por vez.
- **Aba Desempenho ganha um dropdown "Idioma"** (Automático/Português/
  Inglês) — não existe hoje nenhuma aba "geral", e essa aba já reúne
  ajustes globais do motor (FPS, VSync, MSAA, qualidade automática).
  Config ganha `ui_language` (`0=auto, 1=pt, 2=en`, padrão `auto`).
  Auto: `GetUserDefaultUILanguage() & 0x3FF == LANG_PORTUGUESE` → PT,
  senão EN. Trocar o dropdown chama `i18n_init()` de novo e força um
  refresh de todos os textos visíveis do diálogo (sem precisar
  reabrir).
- **`scene.c` também consome `i18n_str()`** pras 3 mensagens da placa de
  erro (Fase 7d) — não fica de fora só por não ser `config_dialog.c`.

## 3. Onde o texto reaproveitado do `.rc` continua

O `.rc` mantém o texto em português tal como está hoje em cada
`LTEXT`/`AUTOCHECKBOX`/`PUSHBUTTON`/`GROUPBOX` — funciona como um
fallback de compilação: se por algum motivo `i18n_init()` falhar, o
diálogo ainda aparece 100% legível em português (nunca em branco). O
código sobrescreve com `SetWindowTextW`/`SetDlgItemTextW` durante
`WM_INITDIALOG` de cada aba (e nos `CB_ADDSTRING` que já existem pra
popular os dropdowns) sempre que o idioma ativo não é português, ou
sempre que o idioma muda em tempo real via o novo dropdown.

## 4. Escopo (o que vira chave)

Todas as ~9 abas do diálogo (rótulos de campo, checkboxes, botões,
itens de cada dropdown, as 2 dicas de texto longo em Efeitos/Pós/
Desempenho), os textos dinâmicos de espaço reservado
(`content_svg_label`/`content_mesh_label`/`material_labels`/`bg_labels`
— "(nenhum)"/"(nenhuma)"/"(procedural)"), os rótulos dos 4 filtros de
seletor de arquivo (SVG/malha/imagem de ambiente/imagem de fundo), o
texto do aviso de arquivo grande (`MessageBoxW`, Fase 7d), e as 3
mensagens da placa de erro (`scene.c`, Fase 7d). Os rótulos numéricos
formatados (`%.2f`, `%d%%`) não entram — não são texto de idioma. Nomes
de fonte (`EnumFontFamiliesExW`) também não entram — vêm do sistema.

**Decisão de granularidade:** cada controle do diálogo ganha SUA
PRÓPRIA chave, mesmo quando o texto atual coincide com outro controle
(ex.: "Desligado" aparece em Bevel, Streaks e MSAA — 3 chaves
distintas, não uma compartilhada). Só os botões de ação puramente
genéricos (Escolher.../Limpar/Escolher cor...) e os 2 placeholders
reaproveitados literalmente no mesmo papel (Arquivo:/"(nenhum)" pra
SVG e malha) compartilham chave. Motivo: o pedido original foi
"poder fazer pequenos ajustes" — chaves independentes evitam que
editar uma reescrita mude outro controle sem querer.

## 5. Campos de configuração

Um campo novo: `ui_language` (int, `0=auto/1=pt/2=en`, padrão `0`).
Clamp de faixa no load segue o mesmo padrão já usado pros outros enums
do `Config` (fora da faixa → volta pro padrão).

## 6. Testes

- **Unitário** (`build/tests/test_i18n.c`, novo): parse de um `.txt`
  de teste embutido inline (não os arquivos reais) cobrindo comentário,
  linha em branco, `\n`/`\\` escapados, chave duplicada (última
  vence), chave desconhecida (ignorada com log, não derruba o parse).
  Um teste separado e explícito varre o `KEY_NAMES[]` real de
  `i18n.c` contra os dois arquivos REAIS embutidos
  (`res/lang/pt.txt`/`res/lang/en.txt`) e falha se qualquer chave do
  enum estiver faltando em QUALQUER um dos dois — é o que garante que
  os dois arquivos nunca ficam dessincronizados do enum (nem um do
  outro) conforme novas chaves forem adicionadas no futuro.
- **Visual real**: como o `M3DT_SHOT` só captura o painel 3D (não o
  diálogo), a verificação de que o texto realmente aparece traduzido
  precisa de uma captura de tela real da janela do diálogo (mesma
  técnica `.NET Graphics.CopyFromScreen` já usada na Fase 7b pra
  verificar layout) — comparar a mesma aba em PT e em EN.

## 7. Fora de escopo

- Mais idiomas além de PT/EN (a arquitetura já permite adicionar um
  `xx.txt` novo depois, mas não faz parte desta fase).
- Traduzir o `CAPTION` da janela ("Modern 3D Text") ou nomes de
  produto/marca.
- Editor de tradução na própria UI (o arquivo de texto já cobre o
  caso de ajuste manual).
- Unificar chaves com texto hoje coincidente além do já decidido em
  §4 (decisão deliberada, não uma limitação técnica).
