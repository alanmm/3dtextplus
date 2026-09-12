# Fase 6a — Modo Relógio — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Adicionar um modo de conteúdo "relógio" (`CONTENT_CLOCK`), exibindo
hora/data localizada do Windows, reaproveitando 100% do pipeline de texto
existente.

**Architecture:** `ContentMode` ganha um segundo valor. Um novo módulo
puro e testável `src/util/clockfmt.c` formata `SYSTEMTIME` → string UTF-8.
`scene_render()` chama isso a cada frame quando em modo relógio, comparando
com o texto já construído e reconstruindo a malha (via o `rebuild_mesh()`
já existente) só quando a string muda de verdade — sem nenhum código novo
de geometria.

**Tech Stack:** C11, Win32 (`GetTimeFormatEx`/`GetDateFormatEx`), reaproveita
`font_outline.c`/`contour_mesh.c` sem alteração.

## Global Constraints

- Schema do `Config` continua v2 (campos ausentes usam o default).
- Sem toggle de 12h/24h — sempre o formato do locale do usuário
  (`LOCALE_NAME_USER_DEFAULT`), como o spec principal pede.
- Build: `mingw32-make -f build/Makefile debug` (ou `release`) a partir da
  raiz, com w64devkit no PATH
  (`export PATH="/c/Users/alanm/w64devkit/bin:$PATH"`). Testes:
  `mingw32-make -f build/Makefile test`. Sempre `mingw32-make -f
  build/Makefile clean` antes de builds depois de editar um `.h` ou trocar
  debug↔release (incrementais não são header-safe neste Makefile).

---

### Task 1: Config schema

**Files:**
- Modify: `src/config.h` (`ContentMode`, struct `Config`)
- Modify: `src/config.c` (`config_defaults`, `config_load_from`,
  `config_save_to`)
- Test: `build/tests/test_config.c`

**Interfaces:**
- Produces: `CONTENT_CLOCK` (valor 1 de `ContentMode`), `Config.clock_show_date`,
  `Config.clock_show_seconds` (int, 0/1). Consumidos pelas Tasks 3 e 5.

- [ ] **Step 1: `ContentMode` e novos campos**

Em `src/config.h`, trocar:

```c
typedef enum { CONTENT_TEXT = 0 } ContentMode;
```

por:

```c
typedef enum { CONTENT_TEXT = 0, CONTENT_CLOCK = 1 } ContentMode;
```

E logo após `float particles_opacity;` (antes de `} Config;`):

```c
    int          clock_show_date;      /* 0/1 */
    int          clock_show_seconds;   /* 0/1 */
```

- [ ] **Step 2: Defaults em `config_defaults`**

Em `src/config.c`, logo após `c->particles_opacity = 1.0f;`:

```c
    c->clock_show_date = 0;
    c->clock_show_seconds = 0;
```

- [ ] **Step 3: Carregar em `config_load_from`**

Logo antes de `RegCloseKey(k);`:

```c
    reg_get_i(k, L"clock_show_date", &c->clock_show_date);
    reg_get_i(k, L"clock_show_seconds", &c->clock_show_seconds);
```

E no bloco de saneamento (antes de `if (c->text[0] == 0) ...`):

```c
    c->clock_show_date = c->clock_show_date ? 1 : 0;
    c->clock_show_seconds = c->clock_show_seconds ? 1 : 0;
```

- [ ] **Step 4: Salvar em `config_save_to`**

Logo após `set_f(k, L"particles_opacity", ...)`:

```c
    set_f(k, L"clock_show_date", (float)c->clock_show_date);
    set_f(k, L"clock_show_seconds", (float)c->clock_show_seconds);
```

- [ ] **Step 5: Estender `build/tests/test_config.c`**

No bloco de defaults:

```c
    EXPECT(d.content_mode == CONTENT_TEXT);
    EXPECT(d.clock_show_date == 0);
    EXPECT(d.clock_show_seconds == 0);
```

No bloco de round-trip, junto das outras atribuições de `a`:

```c
    a.content_mode = CONTENT_CLOCK;
    a.clock_show_date = 1;
    a.clock_show_seconds = 1;
```

E nas verificações de `b`:

```c
    EXPECT(b.content_mode == CONTENT_CLOCK);
    EXPECT(b.clock_show_date == 1);
    EXPECT(b.clock_show_seconds == 1);
```

- [ ] **Step 6: Rodar os testes**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```

- [ ] **Step 7: Commit**

```bash
git add src/config.h src/config.c build/tests/test_config.c
git commit -m "feat: add clock content mode config fields"
```

---

### Task 2: Módulo `src/util/clockfmt.c`

**Files:**
- Create: `src/util/clockfmt.h`
- Create: `src/util/clockfmt.c`
- Create: `build/tests/test_clockfmt.c`
- Modify: `build/tests/test_main.c`
- Modify: `build/Makefile` (`SRC_C`, `TEST_SRC`, `TEST_UNITS`, `test` target LIBS)

**Interfaces:**
- Produces: `void clock_format(SYSTEMTIME st, int show_date, int show_seconds, char *out, int outsz);`
  Consumido pela Task 3 (`scene.c`).

- [ ] **Step 1: `src/util/clockfmt.h`**

```c
#ifndef M3DT_CLOCKFMT_H
#define M3DT_CLOCKFMT_H
#include <windows.h>

/* Formata st em UTF-8 no formato localizado do Windows (locale do
   usuario). show_date prefixa a data (GetDateFormatEx) seguida de '\n'
   antes da hora; show_seconds inclui segundos na hora (senao usa
   TIME_NOSECONDS). Nunca deixa out vazio nem estoura outsz. */
void clock_format(SYSTEMTIME st, int show_date, int show_seconds, char *out, int outsz);

#endif
```

- [ ] **Step 2: `src/util/clockfmt.c`**

```c
#include "util/clockfmt.h"
#include <wchar.h>

void clock_format(SYSTEMTIME st, int show_date, int show_seconds, char *out, int outsz)
{
    wchar_t time_buf[128], date_buf[128], full[300];

    DWORD flags = show_seconds ? 0 : TIME_NOSECONDS;
    if (GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, flags, &st, NULL, time_buf, 128) == 0)
        wcscpy(time_buf, L"--:--");

    if (show_date) {
        if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, NULL, date_buf, 128, NULL) == 0)
            wcscpy(date_buf, L"----");
        swprintf(full, 300, L"%ls\n%ls", date_buf, time_buf);
    } else {
        swprintf(full, 300, L"%ls", time_buf);
    }

    WideCharToMultiByte(CP_UTF8, 0, full, -1, out, outsz, NULL, NULL);
    out[outsz - 1] = 0;
}
```

- [ ] **Step 3: `build/tests/test_clockfmt.c`**

```c
#include "test.h"
#include "util/clockfmt.h"
#include <string.h>

void run_clockfmt_tests(void)
{
    SYSTEMTIME st;
    memset(&st, 0, sizeof st);
    st.wYear = 2026; st.wMonth = 9; st.wDay = 12;
    st.wHour = 14; st.wMinute = 35; st.wSecond = 22;

    char buf_no_date[512], buf_date[512];
    clock_format(st, 0, 1, buf_no_date, sizeof buf_no_date);
    EXPECT(buf_no_date[0] != 0);
    EXPECT(strchr(buf_no_date, '\n') == NULL);   /* sem data = 1 linha */

    clock_format(st, 1, 1, buf_date, sizeof buf_date);
    EXPECT(buf_date[0] != 0);
    EXPECT(strchr(buf_date, '\n') != NULL);       /* com data = 2 linhas */

    char buf_sec[512], buf_nosec[512];
    clock_format(st, 0, 1, buf_sec, sizeof buf_sec);
    clock_format(st, 0, 0, buf_nosec, sizeof buf_nosec);
    EXPECT(strcmp(buf_sec, buf_nosec) != 0);      /* toggle de segundos muda a string */
}
```

- [ ] **Step 4: Registrar em `build/tests/test_main.c`**

Adicionar `void run_clockfmt_tests(void);` junto das outras declarações, e
`run_clockfmt_tests();` junto das outras chamadas em `main()`.

- [ ] **Step 5: `build/Makefile` — adicionar aos 3 lugares**

Em `SRC_C` (bloco principal), logo após `src/util/mathx.c \`:

```makefile
SRC_C := src/main.c src/cmdline.c src/util/log.c src/util/mathx.c src/util/clockfmt.c src/gl_core.c \
```

Em `TEST_SRC`, adicionar `build/tests/test_clockfmt.c`:

```makefile
TEST_SRC   := build/tests/test_main.c build/tests/test_cmdline.c build/tests/test_log.c \
              build/tests/test_mathx.c build/tests/test_font_outline.c \
              build/tests/test_contour_mesh.c build/tests/test_config.c build/tests/test_sdf.c \
              build/tests/test_render_tiers.c build/tests/test_clockfmt.c
```

Em `TEST_UNITS`, adicionar `src/util/clockfmt.c`:

```makefile
TEST_UNITS := src/cmdline.c src/util/log.c src/util/mathx.c src/util/clockfmt.c \
              src/geometry/font_outline.c src/geometry/contour_mesh.c src/config.c \
              src/geometry/sdf.c src/render_tiers.c
```

No target `test:`, adicionar `-lkernel32` ao link (defensivo — `GetTimeFormatEx`/
`GetDateFormatEx` vêm de lá; o link principal do `.scr` já lista
`-lkernel32` explicitamente, o dos testes ainda não):

```makefile
test: build/obj/test/stb_impl.o $(LIBTESS_TEST_OBJ)
	$(CC) -std=c11 -Wall -Wextra $(TEST_INC) \
	  $(TEST_SRC) $(TEST_UNITS) $^ \
	  -o build/obj/run_tests.exe -lm -lgdi32 -luser32 -ladvapi32 -lkernel32
	./build/obj/run_tests.exe
```

- [ ] **Step 6: Rodar os testes**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```

Esperado: build sem erro, todos os `EXPECT` (incluindo os novos de
`clockfmt`) passam.

- [ ] **Step 7: Commit**

```bash
git add src/util/clockfmt.h src/util/clockfmt.c build/tests/test_clockfmt.c \
        build/tests/test_main.c build/Makefile
git commit -m "feat: add clock_format() - localized date/time string, unit tested"
```

---

### Task 3: Integração em `scene.c`

**Files:**
- Modify: `src/scene.c`

**Interfaces:**
- Consumes: `clock_format()` (Task 2), `Config.content_mode`/
  `clock_show_date`/`clock_show_seconds` (Task 1).

- [ ] **Step 1: Include**

Em `src/scene.c:1-17`, adicionar `#include "util/clockfmt.h"` junto dos
outros includes de projeto (depois de `#include "particles.h"`).

- [ ] **Step 2: Novos campos em `struct SceneRenderer`**

Logo após os campos de partículas (`int wall_cache_count;` ... `int have_last_t;`):

```c
    int content_mode;
    int clock_show_date, clock_show_seconds;
```

- [ ] **Step 3: Copiar config + considerar no `mesh_dirty` em `scene_set_config`**

Em `src/scene.c`, no cálculo de `mesh_dirty` (linhas 216-228), adicionar
mais uma condição no final da cadeia `||`:

```c
    int mesh_dirty = !s->have_mesh
        || strcmp(s->text, cfg->text) != 0
        || wcscmp(s->font_family, cfg->font_family) != 0
        || s->bold != cfg->font_bold
        || s->italic != cfg->font_italic
        || s->depth != cfg->depth
        || s->bevel_mode != cfg->bevel_mode
        || s->bevel_size != cfg->bevel_size
        || s->bevel_depth != cfg->bevel_depth
        || s->bevel_segments != cfg->bevel_segments
        || s->shell != cfg->shell
        || s->wall_thickness != cfg->wall_thickness
        || s->quality != cfg->quality
        || s->content_mode != cfg->content_mode;
```

Logo após `strncpy(s->text, cfg->text, ...); s->text[sizeof s->text - 1] = 0;`:

```c
    s->content_mode = cfg->content_mode;
    s->clock_show_date = cfg->clock_show_date;
    s->clock_show_seconds = cfg->clock_show_seconds;
```

- [ ] **Step 4: Gerar a string do relógio em `scene_render`**

Em `src/scene.c`, dentro de `scene_render`, logo após o bloco que calcula
`dt`/`s->have_last_t = 1;` e antes de `glViewport(...)`:

```c
    if (s->content_mode == CONTENT_CLOCK) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char buf[512];
        clock_format(st, s->clock_show_date, s->clock_show_seconds, buf, sizeof buf);
        if (strcmp(buf, s->text) != 0) {
            strncpy(s->text, buf, sizeof s->text - 1);
            s->text[sizeof s->text - 1] = 0;
            if (!rebuild_mesh(s))
                log_errorf("scene: rebuild_mesh (relogio) falhou (text='%s')", s->text);
        }
    }
```

(reaproveita o `rebuild_mesh()` já existente sem nenhuma duplicação —
`s->font_family`/`bold`/`italic`/`depth`/bevel/etc. já foram copiados de
`cfg` em `scene_set_config`, então a malha do relógio usa exatamente a
mesma fonte/geometria configurada pelo usuário.)

- [ ] **Step 5: Build de depuração completo**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile debug
```

Esperado: `Built dist/Modern3DText.scr (N bytes)`, sem erros.

- [ ] **Step 6: Commit**

```bash
git add src/scene.c
git commit -m "feat: render clock content mode in scene.c"
```

---

### Task 4: IDs de recurso + layout da aba "Conteúdo"

**Files:**
- Modify: `src/resource.h`
- Modify: `res/screensaver.rc`

**Interfaces:**
- Produces: `IDC_CONTMODE`, `IDC_CLOCKDATE`, `IDC_CLOCKSEC` (1105-1107).
  Consumidos pela Task 5.

- [ ] **Step 1: Novos IDs em `src/resource.h`**

Logo após `#define IDC_COLOR          1104`:

```c
#define IDC_CONTMODE      1105
#define IDC_CLOCKDATE     1106
#define IDC_CLOCKSEC      1107
```

- [ ] **Step 2: Reorganizar `IDD_TAB_CONTENT` em `res/screensaver.rc`**

O combo de modo precisa vir antes de tudo (decide o que o resto da aba
significa) — desloca os controles existentes 20 unidades pra baixo pra
abrir espaço, e adiciona os 2 checkboxes novos ao lado de
Negrito/Italico. Substituir o bloco `IDD_TAB_CONTENT` inteiro:

```rc
IDD_TAB_CONTENT DIALOGEX 0, 0, 240, 200
STYLE DS_SETFONT | DS_CONTROL | WS_CHILD
FONT 9, "Segoe UI", 400, 0, 0x1
BEGIN
    LTEXT           "Modo:", -1, 8, 10, 60, 9
    COMBOBOX        IDC_CONTMODE, 68, 8, 100, 60, CBS_DROPDOWNLIST | WS_TABSTOP

    LTEXT           "Texto:", -1, 8, 30, 60, 9
    EDITTEXT        IDC_TEXT, 8, 42, 224, 40,
                    ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP
    LTEXT           "Fonte:", -1, 8, 92, 60, 9
    COMBOBOX        IDC_FONT, 8, 104, 224, 160,
                    CBS_DROPDOWNLIST | CBS_SORT | WS_VSCROLL | WS_TABSTOP
    AUTOCHECKBOX     "Negrito", IDC_BOLD,   8, 128, 60, 12
    AUTOCHECKBOX     "Italico", IDC_ITALIC, 76, 128, 60, 12
    AUTOCHECKBOX     "Mostrar data", IDC_CLOCKDATE, 130, 128, 100, 12
    LTEXT           "Cor:", -1, 8, 150, 60, 9
    PUSHBUTTON      "Escolher cor...", IDC_COLOR, 8, 162, 90, 14
    AUTOCHECKBOX     "Mostrar segundos", IDC_CLOCKSEC, 130, 144, 110, 12
END
```

- [ ] **Step 3: Verificar que o `.rc` compila**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
windres --include-dir res --include-dir src -O coff res/screensaver.rc -o build/obj/screensaver.res
echo "rc ok"
```

- [ ] **Step 4: Commit**

```bash
git add src/resource.h res/screensaver.rc
git commit -m "feat: add mode combo + clock checkboxes to Conteudo tab layout"
```

---

### Task 5: Wiring em `config_dialog.c`

**Files:**
- Modify: `src/config_dialog.c`

**Interfaces:**
- Consumes: `IDC_CONTMODE`/`IDC_CLOCKDATE`/`IDC_CLOCKSEC` (Task 4);
  `Config.content_mode`/`clock_show_date`/`clock_show_seconds` (Task 1).

- [ ] **Step 1: `content_enable()` helper**

Logo antes de `content_proc` (antes de `static INT_PTR CALLBACK content_proc`):

```c
static void content_enable(HWND h)
{
    EnableWindow(GetDlgItem(h, IDC_TEXT), g_work.content_mode == CONTENT_TEXT);
    EnableWindow(GetDlgItem(h, IDC_CLOCKDATE), g_work.content_mode == CONTENT_CLOCK);
    EnableWindow(GetDlgItem(h, IDC_CLOCKSEC), g_work.content_mode == CONTENT_CLOCK);
}
```

(fonte/negrito/italico/cor continuam sempre habilitados — se aplicam tanto
ao texto quanto ao relógio.)

- [ ] **Step 2: Popular o combo e os checkboxes em `WM_INITDIALOG`**

Em `content_proc`, dentro do `case WM_INITDIALOG:`, logo antes do
`return TRUE;`:

```c
            static const wchar_t *modes[] = { L"Texto", L"Relogio" };
            for (int i = 0; i < 2; ++i)
                SendDlgItemMessageW(h, IDC_CONTMODE, CB_ADDSTRING, 0, (LPARAM)modes[i]);
            SendDlgItemMessageW(h, IDC_CONTMODE, CB_SETCURSEL, g_work.content_mode, 0);
            CheckDlgButton(h, IDC_CLOCKDATE, g_work.clock_show_date ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(h, IDC_CLOCKSEC, g_work.clock_show_seconds ? BST_CHECKED : BST_UNCHECKED);
            content_enable(h);
```

- [ ] **Step 3: `WM_COMMAND` — modo + checkboxes**

Dentro do `switch (LOWORD(w))` de `content_proc`, adicionar (antes do
`case IDC_COLOR:` já existente, por exemplo):

```c
                case IDC_CONTMODE:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.content_mode =
                            (ContentMode)SendDlgItemMessageW(h, IDC_CONTMODE, CB_GETCURSEL, 0, 0);
                        content_enable(h);
                        preview_dirty(h);
                    }
                    break;
                case IDC_CLOCKDATE:
                    g_work.clock_show_date = (IsDlgButtonChecked(h, IDC_CLOCKDATE) == BST_CHECKED);
                    preview_dirty(h);
                    break;
                case IDC_CLOCKSEC:
                    g_work.clock_show_seconds = (IsDlgButtonChecked(h, IDC_CLOCKSEC) == BST_CHECKED);
                    preview_dirty(h);
                    break;
```

- [ ] **Step 4: Build de depuração + checagem de abertura da aba**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile debug
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

```powershell
$env:M3DT_SELFTEST = "1"
$env:M3DT_TAB = "0"
$env:M3DT_HOLD_MS = "4000"
$p = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p | Wait-Process -Timeout 8
Write-Output "exitcode: $($p.ExitCode)"
```

Esperado: `exitcode: 0`, sem crash.

- [ ] **Step 5: Commit**

```bash
git add src/config_dialog.c
git commit -m "feat: wire content mode combo and clock checkboxes into Conteudo tab"
```

---

### Task 6: Verificação final

**Files:** nenhum (só execução/validação).

**Aviso ao usuário**: enviar uma mensagem no chat antes de rodar as
capturas reais deste task, pedindo pra pausar mouse/teclado por alguns
segundos (ver [[feedback-screenshot-testing-etiquette]] — `PushNotification`
já se mostrou não confiável para este usuário, usar mensagem de chat).

- [ ] **Step 1: Suite de testes completa**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile test
```

- [ ] **Step 2: Capturar o modo relógio (com e sem data/segundos)**

```powershell
function Set-ClockConfig($showDate, $showSeconds) {
    $key = "HKCU:\Software\Modern3DText"
    New-Item -Path $key -Force | Out-Null
    Set-ItemProperty -Path $key -Name "content_mode" -Value "1"
    Set-ItemProperty -Path $key -Name "clock_show_date" -Value "$showDate"
    Set-ItemProperty -Path $key -Name "clock_show_seconds" -Value "$showSeconds"
}
function Shoot($outPath) {
    $env:M3DT_SELFTEST = "1"
    $env:M3DT_SHOT = $outPath
    $env:M3DT_HOLD_MS = "4000"
    $p = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
    $p | Wait-Process -Timeout 10
    Write-Output "$outPath exists: $(Test-Path $outPath)"
}

Set-ClockConfig 0 0
Shoot "$env:TEMP\m3dt_clock_basic.png"

Set-ClockConfig 1 1
Shoot "$env:TEMP\m3dt_clock_full.png"
```

Usar `Read` nos dois PNGs: `m3dt_clock_basic.png` deve mostrar só a hora
(sem segundos, sem data, 1 linha); `m3dt_clock_full.png` deve mostrar data
+ hora com segundos, em 2 linhas.

- [ ] **Step 3: Confirmar que os segundos avançam**

Duas capturas do modo relógio (`clock_show_seconds=1`) separadas por mais
de 1 segundo devem mostrar segundos diferentes:

```powershell
Set-ClockConfig 0 1
Shoot "$env:TEMP\m3dt_clock_t1.png"
Start-Sleep -Seconds 2
Shoot "$env:TEMP\m3dt_clock_t2.png"
```

Comparar visualmente os dois (`Read`) — o valor de segundos deve ser
diferente entre as duas capturas.

- [ ] **Step 4: Confirmar que o modo Texto continua funcionando**

```powershell
$key = "HKCU:\Software\Modern3DText"
Set-ItemProperty -Path $key -Name "content_mode" -Value "0"
Shoot "$env:TEMP\m3dt_text_mode.png"
```

Deve mostrar o texto configurado normalmente (sem regressão).

- [ ] **Step 5: Checagem dual-monitor**

```powershell
Remove-Item -Path "HKCU:\Software\Modern3DText" -Recurse -Force -ErrorAction SilentlyContinue
Set-ClockConfig 1 1
$env:M3DT_SELFTEST = "0"
$env:M3DT_SHOT = "$env:TEMP\m3dt_clock_mon1.png"
$env:M3DT_SHOT2 = "$env:TEMP\m3dt_clock_mon2.png"
$env:M3DT_SHOT_T = "2.25"
$p = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/s" -PassThru
$p | Wait-Process -Timeout 10
Write-Output "mon1: $(Test-Path $env:TEMP\m3dt_clock_mon1.png)"
Write-Output "mon2: $(Test-Path $env:TEMP\m3dt_clock_mon2.png)"
```

(repetir até 3x se algum dos dois arquivos não existir — flakiness já
documentada de `WM_MOUSEMOVE` espúrio em runs reais não-selftest.)

- [ ] **Step 6: Restaurar o registro para os defaults**

```powershell
Remove-Item -Path "HKCU:\Software\Modern3DText" -Recurse -Force -ErrorAction SilentlyContinue
```

- [ ] **Step 7: Build release final**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile release
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

- [ ] **Step 8: Commit final (rede de segurança, só se sobrou algo)**

```bash
git add -A
git status
```

---

## Self-Review (executado antes de apresentar o plano)

1. **Cobertura do spec/design**: §2 (arquitetura, reaproveitar
   `rebuild_mesh`) → Tasks 2-3; §3 (campos) → Task 1; §4 (interface) →
   Tasks 4-5; §5 (preview ao vivo) → automático, sem tarefa dedicada; §6
   (testes) → Task 2 (unitário de `clock_format`) + Task 6 (visual). §7
   (fora de escopo) não gera tarefas.
2. **Placeholders**: nenhum "TBD"/"depois" — todo trecho de código cola
   direto no arquivo indicado.
3. **Consistência de tipos**: `ContentMode`/`CONTENT_CLOCK` usados
   identicamente em `config.h` (Task 1), `scene.c` (Task 3) e
   `config_dialog.c` (Task 5); `clock_format()` tem a mesma assinatura em
   `clockfmt.h` (Task 2), no chamador em `scene.c` (Task 3) e no teste
   (Task 2); os IDs `IDC_CONTMODE`/`IDC_CLOCKDATE`/`IDC_CLOCKSEC` usados em
   `config_dialog.c` (Task 5) são exatamente os declarados em
   `resource.h` (Task 4).

---

**Plano completo e salvo em `docs/superpowers/plans/2026-09-12-phase6a-clock.md`.**

Duas opções de execução:

1. **Subagent-Driven (recomendado)** — dispatco um subagente novo por
   task, com revisão entre elas e iteração rápida.
2. **Inline Execution** — executo as tasks nesta sessão via
   `executing-plans`, em lote com checkpoints para revisão.

Qual prefere?
