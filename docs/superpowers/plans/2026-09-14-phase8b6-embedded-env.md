# Fase 8b-6 — Imagem de Ambiente Embutida — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Substituir o binário "caminho vazio = procedural" por 3
modos explícitos de ambiente (Embutida/Personalizada/Nenhuma) na aba
Material, com uma imagem CC0 (`res/env_default.jpg`, Poly Haven -
`boma_1k`) embutida no `.scr` como padrão de fábrica.

**Architecture:** novo campo `Config.env_mode` (0/1/2); a imagem
embutida entra no mecanismo `embed.c` já existente (mesma lista dos
shaders/idiomas); `env.c` ganha uma variante "carrega da memória" da
função que já existe; `scene.c` decide qual das 3 fontes carregar; a
aba Material ganha 3 radio buttons que substituem a antiga leitura
implícita de `env_path` vazio/preenchido.

**Tech Stack:** C (C99), Win32 (registro, dialog resources, radio
buttons), OpenGL 3.3, `stb_image` (já em uso), `build/Makefile`.

## Global Constraints

- Todo campo novo segue o padrão já usado: default em
  `config_defaults()`, `reg_get_i`/`reg_get_w` em `config_load_from()`,
  clamp em sanitize, `set_f`/`set_w` em `config_save_to()`.
- `mingw32-make -f build/Makefile clean` obrigatório após editar
  qualquer `.h` (sem rastreio de dependência de header).
- Copiar `dist/Modern3DText.scr` -> `dist/Modern3DText.exe` após todo
  build real, antes de testar.
- Captura de tela real exige aviso prévio (PushNotification, senão
  chat + `AskUserQuestion` aguardando confirmação) antes de qualquer
  `/s`/`/c`.
- Registro `HKCU\Software\Modern3DText`: checar/backupar antes de
  limpar pra teste de instalação limpa.

---

### Task 1: Campo `Config.env_mode` (com migração)

**Files:**
- Modify: `src/config.h:22` (struct `Config`, logo antes de
  `env_path`)
- Modify: `src/config.c:30` (`config_defaults`)
- Modify: `src/config.c:165` (`config_load_from` - leitura + migração)
- Modify: `src/config.c:271` (sanitize, dentro de `config_load_from`)
- Modify: `src/config.c:373` (`config_save_to`)
- Test: `build/tests/test_config.c`

**Interfaces:**
- Produces: `int env_mode` em `Config` (0 embutida, 1 personalizada,
  2 nenhuma), consumido pela Task 3 (`scene.c`) e Task 4
  (`config_dialog.c`).

- [ ] **Step 1: Adicionar o campo ao struct `Config`**

Em `src/config.h`, logo antes de `wchar_t env_path[512];` (linha 22):

```c
    int          env_mode;             /* 0 embutida, 1 personalizada, 2 nenhuma */
```

- [ ] **Step 2: Default em `config_defaults()`**

Em `src/config.c`, logo antes de `c->env_path[0] = 0;` (linha 30):

```c
    c->env_mode = 0;
```

- [ ] **Step 3: Leitura + migração em `config_load_from()`**

Em `src/config.c`, troque a linha 165
(`reg_get_w(k, L"env_path", c->env_path, 512);`) por:

```c
    reg_get_w(k, L"env_path", c->env_path, 512);
    int had_env_mode = reg_get_i(k, L"env_mode", &c->env_mode);
    if (!had_env_mode && c->env_path[0])
        c->env_mode = 1;   /* config salvo antes desta fase, com imagem propria -> preserva */
```

- [ ] **Step 4: Clamp em sanitize**

Em `src/config.c`, logo após
`c->roughness = clampf(c->roughness, 0.0f, 1.0f);` (linha 271):

```c
    if (c->env_mode < 0 || c->env_mode > 2) c->env_mode = 0;
```

- [ ] **Step 5: Salvar em `config_save_to()`**

Em `src/config.c`, logo antes de `set_w(k, L"env_path", c->env_path);`
(linha 373):

```c
    set_f(k, L"env_mode", (float)c->env_mode);
```

- [ ] **Step 6: Testes em `test_config.c`**

No bloco de defaults (perto de onde outros campos default são
checados), adicionar:

```c
    EXPECT(d.env_mode == 0);
```

No round-trip (mesmo bloco de `a.env_path`/`b.env_path`, por volta
das linhas 102/167), adicionar:

```c
    a.env_mode = 1;
```
e
```c
    EXPECT(b.env_mode == 1);
```

No array `kv[]` do teste de clamp, adicionar `{ L"env_mode", L"9" }`,
atualizar o `for (int i = 0; i < N; ++i)` pro novo total, e adicionar:

```c
    EXPECT(e.env_mode == 0);   /* 9 -> fora de 0..2 -> 0 */
```

Novo teste de migração (função própria ou bloco dentro de uma
existente) - escreve `env_path` no registro de teste SEM escrever
`env_mode`, carrega, confirma `env_mode == 1`:

```c
static void test_env_mode_migration(void) {
    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);
    HKEY kp;
    RegCreateKeyExW(HKEY_CURRENT_USER, TESTKEY, 0, NULL, 0, KEY_WRITE, NULL, &kp, NULL);
    const wchar_t *path = L"C:\\img\\studio.jpg";
    RegSetValueExW(kp, L"env_path", 0, REG_SZ, (const BYTE *)path,
                   (DWORD)((wcslen(path) + 1) * sizeof(wchar_t)));
    RegCloseKey(kp);

    Config c;
    config_load_from(&c, TESTKEY);
    EXPECT(c.env_mode == 1);   /* env_path presente, env_mode ausente -> migra pra "personalizada" */
    EXPECT(wcscmp(c.env_path, path) == 0);

    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);
}
```

Chamar `test_env_mode_migration()` de onde as outras funções de teste
de `test_config.c` já são chamadas (mesmo padrão do arquivo).

- [ ] **Step 7: Build limpo e testes**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```

Expected: build sem warnings, todos os testes passando.

- [ ] **Step 8: Commit**

```bash
git add src/config.h src/config.c build/tests/test_config.c
git commit -m "$(cat <<'EOF'
feat: add env_mode Config field with legacy env_path migration

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Embutir a imagem + carregar da memória

**Files:**
- Create: `res/env_default.jpg` (copiado de
  `src/hdri/boma_1k.jpg`, CC0 Poly Haven)
- Modify: `build/Makefile:53` (`EMBED_INPUTS`)
- Modify: `src/env.h`
- Modify: `src/env.c`

**Interfaces:**
- Consumes: `generated/embedded.h`'s `EMBED_env_default_jpg[]` /
  `EMBED_env_default_jpg_len` (gerados automaticamente pelo `embed.c`
  já existente a partir do novo arquivo).
- Produces: `unsigned env_load_texture_from_memory(const unsigned
  char *data, int len)`, consumida pela Task 3 (`scene.c`).

- [ ] **Step 1: Copiar a imagem pro local permanente**

```bash
cp "src/hdri/boma_1k.jpg" "res/env_default.jpg"
```

- [ ] **Step 2: Adicionar ao `EMBED_INPUTS` do Makefile**

Em `build/Makefile`, linha 53, troque:

```makefile
                res/lang/pt.txt res/lang/en.txt
```

por:

```makefile
                res/lang/pt.txt res/lang/en.txt res/env_default.jpg
```

- [ ] **Step 3: Declarar `env_load_texture_from_memory` em `env.h`**

Em `src/env.h`, logo após a declaração de `env_load_texture`:

```c
/* Carrega uma imagem (equiretangular) de um buffer ja em memoria
   (ex.: dado embutido no binario) como textura GL 2D. Retorna o id
   da textura, ou 0 se falha. Requer contexto GL. */
unsigned env_load_texture_from_memory(const unsigned char *data, int len);
```

- [ ] **Step 4: Implementar em `env.c`**

Em `src/env.c`, logo após `env_load_texture`:

```c
unsigned env_load_texture_from_memory(const unsigned char *data, int len)
{
    if (!data || len <= 0) return 0;

    int w = 0, h = 0, ch = 0;
    unsigned char *px = stbi_load_from_memory(data, len, &w, &h, &ch, 3);
    if (!px) {
        log_errorf("env: nao carregou imagem embutida (%s)", stbi_failure_reason());
        return 0;
    }

    unsigned t = gl_texture_2d_rgb8(w, h, px, 1);
    stbi_image_free(px);
    log_infof("env: imagem embutida (%dx%d) -> tex %u", w, h, t);
    return t;
}
```

- [ ] **Step 5: Build limpo (a nova imagem precisa entrar no embed)**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```

Expected: build sem warnings (a função nova ainda nao e chamada por
ninguem nesta task - `-Wunused-function` NAO dispara pra funcoes
`extern`/nao-`static`, entao isso e esperado compilar limpo mesmo sem
uso ainda).

- [ ] **Step 6: Commit**

```bash
git add res/env_default.jpg build/Makefile src/env.h src/env.c
git commit -m "$(cat <<'EOF'
feat: embed default CC0 environment image (Poly Haven boma_1k)

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: `scene.c` - decidir a fonte do ambiente por `env_mode`

**Files:**
- Modify: `src/scene.c:46-47` (struct `SceneRenderer`)
- Modify: `src/scene.c:555-560` (`scene_set_config`)

**Interfaces:**
- Consumes: `Config.env_mode` (Task 1),
  `env_load_texture_from_memory` (Task 2), `EMBED_env_default_jpg[]`/
  `EMBED_env_default_jpg_len` (gerado, Task 2 - `scene.c` já inclui
  `embedded.h` na linha 22).
- Produces: `s->env_mode`, `s->env_loaded` (novos campos internos, não
  expostos fora de `scene.c`).

- [ ] **Step 1: Novos campos em `SceneRenderer`**

Em `src/scene.c`, logo após `unsigned env_tex;` (linha 47):

```c
    int      env_mode;
    int      env_loaded;   /* forca 1a carga mesmo quando env_mode==0 bate com o calloc inicial */
```

- [ ] **Step 2: Trocar a lógica de carga em `scene_set_config`**

Troque o bloco (linhas 555-560):

```c
    if (wcscmp(s->env_path, cfg->env_path) != 0) {
        env_free(s->env_tex);
        wcsncpy(s->env_path, cfg->env_path, 511);
        s->env_path[511] = 0;
        s->env_tex = env_load_texture(s->env_path);
    }
```

por:

```c
    if (!s->env_loaded || s->env_mode != cfg->env_mode ||
        (cfg->env_mode == 1 && wcscmp(s->env_path, cfg->env_path) != 0)) {
        env_free(s->env_tex);
        s->env_mode = cfg->env_mode;
        wcsncpy(s->env_path, cfg->env_path, 511);
        s->env_path[511] = 0;
        s->env_loaded = 1;
        switch (cfg->env_mode) {
            case 0:  s->env_tex = env_load_texture_from_memory(EMBED_env_default_jpg, EMBED_env_default_jpg_len); break;
            case 1:  s->env_tex = env_load_texture(s->env_path); break;
            default: s->env_tex = 0; break;   /* 2 = nenhuma -> ambiente procedural */
        }
    }
```

**Nota do porque do `env_loaded`**: `SceneRenderer` nasce via
`calloc` (`scene_create`), entao `s->env_mode` comeca em `0` - o
MESMO valor do default de `cfg->env_mode` (Embutida). Sem o flag
`env_loaded`, a condicao `s->env_mode != cfg->env_mode` seria falsa
logo na 1a chamada (0 != 0), e a textura embutida NUNCA seria
carregada num config recem-criado - o mesmo tipo de armadilha de
"valor calloc'd coincide com o default real" ja visto na correcao da
Fase 8b-5.

- [ ] **Step 3: Build limpo**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile release
```

Expected: build sem warnings novos (o warning pre-existente de
`scene.c:533` sobre `strncpy` e' de uma linha diferente, nao
relacionado a esta mudanca).

- [ ] **Step 4: Commit**

```bash
git add src/scene.c
git commit -m "$(cat <<'EOF'
feat: scene.c loads env texture from embedded/file/none by env_mode

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: UI da aba Material (radios + reflow) + i18n + verificação

**Files:**
- Modify: `src/resource.h` (novos IDs)
- Modify: `res/screensaver.rc:71-87` (`IDD_TAB_MATERIAL`)
- Modify: `src/i18n.h` (novas `StrId`)
- Modify: `src/i18n.c` (novas chaves, mesma ordem)
- Modify: `res/lang/pt.txt`, `res/lang/en.txt`
- Modify: `src/config_dialog.c` (bloco Material: layout + handlers)

**Interfaces:**
- Consumes: `Config.env_mode` (Task 1), o mecanismo de reflow já
  existente `g_mat_blocks`/`material_layout_capture`/
  `material_layout_apply` (`config_dialog.c`, ~linhas 530-597).
- Produces: nada consumido por outra task - última do plano.

- [ ] **Step 1: Novos IDs de recurso**

Em `src/resource.h`, logo após `#define IDC_ENV_LABEL      1311`:

```c
#define IDC_ENVMODE_EMBED  1312
#define IDC_ENVMODE_CUSTOM 1313
#define IDC_ENVMODE_NONE   1314
```

- [ ] **Step 2: Editar o template `IDD_TAB_MATERIAL` no `.rc`**

Em `res/screensaver.rc`, troque as linhas 83-86:

```
    LTEXT      "Imagem de ambiente (opcional):", IDC_ENV_LABEL, 8, 116, 200, 9
    LTEXT      "(procedural)", IDC_ENVPATH, 8, 128, 224, 9, SS_PATHELLIPSIS
    PUSHBUTTON "Escolher...", IDC_ENVPICK, 8, 140, 60, 14
    PUSHBUTTON "Limpar",      IDC_ENVCLEAR, 72, 140, 50, 14
```

por:

```
    LTEXT      "Imagem de ambiente:", IDC_ENV_LABEL, 8, 116, 200, 9
    CONTROL    "Embutida", IDC_ENVMODE_EMBED, "Button", BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 8, 128, 62, 10
    CONTROL    "Personalizada", IDC_ENVMODE_CUSTOM, "Button", BS_AUTORADIOBUTTON | WS_TABSTOP, 74, 128, 78, 10
    CONTROL    "Nenhuma", IDC_ENVMODE_NONE, "Button", BS_AUTORADIOBUTTON | WS_TABSTOP, 156, 128, 60, 10
    LTEXT      "(procedural)", IDC_ENVPATH, 8, 142, 224, 9, SS_PATHELLIPSIS
    PUSHBUTTON "Escolher...", IDC_ENVPICK, 8, 154, 60, 14
    PUSHBUTTON "Limpar",      IDC_ENVCLEAR, 72, 154, 50, 14
```

- [ ] **Step 3: Novas `StrId` em `i18n.h`**

Em `src/i18n.h`, logo após `STR_MATERIAL_ENV_LABEL,`:

```c
    STR_MATERIAL_ENV_MODE_EMBEDDED, STR_MATERIAL_ENV_MODE_CUSTOM, STR_MATERIAL_ENV_MODE_NONE,
```

- [ ] **Step 4: Chaves correspondentes em `i18n.c`**

Em `src/i18n.c`, logo após `"material.env_label",` (mesma posição
relativa do Step 3, pra manter os dois arrays alinhados):

```c
    "material.env_mode.embedded", "material.env_mode.custom", "material.env_mode.none",
```

- [ ] **Step 5: Strings em `res/lang/pt.txt`**

Logo após a linha `material.env_label=...`, trocar essa linha e
adicionar as novas:

```
material.env_label=Imagem de ambiente:
material.env_mode.embedded=Embutida
material.env_mode.custom=Personalizada
material.env_mode.none=Nenhuma
```

- [ ] **Step 6: Strings em `res/lang/en.txt`**

`res/lang/en.txt:55` hoje é
`material.env_label=Environment image (optional):`. Troque essa linha
e adicione as novas logo depois:

```
material.env_label=Environment image:
material.env_mode.embedded=Embedded
material.env_mode.custom=Custom
material.env_mode.none=None
```

- [ ] **Step 7: Crescer `g_mat_blocks` de 3 pra 4 blocos**

Em `src/config_dialog.c`, troque (linhas 530-541):

```c
typedef struct { int id; int x, rel_y; } MatCtrl;

static MatCtrl g_mat_blocks[3][4] = {
    { { IDC_METAL_LABEL, 0, 0 }, { IDC_METAL_VAL, 0, 0 }, { IDC_METAL, 0, 0 } },
    { { IDC_ROUGH_LABEL, 0, 0 }, { IDC_ROUGH_VAL, 0, 0 }, { IDC_ROUGH, 0, 0 } },
    { { IDC_ENV_LABEL, 0, 0 }, { IDC_ENVPATH, 0, 0 }, { IDC_ENVPICK, 0, 0 }, { IDC_ENVCLEAR, 0, 0 } },
};
static const int MAT_BLOCK_N[3] = { 3, 3, 4 };
static int g_mat_block_top[3];
static int g_mat_block_h[3];
static int g_mat_gap_after[2];
static int g_mat_layout_ready = 0;
```

por:

```c
typedef struct { int id; int x, rel_y; } MatCtrl;

static MatCtrl g_mat_blocks[4][4] = {
    { { IDC_METAL_LABEL, 0, 0 }, { IDC_METAL_VAL, 0, 0 }, { IDC_METAL, 0, 0 } },
    { { IDC_ROUGH_LABEL, 0, 0 }, { IDC_ROUGH_VAL, 0, 0 }, { IDC_ROUGH, 0, 0 } },
    { { IDC_ENV_LABEL, 0, 0 }, { IDC_ENVMODE_EMBED, 0, 0 }, { IDC_ENVMODE_CUSTOM, 0, 0 }, { IDC_ENVMODE_NONE, 0, 0 } },
    { { IDC_ENVPATH, 0, 0 }, { IDC_ENVPICK, 0, 0 }, { IDC_ENVCLEAR, 0, 0 } },
};
static const int MAT_BLOCK_N[4] = { 3, 3, 4, 3 };
static int g_mat_block_top[4];
static int g_mat_block_h[4];
static int g_mat_gap_after[3];
static int g_mat_layout_ready = 0;
```

- [ ] **Step 8: Atualizar `material_layout_capture` pro novo tamanho**

Em `src/config_dialog.c`, dentro de `material_layout_capture`, troque
todo `for (int b = 0; b < 3; ++b)` por `for (int b = 0; b < 4; ++b)`
(2 ocorrências: o loop principal e o loop que calcula `rel_y`), e
troque:

```c
    g_mat_gap_after[0] = g_mat_block_top[1] - (g_mat_block_top[0] + g_mat_block_h[0]);
    g_mat_gap_after[1] = g_mat_block_top[2] - (g_mat_block_top[1] + g_mat_block_h[1]);
```

por:

```c
    g_mat_gap_after[0] = g_mat_block_top[1] - (g_mat_block_top[0] + g_mat_block_h[0]);
    g_mat_gap_after[1] = g_mat_block_top[2] - (g_mat_block_top[1] + g_mat_block_h[1]);
    g_mat_gap_after[2] = g_mat_block_top[3] - (g_mat_block_top[2] + g_mat_block_h[2]);
```

- [ ] **Step 9: Atualizar `material_layout_apply` com a 4a visibilidade**

Em `src/config_dialog.c`, troque `material_layout_apply` inteira por:

```c
static void material_layout_apply(HWND h)
{
    int vis_metal   = (g_work.material_mode == 1);
    int vis_rough   = (g_work.material_mode == 1 || g_work.material_mode == 2);
    int vis_env_hdr = (g_work.material_mode == 1 || g_work.material_mode == 2);
    int vis_env_pick = vis_env_hdr && (g_work.env_mode == 1);
    int visible[4] = { vis_metal, vis_rough, vis_env_hdr, vis_env_pick };

    int cursor = g_mat_block_top[0];
    for (int b = 0; b < 4; ++b) {
        if (!visible[b]) {
            for (int i = 0; i < MAT_BLOCK_N[b]; ++i)
                ShowWindow(GetDlgItem(h, g_mat_blocks[b][i].id), SW_HIDE);
            continue;
        }
        for (int i = 0; i < MAT_BLOCK_N[b]; ++i) {
            HWND ctrl = GetDlgItem(h, g_mat_blocks[b][i].id);
            SetWindowPos(ctrl, NULL, g_mat_blocks[b][i].x, cursor + g_mat_blocks[b][i].rel_y,
                         0, 0, SWP_NOSIZE | SWP_NOZORDER);
            ShowWindow(ctrl, SW_SHOW);
        }
        cursor += g_mat_block_h[b] + (b < 3 ? g_mat_gap_after[b] : 0);
    }
}
```

- [ ] **Step 10: Radios no `material_apply_i18n` (texto + estado)**

Em `src/config_dialog.c`, dentro de `material_apply_i18n`, logo após
`SetDlgItemTextW(h, IDC_ENV_LABEL, i18n_str(STR_MATERIAL_ENV_LABEL));`:

```c
    SetDlgItemTextW(h, IDC_ENVMODE_EMBED, i18n_str(STR_MATERIAL_ENV_MODE_EMBEDDED));
    SetDlgItemTextW(h, IDC_ENVMODE_CUSTOM, i18n_str(STR_MATERIAL_ENV_MODE_CUSTOM));
    SetDlgItemTextW(h, IDC_ENVMODE_NONE, i18n_str(STR_MATERIAL_ENV_MODE_NONE));
```

- [ ] **Step 11: Marcar o radio certo e aplicar layout no `WM_INITDIALOG`**

Em `src/config_dialog.c`, dentro de `material_proc`'s `WM_INITDIALOG`,
logo após `material_apply_i18n(h);` e antes de
`material_layout_capture(h);`:

```c
            CheckRadioButton(h, IDC_ENVMODE_EMBED, IDC_ENVMODE_NONE,
                              g_work.env_mode == 1 ? IDC_ENVMODE_CUSTOM :
                              g_work.env_mode == 2 ? IDC_ENVMODE_NONE : IDC_ENVMODE_EMBED);
```

- [ ] **Step 12: Handlers dos 3 radios no `WM_COMMAND`**

Em `src/config_dialog.c`, dentro de `material_proc`'s `WM_COMMAND`
`switch (LOWORD(w))`, adicionar 3 novos `case` (junto dos existentes
`IDC_MATMODE`/`IDC_ENVPICK`/`IDC_ENVCLEAR`):

```c
                case IDC_ENVMODE_EMBED:
                    g_work.env_mode = 0;
                    material_layout_apply(h);
                    material_labels(h);
                    preview_dirty(h);
                    break;
                case IDC_ENVMODE_CUSTOM:
                    g_work.env_mode = 1;
                    material_layout_apply(h);
                    material_labels(h);
                    preview_dirty(h);
                    break;
                case IDC_ENVMODE_NONE:
                    g_work.env_mode = 2;
                    g_work.env_path[0] = 0;
                    material_layout_apply(h);
                    material_labels(h);
                    preview_dirty(h);
                    break;
```

- [ ] **Step 13: Build limpo**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile release
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

Expected: build sem warnings novos.

- [ ] **Step 14: Verificação visual real**

Backup/checagem do registro `HKCU\Software\Modern3DText` antes de
limpar pra teste. Aviso prévio (PushNotification, senão chat +
`AskUserQuestion` aguardando confirmação) antes de qualquer captura.

Com registro limpo, abrir o dialog na aba Material (`M3DT_TAB`),
modo Metálico (default), e verificar via captura real:

1. Estado inicial: radio "Embutida" marcado, bloco de
   caminho/Escolher/Limpar **oculto**, reflexo visível no preview
   (confirma que a imagem embutida carregou e não é a procedural
   antiga - comparar com uma captura da Fase 8b-1 se ajudar).
2. Selecionar "Personalizada" (via `CB_SETCURSEL`-equivalente pro
   radio: `SendMessage(BM_CLICK)` + `WM_COMMAND`/`BN_CLICKED`) - o
   bloco de caminho/Escolher/Limpar **aparece**, mostrando
   "(procedural)" (nenhum arquivo escolhido ainda).
3. Selecionar "Nenhuma" - bloco de caminho some de novo, preview volta
   pro ambiente procedural (comparar visualmente com o Embutida do
   passo 1 - devem ser visivelmente diferentes).
4. Selecionar "Personalizada" de novo, sem escolher arquivo -
   confirma que voltar não trouxe nenhum caminho fantasma (já foi
   limpo no passo 3).
5. Trocar pra modo Vidro - confirma que os blocos de Metalização
   somem mas o de Ambiente continua (Vidro também usa
   `sample_env()`), radio ainda no estado escolhido.
6. Trocar pra Clássico ou Fosco - confirma que o bloco de Ambiente
   inteiro (label + radios + o que estiver visível do bloco de
   caminho) some, igual já acontecia antes desta fase.

- [ ] **Step 15: Enviar o `.exe` e commit**

```bash
git add src/resource.h res/screensaver.rc src/i18n.h src/i18n.c \
        res/lang/pt.txt res/lang/en.txt src/config_dialog.c
git commit -m "$(cat <<'EOF'
feat: 3-way ambient image radios (embedded/custom/none) in Material tab

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

Enviar `dist/Modern3DText.exe` ao usuário via `SendUserFile`.
