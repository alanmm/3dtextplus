# Fase 8b-6 — Imagem de Ambiente Embutida — Documento de Design

## 1. Objetivo

Sexta de 7 sub-fases. Hoje "Imagem de ambiente" só tem 2 estados
implícitos (caminho vazio = ambiente procedural; caminho preenchido =
imagem escolhida pelo usuário). Passa a ter 3 estados explícitos, com
uma imagem de qualidade embutida no `.scr` como padrão de fábrica:

- **Embutida** (padrão): usa uma imagem CC0 já embutida no binário.
- **Personalizada**: usa um arquivo de imagem escolhido pelo usuário
  (comportamento de hoje).
- **Nenhuma**: sem imagem real, ambiente procedural (o que hoje
  acontece com o caminho vazio).

## 2. Imagem escolhida

`boma_1k.jpg` (Poly Haven, CC0, 264 KB) — confirmado pelo usuário como
baixado diretamente de polyhaven.com. Copiado (não movido) pra um
local permanente e commitável: **`res/env_default.jpg`**.
`src/hdri/` continua sendo a pasta de candidatos em revisão do
usuário, fora do git — não é afetada por esta fase.

## 3. Novo campo `Config`

```c
int env_mode;   /* 0 embutida (padrao), 1 personalizada, 2 nenhuma */
```

`env_path` continua existindo sem mudança de formato, mas só é
*usado* pelo renderer quando `env_mode == 1`.

**Migração**: se o registro tem um `env_path` não-vazio mas NÃO tem a
chave `env_mode` (config salvo antes desta fase), assume-se
`env_mode = 1` no load — senão a imagem personalizada de alguém
"sumiria" silenciosamente na atualização. Sem usuários reais ainda,
mas é uma garantia barata.

## 4. Embutindo a imagem

`build/tools/embed.c` já grava qualquer arquivo como array de bytes
(binário ou texto, sem diferença) — só precisa entrar na lista de
arquivos passada ao `embed` no `build/Makefile`, junto dos shaders e
dos `.txt` de idioma. Gera `EMBED_env_default_jpg[]` +
`EMBED_env_default_jpg_len` em `generated/embedded.h`.

`env.c` ganha uma função nova, paralela à existente:

```c
unsigned env_load_texture_from_memory(const unsigned char *data, int len);
```

(usa `stbi_load_from_memory` em vez de `stbi_load` — mesmo resto do
pipeline: RGB8, `gl_texture_2d_rgb8`, log). `env.h` declara a nova
função ao lado da existente `env_load_texture(const wchar_t *path)`.

`scene.c` (que já inclui `embedded.h` pros outros assets) passa a
decidir com base em `cfg->env_mode` em vez de só olhar `env_path`:

```c
if (cfg->env_mode != s->env_mode ||
    (cfg->env_mode == 1 && wcscmp(s->env_path, cfg->env_path) != 0)) {
    env_free(s->env_tex);
    s->env_mode = cfg->env_mode;
    wcsncpy(s->env_path, cfg->env_path, 511); s->env_path[511] = 0;
    switch (cfg->env_mode) {
        case 0:  s->env_tex = env_load_texture_from_memory(EMBED_env_default_jpg, EMBED_env_default_jpg_len); break;
        case 1:  s->env_tex = env_load_texture(s->env_path); break;
        default: s->env_tex = 0; break;   /* 2 = nenhuma -> procedural */
    }
}
```

`SceneRenderer` ganha o campo `int env_mode;` ao lado do já existente
`env_path`/`env_tex`.

## 5. UI (aba Material)

O bloco "Imagem de ambiente" (já oculto em Clássico/Fosco desde a
Fase 8b-3) se divide em **2 blocos** dentro do mecanismo de
captura/reflow já existente (`g_mat_blocks`), sem mudar a mecânica —
só cresce de 3 pra 4 blocos:

- **Bloco 3a** (visível sempre que Metálico/Vidro): label "Imagem de
  ambiente:" (tira o "(opcional)" do texto — não é mais opcional, é
  uma escolha de 3) + 3 radio buttons novos, nesta ordem: **Embutida**
  | **Personalizada** | **Nenhuma**. IDs novos:
  `IDC_ENVMODE_EMBED` (1312), `IDC_ENVMODE_CUSTOM` (1313),
  `IDC_ENVMODE_NONE` (1314) - `BS_AUTORADIOBUTTON`, o primeiro com
  `WS_GROUP`.
- **Bloco 3b** (visível só quando Metálico/Vidro **E**
  `env_mode == 1`): os 3 controles que já existem — caminho
  (`IDC_ENVPATH`), "Escolher..." (`IDC_ENVPICK`) e "Limpar"
  (`IDC_ENVCLEAR`) — ficam **ocultos** (não só desabilitados) fora de
  Personalizada, exatamente como pedido.

Layout no `.rc`: label na linha `y=116` (como hoje), radios numa nova
linha `y=128`, caminho desce pra `y=142`, Escolher/Limpar descem pra
`y=154` — cabe dentro da altura atual do template (200).

**Comportamento dos radios** (`material_proc`'s `WM_COMMAND`):

- **Embutida**: `g_work.env_mode = 0`. `env_path` não é tocado (fica
  guardado, caso o usuário volte pra Personalizada depois).
- **Personalizada**: `g_work.env_mode = 1`. Mesmo comportamento de
  hoje pro caminho (se estiver vazio, mostra o placeholder
  "(procedural)" — `material_labels()` já faz isso).
- **Nenhuma**: `g_work.env_mode = 2` **e limpa `g_work.env_path[0] = 0`**
  (pedido explícito do usuário) — a renderização fica sem imagem
  real, cai no ambiente procedural.

Todos os 3 chamam `material_layout_apply(h)` (reflow do bloco 3b) +
`material_labels(h)` (placeholder do caminho) + `preview_dirty(h)`.

`IDC_ENVPICK`/`IDC_ENVCLEAR` mantêm o comportamento atual sem mudança
(só ficam inacessíveis fora de Personalizada, já que o bloco 3b fica
oculto).

## 6. i18n

Novas chaves (`pt.txt`/`en.txt`): `material.env_mode.embedded`,
`material.env_mode.custom`, `material.env_mode.none`. Sugestão:
PT "Embutida" / "Personalizada" / "Nenhuma", EN "Embedded" /
"Custom" / "None". `material.env_label` perde o "(opcional)" do
texto.

## 7. Testes

- `test_config.c`: round-trip + clamp (`0..2`, fora da faixa -> 0) de
  `env_mode`; caso de migração (env_path não-vazio + chave env_mode
  ausente no registro -> carrega como `1`).
- Captura real: cada um dos 3 radios em Metálico, conferindo (a) o
  bloco 3b aparece só em Personalizada, (b) a imagem embutida reflete
  visivelmente diferente da procedural em Embutida, (c) trocar pra
  Nenhuma limpa o caminho mostrado E volta a aparência procedural,
  (d) trocar pra Personalizada com Escolher aponta um arquivo local,
  volta pra Embutida (path continua guardado, some da tela), volta
  pra Personalizada (path reaparece).

## 8. Fora de escopo

Presets (fase 7). Comprimir/trocar a imagem embutida além do que já
está (264 KB, 1k). Um CREDITS/atribuição formal (CC0 não exige, mas
deixo o nome de origem "Poly Haven - boma" registrado aqui no spec
pra referência futura).
