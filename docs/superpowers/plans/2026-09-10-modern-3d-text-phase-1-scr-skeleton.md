# Modern 3D Text — Fase 1: Esqueleto do `.scr` — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produzir um `Modern3DText.scr` nativo que o Windows reconhece como proteção de tela — despacha `/s`, `/p <hwnd>`, `/c` e sem-argumentos; abre uma janela OpenGL fullscreen por monitor que limpa a tela com uma cor animada e encerra ao primeiro input; roda o preview dentro da mini-tela do diálogo do Windows; e abre um diálogo de configuração mínimo.

**Architecture:** Um único executável Win32 em C11, compilado com o toolchain portátil w64devkit. `src/main.c` faz o `wWinMain`, parseia a linha de comando (`src/cmdline.c`, unidade pura sem `windows.h`) e despacha para um de três modos em `src/host_win32.c` (saver, preview) ou `src/config_dialog.c` (config). O contexto OpenGL 3.3 core é criado à mão via WGL (truque da janela dummy para carregar as funções ARB). Sem bibliotecas de terceiros nesta fase — `glClear`/`glClearColor`/`glViewport` são do OpenGL 1.1, exportados diretamente por `opengl32`.

**Tech Stack:** C11 · w64devkit (gcc, windres, mingw32-make) · Win32 API · WGL / OpenGL 3.3 core · recurso de diálogo via `.rc` + `windres`.

## Global Constraints

Copiado verbatim do spec (`docs/superpowers/specs/2026-09-10-modern-3d-text-screensaver-design.md`). Todo task herda esta seção.

- **Linguagem:** C11. Flags de compilação: `-std=c11 -municode -Wall -Wextra`; release adiciona `-O2 -DNDEBUG`; **nunca** usar `-ffast-math`.
- **Toolchain:** somente w64devkit (sem MSVC, sem .NET, sem Rust, sem MinGW de outra fonte). Versão fixada em `toolchain.txt`.
- **API gráfica:** OpenGL 3.3 core, com fallback de criação de contexto 3.3 → 3.1 → 2.1 → legado.
- **Sem dependência de runtime** além de DLLs que já vêm no Windows. Libs de link permitidas: `-lopengl32 -lgdi32 -luser32 -lkernel32 -lcomdlg32 -lcomctl32 -lshell32 -lole32 -ladvapi32 -ldwmapi -lwinmm`.
- **Nomes:** binário `Modern3DText.scr`; classe/produto "Modern 3D Text"; dados locais em `%LOCALAPPDATA%\Modern3DText\` (log). Registro `HKCU\Software\Modern3DText` (só a partir da Fase 2).
- **Nenhuma escrita** fora de `HKCU\Software\Modern3DText` e `%LOCALAPPDATA%\Modern3DText\`.
- **Tamanho:** o build falha se `dist/Modern3DText.scr` passar de 3 MB.
- **Commits frequentes.** TDD nas unidades puras (parse de linha de comando, formatação de log). Partes de janela/GL são verificadas por checklist manual explícito.
- **Atribuição:** toda mensagem de commit termina com a linha `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>`.

## Pré-requisito do ambiente (fazer uma vez, antes do Task 1)

1. Baixar o release mais recente do **w64devkit** (variante `w64devkit-x64` ou `w64devkit`, o `.zip` "fortran-less" serve): https://github.com/skeeto/w64devkit/releases
2. Extrair em `C:\w64devkit` (qualquer pasta serve; sem instalador, sem admin).
3. Abrir `C:\w64devkit\w64devkit.exe` — isso dá um shell `sh` com `gcc`, `mingw32-make`, `windres` no `PATH`. **Todos os comandos deste plano rodam nesse shell**, com o diretório atual em `D:/Downloads/Screensaver`.
4. Verificar:
   ```sh
   gcc --version && mingw32-make --version && windres --version
   ```
   Esperado: gcc 14.x (ou a versão do release), GNU Make 4.x, GNU windres.

---

## File Structure

Arquivos criados nesta fase e sua responsabilidade única:

| Arquivo | Responsabilidade |
|---|---|
| `toolchain.txt` | Versão exata do w64devkit usada (reprodutibilidade) |
| `README.md` | Setup do toolchain + como buildar/rodar |
| `.gitattributes` | Normalização de fim de linha (LF no repo; binários marcados) |
| `build/Makefile` | Alvos `release`/`debug`/`run`/`config`/`test`/`clean`; regras de compilação, `windres`, link, checagem de tamanho |
| `build/tests/test.h` | Macros de asserção do runner de testes |
| `build/tests/test_main.c` | `main()` do runner; chama cada grupo de testes |
| `build/tests/test_cmdline.c` | Testes de `m3dt_cmd_parse` |
| `build/tests/test_log.c` | Testes de `log_format_line` |
| `src/resource.h` | IDs de recursos (diálogo, controles) |
| `src/cmdline.h` / `src/cmdline.c` | Parse de `/s /p /c` → `M3dtCmdLine` (puro, sem `windows.h`) |
| `src/util/log.h` / `src/util/log.c` | Log para `%LOCALAPPDATA%\Modern3DText\log.txt` + `OutputDebugString` em debug; formatador puro testável |
| `src/host_win32.h` / `src/host_win32.c` | WGL bootstrap, criação/destruição de janela GL, loop de render, watcher de input; `host_run_saver`, `host_run_preview` |
| `src/config_dialog.h` / `src/config_dialog.c` | `config_dialog_run` — diálogo mínimo (stub desta fase) |
| `src/main.c` | `wWinMain` — init de log, parse, dispatch |
| `res/screensaver.rc` | Template do diálogo, manifest, `VERSIONINFO` |
| `res/screensaver.manifest` | DPI per-monitor v2 + Common Controls v6 |

---

## Task 1: Build mínimo — `make` produz um `.scr` que roda e sai

**Files:**
- Create: `toolchain.txt`
- Create: `.gitattributes`
- Create: `build/Makefile`
- Create: `src/main.c` (stub temporário; substituído no Task 8)
- Create: `README.md`

**Interfaces:**
- Consumes: nada.
- Produces: alvo `make release` gera `dist/Modern3DText.scr`; alvo `make clean`. `src/main.c` define `int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)`.

- [ ] **Step 1: Escrever `toolchain.txt`**

Substituir `<...>` pela versão realmente baixada (visível em `gcc --version` e no nome do zip):

```
w64devkit <VERSION>   (ex.: 2.0.0)
gcc <GCC VERSION>      (ex.: 14.2.0)
Baixado de: https://github.com/skeeto/w64devkit/releases/tag/v<VERSION>
Alvo: x86_64-w64-mingw32
```

- [ ] **Step 2: Escrever `.gitattributes`**

```gitattributes
* text=auto eol=lf
*.rc        text eol=lf
*.manifest  text eol=lf
*.ico  binary
*.ttf  binary
*.otf  binary
*.png  binary
*.glb  binary
*.obj  -text
```

- [ ] **Step 3: Escrever `src/main.c` (stub)**

```c
#include <windows.h>

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR lpCmdLine, int nShow)
{
    (void)hInst; (void)hPrev; (void)lpCmdLine; (void)nShow;
    /* Stub da Fase 1 / Task 1: apenas prova que o .scr builda e roda.
       Substituído pelo dispatch real no Task 8. */
    MessageBoxW(NULL, L"Modern 3D Text — stub de build OK", L"Modern 3D Text", MB_OK);
    return 0;
}
```

- [ ] **Step 4: Escrever `build/Makefile`**

```make
# Modern 3D Text — build (w64devkit)
# Rode a partir do shell do w64devkit, com CWD em D:/Downloads/Screensaver:
#   mingw32-make -f build/Makefile            (release)
#   mingw32-make -f build/Makefile debug
#   mingw32-make -f build/Makefile run
#   mingw32-make -f build/Makefile config
#   mingw32-make -f build/Makefile test
#   mingw32-make -f build/Makefile clean

CC      := gcc
WINDRES := windres
NAME    := Modern3DText
OUT     := dist/$(NAME).scr

CFLAGS  := -std=c11 -municode -Wall -Wextra -Isrc
LDFLAGS := -mwindows -municode -static -static-libgcc -s
LIBS    := -lopengl32 -lgdi32 -luser32 -lkernel32 -lcomdlg32 -lcomctl32 \
           -lshell32 -lole32 -ladvapi32 -ldwmapi -lwinmm

MODE ?= release
ifeq ($(MODE),debug)
  CFLAGS += -O0 -g -DDEBUG
else
  CFLAGS += -O2 -DNDEBUG
endif

SRC := src/main.c
OBJ := $(patsubst src/%.c,build/obj/%.o,$(SRC))
RES := build/obj/screensaver.res

MAX_BYTES := 3145728

.PHONY: all release debug run config test clean
all: release
release:
	$(MAKE) -f build/Makefile MODE=release $(OUT)
debug:
	$(MAKE) -f build/Makefile MODE=debug $(OUT)

build/obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(RES): res/screensaver.rc res/screensaver.manifest
	@mkdir -p build/obj
	$(WINDRES) --include-dir res --include-dir src -O coff $< -o $@

$(OUT): $(OBJ) $(RES)
	@mkdir -p dist
	$(CC) $(OBJ) $(RES) -o $@ $(LDFLAGS) $(LIBS)
	@sz=$$(wc -c < $@); echo "Built $@ ($$sz bytes)"; \
	 if [ $$sz -gt $(MAX_BYTES) ]; then echo "ERRO: passou de 3 MB"; exit 1; fi

run: debug
	./$(OUT) /s

config: debug
	./$(OUT) /c

TEST_SRC := build/tests/test_main.c build/tests/test_cmdline.c build/tests/test_log.c
TEST_UNITS := src/cmdline.c src/util/log.c
test:
	@mkdir -p build/obj
	$(CC) -std=c11 -Wall -Wextra -Isrc -Ibuild/tests \
	  $(TEST_SRC) $(TEST_UNITS) -o build/obj/run_tests.exe
	./build/obj/run_tests.exe

clean:
	rm -rf build/obj dist generated
```

Nota: a regra `$(RES)` referencia arquivos que só existem a partir do Task 7. Até lá, buildar com `mingw32-make -f build/Makefile` vai falhar no `windres`. Para o Task 1, buildar sem recurso (Step 5 usa um override).

- [ ] **Step 5: Buildar o stub sem recurso e confirmar que gera um `.scr`**

```sh
mkdir -p build/obj dist
gcc -std=c11 -municode -O2 -Wall -Wextra -Isrc -c src/main.c -o build/obj/main.o
gcc build/obj/main.o -o dist/Modern3DText.scr -mwindows -municode -static -static-libgcc -s -luser32
ls -l dist/Modern3DText.scr
```
Esperado: o arquivo `dist/Modern3DText.scr` existe, com algumas dezenas de KB.

- [ ] **Step 6: Rodar e confirmar**

```sh
./dist/Modern3DText.scr
```
Esperado: aparece um `MessageBox` "stub de build OK"; ao clicar OK, o processo sai com código 0 (`echo $?` → `0`).

- [ ] **Step 7: Escrever `README.md`**

```markdown
# Modern 3D Text

Reescrita moderna do screensaver clássico "Texto 3D" do Windows. Nativo,
leve (~1 MB), OpenGL 3.3.

Design completo: `docs/superpowers/specs/2026-09-10-modern-3d-text-screensaver-design.md`
Planos de implementação: `docs/superpowers/plans/`

## Build

Precisa do **w64devkit** (MinGW-w64 portátil, sem instalador):

1. Baixe de https://github.com/skeeto/w64devkit/releases e extraia em `C:\w64devkit`.
2. Rode `C:\w64devkit\w64devkit.exe` (abre um shell).
3. No shell, vá até a pasta do projeto e:
   ```sh
   mingw32-make -f build/Makefile          # release -> dist/Modern3DText.scr
   mingw32-make -f build/Makefile debug
   mingw32-make -f build/Makefile test      # testes unitários
   mingw32-make -f build/Makefile run        # roda /s
   mingw32-make -f build/Makefile config     # roda /c
   ```

Versão do toolchain testada: ver `toolchain.txt`.

## Instalar para testar

Copie `dist/Modern3DText.scr` para `C:\Windows\System32\` (precisa de admin),
ou clique com o botão direito no arquivo e escolha **Instalar** / **Testar**.

## Licença

MIT — ver `LICENSE`.
```

- [ ] **Step 8: Commit**

```sh
git add toolchain.txt .gitattributes build/Makefile src/main.c README.md
git commit -m "$(printf 'build: w64devkit Makefile + buildable .scr stub\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 2: Parse da linha de comando (`m3dt_cmd_parse`) — TDD

**Files:**
- Create: `src/cmdline.h`
- Create: `src/cmdline.c`
- Create: `build/tests/test.h`
- Create: `build/tests/test_main.c`
- Create: `build/tests/test_cmdline.c`

**Interfaces:**
- Consumes: nada.
- Produces:
  ```c
  typedef enum { M3DT_MODE_CONFIG = 0, M3DT_MODE_SAVER = 1, M3DT_MODE_PREVIEW = 2 } M3dtRunMode;
  typedef struct { M3dtRunMode mode; unsigned long long parent_hwnd; } M3dtCmdLine;
  void m3dt_cmd_parse(int argc, const wchar_t *const *argv, M3dtCmdLine *out);
  ```
  Regras: `argv[0]` é o exe; a flag está em `argv[1]`. `/s` (case-insensitive, aceita `-s`) → SAVER. `/p <n>` ou `/p:<n>` → PREVIEW com `parent_hwnd = n`; `/p` sem número → CONFIG. `/c`, `/c:<n>` → CONFIG (guarda `parent_hwnd` se houver). Sem args ou flag desconhecida → CONFIG, `parent_hwnd = 0`.

- [ ] **Step 1: Escrever `build/tests/test.h`**

```c
#ifndef M3DT_TEST_H
#define M3DT_TEST_H
#include <stdio.h>
#include <string.h>
extern int g_test_failures;
#define EXPECT(cond) do { \
    if (!(cond)) { g_test_failures++; \
      printf("FAIL %s:%d  EXPECT(%s)\n", __FILE__, __LINE__, #cond); } \
  } while (0)
#define EXPECT_EQ_INT(a,b) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (_a != _b) { g_test_failures++; \
      printf("FAIL %s:%d  %s (=%lld) != %s (=%lld)\n", __FILE__, __LINE__, #a, _a, #b, _b); } \
  } while (0)
#define EXPECT_STR(a,b) do { \
    if (strcmp((a),(b)) != 0) { g_test_failures++; \
      printf("FAIL %s:%d  \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); } \
  } while (0)
#endif
```

- [ ] **Step 2: Escrever `build/tests/test_main.c`**

```c
#include "test.h"

int g_test_failures = 0;

void run_cmdline_tests(void);
void run_log_tests(void);

int main(void)
{
    run_cmdline_tests();
    run_log_tests();
    if (g_test_failures) {
        printf("\n%d assertion(s) FAILED\n", g_test_failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
```

- [ ] **Step 3: Escrever `build/tests/test_cmdline.c` (o teste que falha)**

```c
#include "test.h"
#include "cmdline.h"
#include <wchar.h>

static M3dtCmdLine P(const wchar_t *a1, const wchar_t *a2)
{
    const wchar_t *argv[3] = { L"Modern3DText.scr", a1, a2 };
    int argc = 1 + (a1 ? 1 : 0) + (a1 && a2 ? 1 : 0);
    M3dtCmdLine c;
    m3dt_cmd_parse(argc, argv, &c);
    return c;
}

void run_cmdline_tests(void)
{
    EXPECT_EQ_INT(P(NULL, NULL).mode,        M3DT_MODE_CONFIG);
    EXPECT_EQ_INT(P(L"/s", NULL).mode,       M3DT_MODE_SAVER);
    EXPECT_EQ_INT(P(L"/S", NULL).mode,       M3DT_MODE_SAVER);
    EXPECT_EQ_INT(P(L"-s", NULL).mode,       M3DT_MODE_SAVER);
    EXPECT_EQ_INT(P(L"/c", NULL).mode,       M3DT_MODE_CONFIG);
    EXPECT_EQ_INT(P(L"/c", NULL).parent_hwnd, 0);
    EXPECT_EQ_INT(P(L"/c:1234", NULL).mode,        M3DT_MODE_CONFIG);
    EXPECT_EQ_INT(P(L"/c:1234", NULL).parent_hwnd, 1234ull);
    EXPECT_EQ_INT(P(L"/p", L"5678").mode,        M3DT_MODE_PREVIEW);
    EXPECT_EQ_INT(P(L"/p", L"5678").parent_hwnd, 5678ull);
    EXPECT_EQ_INT(P(L"/p:9012", NULL).mode,        M3DT_MODE_PREVIEW);
    EXPECT_EQ_INT(P(L"/p:9012", NULL).parent_hwnd, 9012ull);
    EXPECT_EQ_INT(P(L"/p", NULL).mode,   M3DT_MODE_CONFIG);   /* preview inválido -> config */
    EXPECT_EQ_INT(P(L"/x", NULL).mode,   M3DT_MODE_CONFIG);   /* flag desconhecida -> config */
    EXPECT_EQ_INT(P(L"texto", NULL).mode, M3DT_MODE_CONFIG);  /* sem '/' nem '-' -> config */
}
```

- [ ] **Step 4: Escrever `src/cmdline.h`**

```c
#ifndef M3DT_CMDLINE_H
#define M3DT_CMDLINE_H

#include <wchar.h>

typedef enum {
    M3DT_MODE_CONFIG  = 0,
    M3DT_MODE_SAVER   = 1,
    M3DT_MODE_PREVIEW = 2
} M3dtRunMode;

typedef struct {
    M3dtRunMode        mode;
    unsigned long long parent_hwnd;   /* valor cru; convertido para HWND no host */
} M3dtCmdLine;

/* argc/argv no estilo CommandLineToArgvW: argv[0] = caminho do exe. */
void m3dt_cmd_parse(int argc, const wchar_t *const *argv, M3dtCmdLine *out);

#endif
```

- [ ] **Step 5: Rodar os testes e confirmar que falham na compilação/link**

```sh
mingw32-make -f build/Makefile test
```
Esperado: erro de link — `undefined reference to 'm3dt_cmd_parse'` (o `.c` ainda não existe). Isso conta como "o teste falha".

- [ ] **Step 6: Escrever `src/cmdline.c` (implementação mínima)**

```c
#include "cmdline.h"
#include <wctype.h>
#include <stdlib.h>

void m3dt_cmd_parse(int argc, const wchar_t *const *argv, M3dtCmdLine *out)
{
    out->mode = M3DT_MODE_CONFIG;
    out->parent_hwnd = 0;

    if (argc < 2 || !argv || !argv[1] || !argv[1][0])
        return;

    const wchar_t *a = argv[1];
    if (a[0] != L'/' && a[0] != L'-')
        return;                         /* não é flag -> config */

    wchar_t flag = (wchar_t)towlower((wint_t)a[1]);
    if (!flag)
        return;

    /* número embutido: "/p:1234" ou "/p1234" */
    const wchar_t *inln = NULL;
    if (a[2] == L':')      inln = a + 3;
    else if (a[2])         inln = a + 2;

    const wchar_t *hs = inln;
    if (!hs && (flag == L'p' || flag == L'c') && argc >= 3 && argv[2] && argv[2][0])
        hs = argv[2];                   /* número como argumento separado */

    unsigned long long hv = hs ? wcstoull(hs, NULL, 10) : 0ull;

    switch (flag) {
        case L's':
            out->mode = M3DT_MODE_SAVER;
            break;
        case L'p':
            out->parent_hwnd = hv;
            out->mode = hv ? M3DT_MODE_PREVIEW : M3DT_MODE_CONFIG;
            break;
        case L'c':
            out->parent_hwnd = hv;
            out->mode = M3DT_MODE_CONFIG;
            break;
        default:
            out->mode = M3DT_MODE_CONFIG;
            break;
    }
}
```

- [ ] **Step 7: Rodar os testes e confirmar que passam**

```sh
mingw32-make -f build/Makefile test
```
Esperado: `all tests passed` (o `run_log_tests` ainda não existe → vai dar erro de link; se o Task 3 ainda não foi feito, comentar temporariamente a chamada `run_log_tests();` em `test_main.c` **não** — em vez disso, fazer o Task 3 em seguida). Para manter os tasks independentes: adicionar já um `build/tests/test_log.c` com um `run_log_tests(void) {}` vazio e removê-lo no Task 3 Step 1. 

Ação concreta: criar `build/tests/test_log.c` com:
```c
#include "test.h"
void run_log_tests(void) { /* preenchido no Task 3 */ }
```
e rodar de novo. Esperado agora: `all tests passed`.

- [ ] **Step 8: Commit**

```sh
git add src/cmdline.h src/cmdline.c build/tests/
git commit -m "$(printf 'feat: command-line parser for /s /p /c dispatch\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 3: Logging (`log_*`) — TDD no formatador

**Files:**
- Create: `src/util/log.h`
- Create: `src/util/log.c`
- Modify: `build/tests/test_log.c` (substitui o stub do Task 2)

**Interfaces:**
- Consumes: nada.
- Produces:
  ```c
  void log_init(void);      /* abre %LOCALAPPDATA%\Modern3DText\log.txt em append */
  void log_shutdown(void);
  void log_infof(const char *fmt, ...);
  void log_errorf(const char *fmt, ...);
  /* puro / testável: */
  void log_format_line(char *buf, int n, const char *lvl, const char *msg,
                       int Y, int Mo, int D, int h, int mi, int s);
  ```
  `log_format_line` escreve `"YYYY-MM-DD HH:MM:SS [LVL] MSG\n"` em `buf` (truncando em `n`).

- [ ] **Step 1: Substituir `build/tests/test_log.c` pelo teste real (que falha)**

```c
#include "test.h"
#include "util/log.h"

void run_log_tests(void)
{
    char b[128];
    log_format_line(b, (int)sizeof b, "INFO", "hello world",
                    2026, 9, 10, 8, 5, 3);
    EXPECT_STR(b, "2026-09-10 08:05:03 [INFO] hello world\n");

    /* truncamento seguro: n pequeno não estoura o buffer */
    char small[10];
    log_format_line(small, (int)sizeof small, "ERROR", "xxxxxxxxxxxxxxxx",
                    2026, 1, 2, 3, 4, 5);
    EXPECT(small[9] == '\0' || small[8] == '\0');
}
```

- [ ] **Step 2: Rodar e confirmar falha**

```sh
mingw32-make -f build/Makefile test
```
Esperado: erro de link `undefined reference to 'log_format_line'`.

- [ ] **Step 3: Escrever `src/util/log.h`**

```c
#ifndef M3DT_LOG_H
#define M3DT_LOG_H

void log_init(void);
void log_shutdown(void);
void log_infof(const char *fmt, ...);
void log_errorf(const char *fmt, ...);

void log_format_line(char *buf, int n, const char *lvl, const char *msg,
                     int Y, int Mo, int D, int h, int mi, int s);

#endif
```

- [ ] **Step 4: Escrever `src/util/log.c`**

```c
#include "util/log.h"

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

static FILE *g_log;

void log_format_line(char *buf, int n, const char *lvl, const char *msg,
                     int Y, int Mo, int D, int h, int mi, int s)
{
    snprintf(buf, (size_t)n, "%04d-%02d-%02d %02d:%02d:%02d [%s] %s\n",
             Y, Mo, D, h, mi, s, lvl, msg);
}

static void log_dir_path(char *out, int n)
{
    char base[MAX_PATH];
    DWORD k = GetEnvironmentVariableA("LOCALAPPDATA", base, sizeof base);
    if (k == 0 || k >= sizeof base) {
        snprintf(out, (size_t)n, ".");
        return;
    }
    snprintf(out, (size_t)n, "%s\\Modern3DText", base);
}

void log_init(void)
{
    char dir[MAX_PATH];
    log_dir_path(dir, (int)sizeof dir);
    CreateDirectoryA(dir, NULL);

    char path[MAX_PATH];
    snprintf(path, sizeof path, "%s\\log.txt", dir);
    g_log = fopen(path, "a");
    log_infof("---- log_init ----");
}

void log_shutdown(void)
{
    if (g_log) { fclose(g_log); g_log = NULL; }
}

static void log_v(const char *lvl, const char *fmt, va_list ap)
{
    char msg[1024];
    vsnprintf(msg, sizeof msg, fmt, ap);

    SYSTEMTIME t;
    GetLocalTime(&t);

    char line[1200];
    log_format_line(line, (int)sizeof line, lvl, msg,
                    t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);

    if (g_log) { fputs(line, g_log); fflush(g_log); }
#ifdef DEBUG
    OutputDebugStringA(line);
#endif
}

void log_infof(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); log_v("INFO", fmt, ap); va_end(ap);
}

void log_errorf(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); log_v("ERROR", fmt, ap); va_end(ap);
}
```

- [ ] **Step 5: Rodar e confirmar que passa**

```sh
mingw32-make -f build/Makefile test
```
Esperado: `all tests passed`.

- [ ] **Step 6: Commit**

```sh
git add src/util/log.h src/util/log.c build/tests/test_log.c
git commit -m "$(printf 'feat: file + debug logging with testable line formatter\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 4: Fundação de janela GL (WGL bootstrap + `GlWindow`)

Sem teste unitário — código de janela/GL. A verificação é um pequeno hook temporário (removido no Step final) que abre uma janela e limpa a tela.

**Files:**
- Create: `src/host_win32.h`
- Create: `src/host_win32.c`
- Modify: `build/Makefile` (adicionar `src/host_win32.c` e `src/config_dialog.c` — este último só a partir do Task 7 — a `SRC`; por ora só `host_win32.c`)

**Interfaces:**
- Consumes: `log_infof`, `log_errorf` de `src/util/log.h`.
- Produces (internos ao módulo, mas fixados aqui para os tasks seguintes):
  ```c
  /* host_win32.h */
  #include <windows.h>
  int host_run_saver(HINSTANCE hInst);
  int host_run_preview(HINSTANCE hInst, HWND parent);

  /* internos de host_win32.c reutilizados nos Tasks 5-6: */
  typedef struct { HWND hwnd; HDC dc; HGLRC rc; int w, h; } GlWindow;
  static void m3dt_set_dpi_aware(void);
  static void m3dt_wgl_bootstrap(HINSTANCE hInst);
  static int  gl_window_create(HINSTANCE hInst, GlWindow *g, DWORD style, DWORD exstyle,
                               HWND parent, int x, int y, int w, int h,
                               const wchar_t *cls, WNDPROC proc);
  static void gl_window_destroy(GlWindow *g);
  static void gl_render_clear(GlWindow *g, double t);
  ```

- [ ] **Step 1: Escrever `src/host_win32.h`**

```c
#ifndef M3DT_HOST_WIN32_H
#define M3DT_HOST_WIN32_H

#include <windows.h>

int host_run_saver(HINSTANCE hInst);
int host_run_preview(HINSTANCE hInst, HWND parent);

#endif
```

- [ ] **Step 2: Escrever `src/host_win32.c` — parte 1: DPI, WGL bootstrap, GlWindow**

```c
#include "host_win32.h"
#include "util/log.h"

#include <windowsx.h>
#include <GL/gl.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

/* ---------- constantes WGL ARB (não estão no <GL/gl.h> do MinGW) ---------- */
#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB      0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001

#define WGL_DRAW_TO_WINDOW_ARB  0x2001
#define WGL_SUPPORT_OPENGL_ARB  0x2010
#define WGL_DOUBLE_BUFFER_ARB   0x2011
#define WGL_PIXEL_TYPE_ARB      0x2013
#define WGL_TYPE_RGBA_ARB       0x202B
#define WGL_COLOR_BITS_ARB      0x2014
#define WGL_DEPTH_BITS_ARB      0x2022
#define WGL_STENCIL_BITS_ARB    0x2023

typedef HGLRC (WINAPI *PFN_wglCreateContextAttribsARB)(HDC, HGLRC, const int *);
typedef BOOL  (WINAPI *PFN_wglChoosePixelFormatARB)(HDC, const int *, const FLOAT *, UINT, int *, UINT *);
typedef BOOL  (WINAPI *PFN_wglSwapIntervalEXT)(int);

static PFN_wglCreateContextAttribsARB p_wglCreateContextAttribsARB;
static PFN_wglChoosePixelFormatARB    p_wglChoosePixelFormatARB;
static PFN_wglSwapIntervalEXT         p_wglSwapIntervalEXT;

typedef struct { HWND hwnd; HDC dc; HGLRC rc; int w, h; } GlWindow;

static void m3dt_set_dpi_aware(void)
{
    HMODULE u = GetModuleHandleW(L"user32");
    typedef BOOL (WINAPI *PFN_setctx)(HANDLE);
    PFN_setctx f = u ? (PFN_setctx)(void *)GetProcAddress(u, "SetProcessDpiAwarenessContext") : NULL;
    if (f) {
        /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4 */
        if (f((HANDLE)(INT_PTR)-4)) return;
    }
    SetProcessDPIAware();
}

static void m3dt_wgl_bootstrap(HINSTANCE hInst)
{
    static bool done = false;
    if (done) return;
    done = true;

    const wchar_t *cls = L"M3DTGLBootstrap";
    WNDCLASSW wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = hInst;
    wc.lpszClassName = cls;
    RegisterClassW(&wc);

    HWND w = CreateWindowW(cls, L"", WS_OVERLAPPED, 0, 0, 1, 1, NULL, NULL, hInst, NULL);
    HDC dc = GetDC(w);

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof pfd);
    pfd.nSize      = sizeof pfd;
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;

    int pf = ChoosePixelFormat(dc, &pfd);
    SetPixelFormat(dc, pf, &pfd);

    HGLRC rc = wglCreateContext(dc);
    wglMakeCurrent(dc, rc);

    p_wglCreateContextAttribsARB =
        (PFN_wglCreateContextAttribsARB)(void *)wglGetProcAddress("wglCreateContextAttribsARB");
    p_wglChoosePixelFormatARB =
        (PFN_wglChoosePixelFormatARB)(void *)wglGetProcAddress("wglChoosePixelFormatARB");
    p_wglSwapIntervalEXT =
        (PFN_wglSwapIntervalEXT)(void *)wglGetProcAddress("wglSwapIntervalEXT");

    log_infof("GL vendor=%s renderer=%s",
              (const char *)glGetString(GL_VENDOR),
              (const char *)glGetString(GL_RENDERER));

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(rc);
    ReleaseDC(w, dc);
    DestroyWindow(w);
    UnregisterClassW(cls, hInst);
}

static int gl_window_create(HINSTANCE hInst, GlWindow *g, DWORD style, DWORD exstyle,
                            HWND parent, int x, int y, int w, int h,
                            const wchar_t *cls, WNDPROC proc)
{
    memset(g, 0, sizeof *g);

    WNDCLASSW wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc   = proc ? proc : DefWindowProcW;
    wc.hInstance     = hInst;
    wc.hCursor       = NULL;
    wc.lpszClassName = cls;
    wc.style         = CS_OWNDC;
    RegisterClassW(&wc);   /* re-registro na 2ª chamada retorna 0; ok */

    g->hwnd = CreateWindowExW(exstyle, cls, L"Modern 3D Text", style,
                              x, y, w, h, parent, NULL, hInst, NULL);
    if (!g->hwnd) { log_errorf("CreateWindowExW falhou (%lu)", GetLastError()); return 0; }

    g->dc = GetDC(g->hwnd);

    int pf = 0;
    if (p_wglChoosePixelFormatARB) {
        const int attribs[] = {
            WGL_DRAW_TO_WINDOW_ARB, 1,
            WGL_SUPPORT_OPENGL_ARB, 1,
            WGL_DOUBLE_BUFFER_ARB,  1,
            WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB,     32,
            WGL_DEPTH_BITS_ARB,     24,
            WGL_STENCIL_BITS_ARB,   8,
            0
        };
        UINT n = 0;
        p_wglChoosePixelFormatARB(g->dc, attribs, NULL, 1, &pf, &n);
        if (n == 0) pf = 0;
    }
    if (!pf) {
        PIXELFORMATDESCRIPTOR pfd;
        memset(&pfd, 0, sizeof pfd);
        pfd.nSize = sizeof pfd; pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32; pfd.cDepthBits = 24; pfd.cStencilBits = 8;
        pf = ChoosePixelFormat(g->dc, &pfd);
    }

    PIXELFORMATDESCRIPTOR chosen;
    DescribePixelFormat(g->dc, pf, sizeof chosen, &chosen);
    SetPixelFormat(g->dc, pf, &chosen);

    const int versions[][2] = { {3,3}, {3,1}, {2,1} };
    for (int i = 0; i < 3 && !g->rc && p_wglCreateContextAttribsARB; ++i) {
        const int cattr[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, versions[i][0],
            WGL_CONTEXT_MINOR_VERSION_ARB, versions[i][1],
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        g->rc = p_wglCreateContextAttribsARB(g->dc, NULL, cattr);
        if (g->rc) log_infof("GL context %d.%d core", versions[i][0], versions[i][1]);
    }
    if (!g->rc) {
        g->rc = wglCreateContext(g->dc);
        if (g->rc) log_infof("GL context legado");
    }
    if (!g->rc) { log_errorf("sem contexto GL"); ReleaseDC(g->hwnd, g->dc); DestroyWindow(g->hwnd); return 0; }

    wglMakeCurrent(g->dc, g->rc);
    if (p_wglSwapIntervalEXT) p_wglSwapIntervalEXT(1);

    RECT cr; GetClientRect(g->hwnd, &cr);
    g->w = cr.right; g->h = cr.bottom;
    return 1;
}

static void gl_window_destroy(GlWindow *g)
{
    if (g->rc) { wglMakeCurrent(NULL, NULL); wglDeleteContext(g->rc); }
    if (g->dc && g->hwnd) ReleaseDC(g->hwnd, g->dc);
    if (g->hwnd) DestroyWindow(g->hwnd);
    memset(g, 0, sizeof *g);
}

static void gl_render_clear(GlWindow *g, double t)
{
    wglMakeCurrent(g->dc, g->rc);
    glViewport(0, 0, g->w, g->h);

    double hue = fmod(t * 0.05, 1.0);
    double r = fabs(hue * 6.0 - 3.0) - 1.0;
    double gg = 2.0 - fabs(hue * 6.0 - 2.0);
    double b = 2.0 - fabs(hue * 6.0 - 4.0);
    #define M3DT_SAT(x) ((x) < 0.0 ? 0.0 : ((x) > 1.0 ? 1.0 : (x)))
    glClearColor((float)(M3DT_SAT(r) * 0.15), (float)(M3DT_SAT(gg) * 0.15),
                 (float)(M3DT_SAT(b) * 0.15), 1.0f);
    #undef M3DT_SAT

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    SwapBuffers(g->dc);
}

/* host_run_saver / host_run_preview: Tasks 5 e 6. Stubs por ora: */
int host_run_saver(HINSTANCE hInst)   { (void)hInst; return 0; }
int host_run_preview(HINSTANCE hInst, HWND parent) { (void)hInst; (void)parent; return 0; }
```

- [ ] **Step 3: Adicionar `src/host_win32.c` ao Makefile**

Em `build/Makefile`, trocar a linha `SRC := src/main.c` por:
```make
SRC := src/main.c src/cmdline.c src/util/log.c src/host_win32.c
```

- [ ] **Step 4: Hook temporário de verificação em `src/main.c`**

Substituir o corpo de `wWinMain` por:
```c
#include <windows.h>
#include "host_win32.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR lpCmdLine, int nShow)
{
    (void)hPrev; (void)lpCmdLine; (void)nShow;
    /* VERIFICAÇÃO TEMPORÁRIA do Task 4 — substituída no Task 8 */
    extern void m3dt_task4_smoke(HINSTANCE);
    m3dt_task4_smoke(hInst);
    return 0;
}
```
E adicionar ao final de `src/host_win32.c`:
```c
void m3dt_task4_smoke(HINSTANCE hInst)
{
    m3dt_set_dpi_aware();
    m3dt_wgl_bootstrap(hInst);

    GlWindow g;
    if (!gl_window_create(hInst, &g, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, NULL,
                          CW_USEDEFAULT, CW_USEDEFAULT, 640, 400, L"M3DTSmoke", NULL))
        return;

    LARGE_INTEGER f, s; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&s);
    for (int i = 0; i < 240; ++i) {   /* ~4 s a 60 fps */
        MSG m;
        while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        gl_render_clear(&g, (double)(now.QuadPart - s.QuadPart) / (double)f.QuadPart);
        Sleep(16);
    }
    gl_window_destroy(&g);
}
```

- [ ] **Step 5: Buildar e rodar o smoke test**

```sh
gcc -std=c11 -municode -O2 -Wall -Wextra -Isrc -c src/main.c -o build/obj/main.o
gcc -std=c11 -municode -O2 -Wall -Wextra -Isrc -c src/cmdline.c -o build/obj/cmdline.o
gcc -std=c11 -municode -O2 -Wall -Wextra -Isrc -c src/util/log.c -o build/obj/util/log.o
gcc -std=c11 -municode -O2 -Wall -Wextra -Isrc -c src/host_win32.c -o build/obj/host_win32.o
gcc build/obj/main.o build/obj/cmdline.o build/obj/util/log.o build/obj/host_win32.o \
  -o dist/Modern3DText.scr -mwindows -municode -static -static-libgcc -s \
  -lopengl32 -lgdi32 -luser32 -lkernel32 -lwinmm
./dist/Modern3DText.scr
```
Esperado: abre uma janela 640×400 por ~4 s cujo fundo muda de cor lentamente (ciano→azul→roxo, bem escuro), depois fecha sozinha. Sem erros no console. `%LOCALAPPDATA%\Modern3DText\log.txt` contém uma linha `GL context 3.3 core` (ou 3.1/2.1 conforme a GPU).

- [ ] **Step 6: Rodar os testes unitários (garantir que nada quebrou)**

```sh
mingw32-make -f build/Makefile test
```
Esperado: `all tests passed`.

- [ ] **Step 7: Commit**

```sh
git add src/host_win32.h src/host_win32.c src/main.c build/Makefile
git commit -m "$(printf 'feat: WGL 3.3-core context bootstrap and GL window helpers\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 5: Modo Saver — janela por monitor, loop, watcher de input

**Files:**
- Modify: `src/host_win32.c` (implementar `host_run_saver`, adicionar `saver_wndproc`, enumeração de monitores; remover `m3dt_task4_smoke`)
- Modify: `src/main.c` (chamar `host_run_saver` de verdade — provisório até o Task 8)

**Interfaces:**
- Consumes: `GlWindow`, `gl_window_create`, `gl_window_destroy`, `gl_render_clear`, `m3dt_wgl_bootstrap`, `m3dt_set_dpi_aware` (Task 4).
- Produces: `int host_run_saver(HINSTANCE)` — cria uma janela `WS_POPUP | WS_EX_TOPMOST` por monitor, renderiza cor animada em todas, retorna 0 quando o usuário mexe o mouse (> 4 px), pressiona tecla, clica ou usa a roda.

- [ ] **Step 1: Substituir os stubs no fim de `src/host_win32.c` e remover o smoke**

Remover `m3dt_task4_smoke` e o `extern` correspondente em `main.c` (Step 5). Substituir `int host_run_saver(HINSTANCE hInst) { ... }` por:

```c
/* ---------------- modo saver ---------------- */

static volatile LONG g_quit;
static POINT g_mouse_anchor;
static bool  g_mouse_anchored;

static void request_quit(void) { InterlockedExchange(&g_quit, 1); }

static LRESULT CALLBACK saver_wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
        case WM_MOUSEMOVE: {
            POINT p; GetCursorPos(&p);
            if (!g_mouse_anchored) { g_mouse_anchor = p; g_mouse_anchored = true; break; }
            long dx = p.x - g_mouse_anchor.x, dy = p.y - g_mouse_anchor.y;
            if (dx * dx + dy * dy > 16) request_quit();   /* > 4 px */
            break;
        }
        case WM_KEYDOWN: case WM_SYSKEYDOWN:
        case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
        case WM_MOUSEWHEEL:
            request_quit();
            break;
        case WM_CLOSE: case WM_DESTROY:
            request_quit();
            return 0;
        case WM_SETCURSOR:
            SetCursor(NULL);
            return TRUE;
    }
    return DefWindowProcW(h, m, w, l);
}

typedef struct { RECT r[16]; int n; } MonitorList;

static BOOL CALLBACK monitor_cb(HMONITOR mon, HDC dc, LPRECT rc, LPARAM lp)
{
    (void)mon; (void)dc;
    MonitorList *ml = (MonitorList *)lp;
    if (ml->n < 16) ml->r[ml->n++] = *rc;
    return TRUE;
}

int host_run_saver(HINSTANCE hInst)
{
    m3dt_set_dpi_aware();
    m3dt_wgl_bootstrap(hInst);

    MonitorList ml; ml.n = 0;
    EnumDisplayMonitors(NULL, NULL, monitor_cb, (LPARAM)&ml);
    if (ml.n == 0) {
        ml.r[0].left = 0; ml.r[0].top = 0;
        ml.r[0].right = GetSystemMetrics(SM_CXSCREEN);
        ml.r[0].bottom = GetSystemMetrics(SM_CYSCREEN);
        ml.n = 1;
    }

    GlWindow win[16]; int nwin = 0;
    for (int i = 0; i < ml.n; ++i) {
        RECT r = ml.r[i];
        wchar_t cls[32]; wsprintfW(cls, L"M3DTSaver%d", i);
        if (gl_window_create(hInst, &win[nwin], WS_POPUP | WS_VISIBLE, WS_EX_TOPMOST,
                             NULL, r.left, r.top, r.right - r.left, r.bottom - r.top,
                             cls, saver_wndproc)) {
            SetWindowPos(win[nwin].hwnd, HWND_TOPMOST, r.left, r.top,
                         r.right - r.left, r.bottom - r.top, SWP_SHOWWINDOW);
            nwin++;
        }
    }
    if (nwin == 0) { log_errorf("nenhuma janela saver criada"); return 1; }

    ShowCursor(FALSE);
    SetForegroundWindow(win[0].hwnd);

    timeBeginPeriod(1);
    LARGE_INTEGER freq, start;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    while (!g_quit) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) request_quit();
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        double t = (double)(now.QuadPart - start.QuadPart) / (double)freq.QuadPart;
        for (int i = 0; i < nwin; ++i) gl_render_clear(&win[i], t);
        Sleep(1);
    }

    timeEndPeriod(1);
    ShowCursor(TRUE);
    for (int i = 0; i < nwin; ++i) gl_window_destroy(&win[i]);
    log_infof("saver encerrou");
    return 0;
}
```

- [ ] **Step 2: Ajustar `src/main.c` (provisório)**

```c
#include <windows.h>
#include "host_win32.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR lpCmdLine, int nShow)
{
    (void)hPrev; (void)lpCmdLine; (void)nShow;
    return host_run_saver(hInst);   /* provisório: só /s até o Task 8 */
}
```

- [ ] **Step 3: Buildar**

```sh
mingw32-make -f build/Makefile debug MODE=debug
```
Nota: o alvo depende de `$(RES)` (Task 7). Enquanto o `.rc` não existe, buildar direto:
```sh
mkdir -p build/obj/util
for f in main cmdline util/log host_win32; do \
  gcc -std=c11 -municode -O0 -g -DDEBUG -Wall -Wextra -Isrc -c src/$f.c -o build/obj/$f.o || exit 1; done
gcc build/obj/main.o build/obj/cmdline.o build/obj/util/log.o build/obj/host_win32.o \
  -o dist/Modern3DText.scr -mwindows -municode -static -static-libgcc \
  -lopengl32 -lgdi32 -luser32 -lkernel32 -lwinmm
```
Esperado: compila sem warnings.

- [ ] **Step 4: Verificação manual — fullscreen + saída por input**

```sh
./dist/Modern3DText.scr /s
```
Verificar:
- Tela(s) inteira(s) preenchida(s), cor escura mudando devagar, sem barra de título, sobre tudo.
- Em multi-monitor: **todas** as telas preenchidas.
- Mover o mouse ~1 cm → fecha imediatamente.
- Repetir e testar: apertar uma tecla → fecha; clicar → fecha.
- Um tremor mínimo do mouse (1–2 px) **não** deve fechar.
- Cursor some enquanto roda, volta ao fechar.
- `log.txt` tem `saver encerrou`.

- [ ] **Step 5: Rodar os testes unitários**

```sh
mingw32-make -f build/Makefile test
```
Esperado: `all tests passed`.

- [ ] **Step 6: Commit**

```sh
git add src/host_win32.c src/main.c
git commit -m "$(printf 'feat: saver mode with per-monitor fullscreen windows and input-exit\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 6: Modo Preview — janela filha da mini-tela, sai quando o pai morre

**Files:**
- Modify: `src/host_win32.c` (implementar `host_run_preview`)
- Modify: `src/main.c` (provisório: despachar saver/preview conforme a linha de comando)

**Interfaces:**
- Consumes: `GlWindow` e helpers do Task 4; `m3dt_cmd_parse` do Task 2.
- Produces: `int host_run_preview(HINSTANCE, HWND parent)` — cria uma `WS_CHILD` dentro de `parent`, renderiza a cor animada, acompanha o resize do pai, retorna 0 assim que `IsWindow(parent)` for falso.

- [ ] **Step 1: Implementar `host_run_preview` em `src/host_win32.c`**

Substituir o stub por:
```c
/* ---------------- modo preview ---------------- */

int host_run_preview(HINSTANCE hInst, HWND parent)
{
    if (!IsWindow(parent)) { log_errorf("preview: parent inválido"); return 1; }

    m3dt_set_dpi_aware();
    m3dt_wgl_bootstrap(hInst);

    RECT pr; GetClientRect(parent, &pr);
    GlWindow g;
    if (!gl_window_create(hInst, &g, WS_CHILD | WS_VISIBLE, 0, parent,
                          0, 0, pr.right, pr.bottom, L"M3DTPreview", DefWindowProcW))
        return 1;

    LARGE_INTEGER freq, start;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    for (;;) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!IsWindow(parent)) break;

        RECT c; GetClientRect(parent, &c);
        if (c.right != g.w || c.bottom != g.h) {
            SetWindowPos(g.hwnd, NULL, 0, 0, c.right, c.bottom, SWP_NOZORDER | SWP_NOACTIVATE);
            g.w = c.right; g.h = c.bottom;
        }

        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        double t = (double)(now.QuadPart - start.QuadPart) / (double)freq.QuadPart;
        gl_render_clear(&g, t);
        Sleep(16);
    }

    gl_window_destroy(&g);
    log_infof("preview encerrou");
    return 0;
}
```

- [ ] **Step 2: `src/main.c` provisório com dispatch parcial**

```c
#include <windows.h>
#include <shellapi.h>
#include "cmdline.h"
#include "host_win32.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR lpCmdLine, int nShow)
{
    (void)hPrev; (void)lpCmdLine; (void)nShow;

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    M3dtCmdLine cmd;
    m3dt_cmd_parse(argc, (const wchar_t *const *)argv, &cmd);
    LocalFree(argv);

    switch (cmd.mode) {
        case M3DT_MODE_SAVER:   return host_run_saver(hInst);
        case M3DT_MODE_PREVIEW: return host_run_preview(hInst, (HWND)(UINT_PTR)cmd.parent_hwnd);
        case M3DT_MODE_CONFIG:  return 0;   /* Task 7 */
    }
    return 0;
}
```

- [ ] **Step 3: Buildar**

```sh
for f in main cmdline util/log host_win32; do \
  gcc -std=c11 -municode -O0 -g -DDEBUG -Wall -Wextra -Isrc -c src/$f.c -o build/obj/$f.o || exit 1; done
gcc build/obj/main.o build/obj/cmdline.o build/obj/util/log.o build/obj/host_win32.o \
  -o dist/Modern3DText.scr -mwindows -municode -static -static-libgcc \
  -lopengl32 -lgdi32 -luser32 -lkernel32 -lwinmm -lshell32
```
Esperado: compila limpo.

- [ ] **Step 4: Verificação manual — preview dentro do diálogo do Windows**

1. Copiar o `.scr` para o sistema:
   ```sh
   cp dist/Modern3DText.scr "$WINDIR/System32/" 2>/dev/null || \
     powershell -Command "Start-Process cmd -Verb runAs -ArgumentList '/c copy /Y dist\\Modern3DText.scr %WINDIR%\\System32\\'"
   ```
2. Abrir as configurações de proteção de tela: `powershell -Command "rundll32 desk.cpl,InstallScreenSaver Modern3DText.scr"` ou pelo menu Iniciar → "Alterar proteção de tela".
3. Selecionar "Modern3DText" na lista.
4. Verificar: a **mini-tela** do diálogo mostra o fundo com cor animada.
5. Fechar o diálogo → o processo de preview some (checar no Gerenciador de Tarefas que não sobrou `Modern3DText.scr`).
6. `log.txt` tem `preview encerrou`.

- [ ] **Step 5: Verificação manual — vida do preview sem o diálogo real**

```sh
# roda o preview com um pai que fecha em 5 s (PowerShell cria uma janela temporária)
powershell -NoProfile -Command "$f=New-Object System.Windows.Forms.Form; $f.Text='pai'; $f.Show(); \
  $h=$f.Handle; Start-Process 'dist\Modern3DText.scr' -ArgumentList \"/p $h\"; Start-Sleep 5; $f.Close()"
```
Esperado: janelinha filha renderiza por 5 s, e ao fechar o "pai" o `Modern3DText.scr` encerra sozinho (sem processo órfão).

- [ ] **Step 6: Testes unitários**

```sh
mingw32-make -f build/Makefile test
```
Esperado: `all tests passed`.

- [ ] **Step 7: Commit**

```sh
git add src/host_win32.c src/main.c
git commit -m "$(printf 'feat: preview mode as child window with parent-lifetime tracking\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 7: Modo Config — diálogo mínimo + recurso `.rc` + manifest

**Files:**
- Create: `src/resource.h`
- Create: `src/config_dialog.h`
- Create: `src/config_dialog.c`
- Create: `res/screensaver.rc`
- Create: `res/screensaver.manifest`
- Modify: `build/Makefile` (`SRC` ganha `src/config_dialog.c`; a regra `$(RES)` agora resolve)

**Interfaces:**
- Consumes: `log_infof`.
- Produces: `int config_dialog_run(HINSTANCE hInst, HWND parent)` — abre um diálogo modal (dono = `parent` se for janela válida, senão sem dono), retorna 0 ao fechar em OK ou Cancelar.

- [ ] **Step 1: Escrever `src/resource.h`**

```c
#ifndef M3DT_RESOURCE_H
#define M3DT_RESOURCE_H

#define IDD_CONFIG   101
#define IDC_INTRO    1001

#endif
```

- [ ] **Step 2: Escrever `res/screensaver.manifest`**

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity type="win32" name="Modern3DText" version="0.1.0.0"/>
  <dependency>
    <dependentAssembly>
      <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls"
        version="6.0.0.0" processorArchitecture="*"
        publicKeyToken="6595b64144ccf1df" language="*"/>
    </dependentAssembly>
  </dependency>
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">permonitorv2</dpiAwareness>
    </windowsSettings>
  </application>
</assembly>
```

- [ ] **Step 3: Escrever `res/screensaver.rc`**

```rc
#include <windows.h>
#include "resource.h"

1 24 "screensaver.manifest"

IDD_CONFIG DIALOGEX 0, 0, 260, 120
STYLE DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU
CAPTION "Modern 3D Text"
FONT 9, "Segoe UI", 400, 0, 0x1
BEGIN
    LTEXT           "Modern 3D Text\nConfigura\x00e7\x00e3o em constru\x00e7\x00e3o (Fase 1).",
                    IDC_INTRO, 14, 16, 232, 24
    DEFPUSHBUTTON   "OK",       IDOK,     150, 96, 46, 15
    PUSHBUTTON      "Cancelar", IDCANCEL, 202, 96, 46, 15
END

VS_VERSION_INFO VERSIONINFO
 FILEVERSION    0,1,0,0
 PRODUCTVERSION 0,1,0,0
 FILEOS         VOS_NT_WINDOWS32
 FILETYPE       VFT_APP
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904b0"
    BEGIN
      VALUE "CompanyName",      "Alan Maziero"
      VALUE "FileDescription",  "Modern 3D Text Screensaver"
      VALUE "FileVersion",      "0.1.0.0"
      VALUE "InternalName",     "Modern3DText"
      VALUE "OriginalFilename", "Modern3DText.scr"
      VALUE "ProductName",      "Modern 3D Text"
      VALUE "ProductVersion",   "0.1.0.0"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x409, 1200
  END
END
```

- [ ] **Step 4: Escrever `src/config_dialog.h`**

```c
#ifndef M3DT_CONFIG_DIALOG_H
#define M3DT_CONFIG_DIALOG_H

#include <windows.h>

int config_dialog_run(HINSTANCE hInst, HWND parent);

#endif
```

- [ ] **Step 5: Escrever `src/config_dialog.c`**

```c
#include "config_dialog.h"
#include "resource.h"
#include "util/log.h"

#include <commctrl.h>

static INT_PTR CALLBACK dlg_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG:
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL) {
                EndDialog(h, (INT_PTR)LOWORD(w));
                return TRUE;
            }
            break;
        case WM_CLOSE:
            EndDialog(h, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

int config_dialog_run(HINSTANCE hInst, HWND parent)
{
    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    HWND owner = IsWindow(parent) ? parent : NULL;
    log_infof("config: abrindo diálogo (owner=%p)", (void *)owner);

    DialogBoxParamW(hInst, MAKEINTRESOURCEW(IDD_CONFIG), owner, dlg_proc, 0);
    return 0;
}
```

- [ ] **Step 6: Atualizar `build/Makefile`**

Trocar a linha `SRC` para:
```make
SRC := src/main.c src/cmdline.c src/util/log.c src/host_win32.c src/config_dialog.c
```

- [ ] **Step 7: Buildar via `windres` + gcc**

```sh
windres --include-dir res --include-dir src -O coff res/screensaver.rc -o build/obj/screensaver.res
for f in main cmdline util/log host_win32 config_dialog; do \
  gcc -std=c11 -municode -O0 -g -DDEBUG -Wall -Wextra -Isrc -c src/$f.c -o build/obj/$f.o || exit 1; done
gcc build/obj/main.o build/obj/cmdline.o build/obj/util/log.o build/obj/host_win32.o \
  build/obj/config_dialog.o build/obj/screensaver.res \
  -o dist/Modern3DText.scr -mwindows -municode -static -static-libgcc \
  -lopengl32 -lgdi32 -luser32 -lkernel32 -lwinmm -lshell32 -lcomctl32
```
Esperado: `windres` sem erro; link limpo.

- [ ] **Step 8: `src/main.c` — ligar o modo config**

```c
        case M3DT_MODE_CONFIG:
            return config_dialog_run(hInst, (HWND)(UINT_PTR)cmd.parent_hwnd);
```
(adicionar `#include "config_dialog.h"` no topo).

- [ ] **Step 9: Verificação manual**

```sh
./dist/Modern3DText.scr /c
./dist/Modern3DText.scr
```
Verificar (ambos os comandos):
- Abre um diálogo centralizado, fonte Segoe UI, com o texto de "em construção", botões **OK** e **Cancelar**.
- `Esc` e **Cancelar** fecham; **OK** fecha; o "X" fecha.
- Em 150 %/200 % DPI o diálogo escala sem cortar texto (o manifest per-monitor v2 cuida disso).
- Testar também pelo diálogo real do Windows: botão **Configurações...** → deve abrir este diálogo (parented à janela de configurações).

- [ ] **Step 10: Testes unitários**

```sh
mingw32-make -f build/Makefile test
```
Esperado: `all tests passed`.

- [ ] **Step 11: Commit**

```sh
git add src/resource.h src/config_dialog.h src/config_dialog.c res/ build/Makefile src/main.c
git commit -m "$(printf 'feat: minimal /c config dialog with manifest and version info\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
```

---

## Task 8: Integração final — dispatch completo, `make` verde, checklist de fase

**Files:**
- Modify: `src/main.c` (versão final: log + parse + dispatch + shutdown)
- Modify: `build/Makefile` (garantir que `all`/`release`/`debug`/`run`/`config`/`test` funcionam de ponta a ponta)
- Create: `docs/qa-checklist-phase-1.md`

**Interfaces:**
- Consumes: tudo dos Tasks 2–7.
- Produces: `mingw32-make -f build/Makefile` gera `dist/Modern3DText.scr` (≤ 3 MB) sem passos manuais; `mingw32-make -f build/Makefile test` fica verde.

- [ ] **Step 1: `src/main.c` final**

```c
#include <windows.h>
#include <shellapi.h>

#include "cmdline.h"
#include "host_win32.h"
#include "config_dialog.h"
#include "util/log.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR lpCmdLine, int nShow)
{
    (void)hPrev; (void)lpCmdLine; (void)nShow;

    log_init();

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    M3dtCmdLine cmd;
    m3dt_cmd_parse(argc, (const wchar_t *const *)argv, &cmd);
    LocalFree(argv);

    log_infof("start mode=%d parent=%llu", (int)cmd.mode, cmd.parent_hwnd);

    int rc = 0;
    switch (cmd.mode) {
        case M3DT_MODE_SAVER:
            rc = host_run_saver(hInst);
            break;
        case M3DT_MODE_PREVIEW:
            rc = host_run_preview(hInst, (HWND)(UINT_PTR)cmd.parent_hwnd);
            break;
        case M3DT_MODE_CONFIG:
            rc = config_dialog_run(hInst, (HWND)(UINT_PTR)cmd.parent_hwnd);
            break;
    }

    log_infof("exit rc=%d", rc);
    log_shutdown();
    return rc;
}
```

- [ ] **Step 2: Verificar o Makefile de ponta a ponta**

```sh
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile
```
Esperado: compila todos os `.c`, roda `windres`, linka, imprime `Built dist/Modern3DText.scr (<N> bytes)` com `N < 3145728`. Sem warnings (`-Wall -Wextra`).

- [ ] **Step 3: Verificar todos os alvos**

```sh
mingw32-make -f build/Makefile clean && mingw32-make -f build/Makefile debug
mingw32-make -f build/Makefile test
./dist/Modern3DText.scr /c        # diálogo
./dist/Modern3DText.scr /s        # saver, sai no input
```
Esperado: `test` → `all tests passed`; os dois modos funcionam.

- [ ] **Step 4: Escrever `docs/qa-checklist-phase-1.md`**

```markdown
# QA manual — Fase 1 (esqueleto do .scr)

Buildar: `mingw32-make -f build/Makefile` (shell do w64devkit).

## Instalação de teste
- [ ] Copiar `dist/Modern3DText.scr` para `C:\Windows\System32\` (admin).
- [ ] "Modern3DText" aparece na lista de proteções de tela do Windows.

## /c (config)
- [ ] `Modern3DText.scr /c` abre o diálogo; OK / Cancelar / Esc / X fecham.
- [ ] Botão "Configurações..." no diálogo do Windows abre este diálogo.
- [ ] 100 % / 150 % / 200 % DPI: sem corte de texto.

## /p (preview)
- [ ] A mini-tela do diálogo do Windows mostra a cor animada.
- [ ] Fechar o diálogo não deixa `Modern3DText.scr` órfão (Gerenciador de Tarefas).

## /s (saver)
- [ ] Preenche todos os monitores, topmost, sem barra de título.
- [ ] Multi-monitor com resoluções diferentes: cada tela cobre 100 %.
- [ ] Sai ao: mover o mouse > ~4 px · tecla · clique · roda.
- [ ] Tremor de 1–2 px NÃO encerra.
- [ ] Cursor some ao rodar, volta ao sair.

## Robustez
- [ ] `%LOCALAPPDATA%\Modern3DText\log.txt` registra start/mode/exit e a versão do contexto GL.
- [ ] Rodar `/s` 20×seguidas: sem vazamento de processo/handle (Process Explorer).
- [ ] VM sem GPU dedicada (se disponível): cai para contexto 2.1 ou legado e ainda limpa a tela / sai no input.
```

- [ ] **Step 5: Rodar o checklist de QA e marcar os itens**

Executar cada item de `docs/qa-checklist-phase-1.md`. Anotar qualquer falha como um novo task de correção antes de fechar a fase.

- [ ] **Step 6: Commit + tag**

```sh
git add src/main.c build/Makefile docs/qa-checklist-phase-1.md
git commit -m "$(printf 'feat: complete /s /p /c dispatch; phase 1 skeleton done\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>')"
git tag -a v0.1.0-phase1 -m "Fase 1: esqueleto do .scr (dispatch, saver por monitor, preview, config stub)"
```

---

## Self-Review (feito na redação deste plano)

**1. Cobertura do spec (recorte da Fase 1 conforme §17 item 1 do spec):**
- "cmdline + host_win32 (janela saver por monitor, contexto GL 3.3, watcher de input, saída limpa)" → Tasks 2, 4, 5. ✓
- "+ /p" → Task 6. ✓
- "+ /c abrindo um diálogo vazio" → Task 7. ✓
- "Render: só um glClear colorido" → Task 4 (`gl_render_clear`). ✓
- "Testes: cmdline" → Task 2. ✓ (extra: `log_format_line` no Task 3).
- "Entregável: instala, 'protege a tela', sai no input" → Task 8 + `docs/qa-checklist-phase-1.md`. ✓
- Global constraints do spec cobertos: toolchain w64devkit (pré-requisito + `toolchain.txt`), C11/flags (Makefile), libs de link permitidas (Makefile `LIBS`), fallback de contexto 3.3→3.1→2.1→legado (Task 4 Step 2), `%LOCALAPPDATA%\Modern3DText\` só para log (Task 3), sem escrita em outros lugares (nenhum `RegSetValue`/`fopen` fora disso nesta fase), checagem de 3 MB (Makefile), atribuição nos commits (todos os Steps de commit). ✓
- Fora do escopo da Fase 1 e corretamente ausentes: geometria, shaders, `glad`, config real/registro, i18n, presets, fallback GDI 2D, instalador. Entram nas Fases 2–8.

**2. Varredura de placeholders:** sem "TBD"/"TODO"/"implementar depois". Todo Step de código traz o código real. As partes não testáveis por unidade (janela/GL) têm passos de verificação manual concretos com comandos e resultado esperado. O único "provisório" explícito é o `src/main.c` que evolui a cada task — cada versão está escrita por inteiro.

**3. Consistência de tipos:**
- `M3dtCmdLine { M3dtRunMode mode; unsigned long long parent_hwnd; }` — idêntico em `cmdline.h` (Task 2), no teste (Task 2), e no uso em `main.c` (Tasks 6, 8): `(HWND)(UINT_PTR)cmd.parent_hwnd`. ✓
- `M3DT_MODE_CONFIG/SAVER/PREVIEW` = 0/1/2 — usados como inteiros nos `EXPECT_EQ_INT` e no `switch`. ✓
- `GlWindow { HWND hwnd; HDC dc; HGLRC rc; int w, h; }` — definido no Task 4, consumido nos Tasks 5 e 6 com os mesmos campos (`g.w`, `g.h`, `g.hwnd`, `g.dc`). ✓
- `gl_window_create(HINSTANCE, GlWindow*, DWORD style, DWORD exstyle, HWND parent, int x, int y, int w, int h, const wchar_t* cls, WNDPROC proc)` — assinatura fixada no bloco Interfaces do Task 4 e chamada exatamente assim nos Tasks 4 (smoke), 5 (`saver_wndproc`), 6 (`DefWindowProcW`). ✓
- `host_run_saver(HINSTANCE)` / `host_run_preview(HINSTANCE, HWND)` — declarados em `host_win32.h` (Task 4), definidos como stub no Task 4, implementados nos Tasks 5/6, chamados em `main.c` (Tasks 5/6/8). ✓
- `config_dialog_run(HINSTANCE, HWND)` — `config_dialog.h` (Task 7), chamado em `main.c` (Tasks 7 Step 8, 8). ✓
- `log_init/shutdown/infof/errorf/format_line` — `log.h` (Task 3), usados em `host_win32.c`, `config_dialog.c`, `main.c`. ✓
- IDs de recurso `IDD_CONFIG=101`, `IDC_INTRO=1001` — `resource.h` (Task 7), usados no `.rc` e em `config_dialog.c`. ✓

Nenhuma inconsistência encontrada.

---

## Execution Handoff

**Plano completo e salvo em `docs/superpowers/plans/2026-09-10-modern-3d-text-phase-1-scr-skeleton.md`. Duas opções de execução:**

**1. Subagent-Driven (recomendado)** — despacho um subagente novo por task, reviso entre tasks, iteração rápida.

**2. Inline Execution** — executo os tasks nesta sessão com a skill executing-plans, em lotes com checkpoints de revisão.

**Qual abordagem?**

> Observação para ambas: a compilação e as verificações manuais precisam rodar **no shell do w64devkit** (Windows, GPU real para os Tasks 4–6). Se a execução for por subagente/sessão sem acesso a esse shell, os Steps de build/verificação ficam como instruções para você rodar e reportar o resultado.
