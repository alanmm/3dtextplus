# Fase 8b-5 — Partículas: Valor Padrão por Tipo — Plano de Implementação

> **Nota pós-execução:** as Tasks 1 e 2 abaixo foram executadas como
> escrito e depois **corrigidas** após verificação com captura real
> revelar que o design de "4 flags + 1 slot compartilhado" não
> preserva edições manuais entre os 4 tipos (ver a correção registrada
> no spec, seção 3-bis). O código final substitui as 4 flags
> `particles_*_customized` por 12 campos `float` de memória por tipo —
> commit `837faa2`. As tasks abaixo ficam como registro do que foi
> planejado originalmente.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ao trocar o tipo de partícula (Poeira/Bokeh/Faíscas/Estrelas) pela
primeira vez, aplicar automaticamente os valores de Densidade/Tamanho/
Opacidade ideais para aquele tipo — sem nunca sobrescrever um ajuste manual
já feito pelo usuário.

**Architecture:** 4 novos campos booleanos na `Config`, um por tipo de
partícula (`particles_dust_customized`/`bokeh_customized`/
`sparks_customized`/`stars_customized`). Uma função
`particles_apply_kind_default()` chamada no handler `CBN_SELCHANGE` do
combo `IDC_PARTKIND` aplica os 4 valores-alvo do tipo recém-selecionado
somente se a flag daquele tipo ainda estiver zerada, e atualiza os
sliders/rótulos na tela. O handler `WM_HSCROLL` (que já é compartilhado
pelos 4 sliders desta aba) marca a flag do tipo ATUALMENTE selecionado.
`particles_speed` (Velocidade) fica fora do mecanismo — é `0.76` para os
4 tipos, não há conflito a resolver.

**Tech Stack:** C (C99), Win32 API (registro HKCU, controles de diálogo),
`build/Makefile` (mingw32-make / w64devkit).

## Global Constraints

- Padrão de nomenclatura do registro: `snprintf`-free, chaves em
  `snake_case` minúsculo, mesmo nome do campo da `Config` (ex.:
  `particles_dust_customized`).
- Todo novo campo booleano segue o padrão já usado no projeto: default
  `0` em `config_defaults()`, `reg_get_i()` em `config_load_from()`,
  clamp `c->x = c->x ? 1 : 0;` em sanitize, `set_f(k, L"x", (float)c->x)`
  em `config_save_to()`.
- `mingw32-make -f build/Makefile clean` é obrigatório após qualquer
  edição em `.h` (o Makefile não rastreia dependências de header).
- Após cada build real (`release`), copiar
  `cp -f dist/Modern3DText.scr dist/Modern3DText.exe` antes de testar.
- Testar via captura de tela real exige aviso prévio ao usuário
  (PushNotification, senão mensagem de chat + `AskUserQuestion`
  aguardando confirmação) antes de qualquer `/s` ou `/c`.

---

### Task 1: Novos campos `Config` para "customizado por tipo"

**Files:**
- Modify: `src/config.h:69-71` (struct `Config`, logo após
  `bg_gradient_customized`)
- Modify: `src/config.c:77-79` (`config_defaults`)
- Modify: `src/config.c:231-232` (`config_load_from`)
- Modify: `src/config.c:314-315` (sanitize, dentro de `config_load_from`)
- Modify: `src/config.c:406-407` (`config_save_to`)
- Test: `build/tests/test_config.c`

**Interfaces:**
- Produces: 4 campos `int` na `Config` — `particles_dust_customized`,
  `particles_bokeh_customized`, `particles_sparks_customized`,
  `particles_stars_customized` — usados pela Task 2.

- [ ] **Step 1: Adicionar os 4 campos ao struct `Config`**

Em `src/config.h`, logo após a linha
`int bg_gradient_customized; /* ... */` (linha 71) e antes de `} Config;`:

```c
    int          particles_dust_customized;   /* 0/1 - Densidade/Tamanho/Opacidade do tipo Poeira ja foram ajustados manualmente */
    int          particles_bokeh_customized;  /* 0/1 - idem, tipo Bokeh */
    int          particles_sparks_customized; /* 0/1 - idem, tipo Faiscas */
    int          particles_stars_customized;  /* 0/1 - idem, tipo Estrelas */
```

- [ ] **Step 2: Inicializar os defaults em `config_defaults()`**

Em `src/config.c`, logo após `c->bg_gradient_customized = 0;` (linha 79):

```c
    c->particles_dust_customized = 0;
    c->particles_bokeh_customized = 0;
    c->particles_sparks_customized = 0;
    c->particles_stars_customized = 0;
```

- [ ] **Step 3: Ler os campos em `config_load_from()`**

Em `src/config.c`, logo após
`reg_get_i(k, L"bg_gradient_customized", &c->bg_gradient_customized);`
(linha 232):

```c
    reg_get_i(k, L"particles_dust_customized", &c->particles_dust_customized);
    reg_get_i(k, L"particles_bokeh_customized", &c->particles_bokeh_customized);
    reg_get_i(k, L"particles_sparks_customized", &c->particles_sparks_customized);
    reg_get_i(k, L"particles_stars_customized", &c->particles_stars_customized);
```

- [ ] **Step 4: Sanitizar os campos (clamp para 0/1)**

Ainda em `config_load_from()`, logo após
`c->bg_gradient_customized = c->bg_gradient_customized ? 1 : 0;`
(linha 315):

```c
    c->particles_dust_customized = c->particles_dust_customized ? 1 : 0;
    c->particles_bokeh_customized = c->particles_bokeh_customized ? 1 : 0;
    c->particles_sparks_customized = c->particles_sparks_customized ? 1 : 0;
    c->particles_stars_customized = c->particles_stars_customized ? 1 : 0;
```

- [ ] **Step 5: Salvar os campos em `config_save_to()`**

Em `src/config.c`, logo após
`set_f(k, L"bg_gradient_customized", (float)c->bg_gradient_customized);`
(linha 407):

```c
    set_f(k, L"particles_dust_customized", (float)c->particles_dust_customized);
    set_f(k, L"particles_bokeh_customized", (float)c->particles_bokeh_customized);
    set_f(k, L"particles_sparks_customized", (float)c->particles_sparks_customized);
    set_f(k, L"particles_stars_customized", (float)c->particles_stars_customized);
```

- [ ] **Step 6: Atualizar o teste de round-trip em `test_config.c`**

Na função de round-trip (mesmo bloco onde `a.bg_solid_customized = 1;` e
`a.bg_gradient_customized = 1;` já aparecem, por volta da linha 134-135),
adicionar logo depois:

```c
    a.particles_dust_customized = 1;
    a.particles_bokeh_customized = 1;
    a.particles_sparks_customized = 0;
    a.particles_stars_customized = 1;
```

E, no bloco de `EXPECT` que lê de volta (logo após
`EXPECT(b.bg_gradient_customized == 1);`, por volta da linha 197):

```c
    EXPECT(b.particles_dust_customized == 1);
    EXPECT(b.particles_bokeh_customized == 1);
    EXPECT(b.particles_sparks_customized == 0);
    EXPECT(b.particles_stars_customized == 1);
```

- [ ] **Step 7: Atualizar o teste de defaults em `test_config.c`**

Logo após `EXPECT(d.bg_gradient_customized == 0);` (linha 58), adicionar:

```c
    EXPECT(d.particles_dust_customized == 0);
    EXPECT(d.particles_bokeh_customized == 0);
    EXPECT(d.particles_sparks_customized == 0);
    EXPECT(d.particles_stars_customized == 0);
```

- [ ] **Step 8: Atualizar o teste de clamp (valores crus invalidos)**

No array `kv[]` de `test_config.c` (por volta da linha 269-289), logo
após `{ L"bg_gradient_customized", L"7" },`, adicionar 4 entradas:

```c
            { L"particles_dust_customized", L"5" },
            { L"particles_bokeh_customized", L"7" },
            { L"particles_sparks_customized", L"5" },
            { L"particles_stars_customized", L"7" },
```

Atualizar o loop `for (int i = 0; i < 30; ++i)` (linha 290) para
`for (int i = 0; i < 34; ++i)`.

Logo após `EXPECT(e.bg_gradient_customized == 1);` (linha 326),
adicionar:

```c
    EXPECT(e.particles_dust_customized == 1);    /* 5 -> !=0 -> 1 */
    EXPECT(e.particles_bokeh_customized == 1);    /* 7 -> !=0 -> 1 */
    EXPECT(e.particles_sparks_customized == 1);   /* 5 -> !=0 -> 1 */
    EXPECT(e.particles_stars_customized == 1);    /* 7 -> !=0 -> 1 */
```

- [ ] **Step 9: Build limpo e rodar os testes**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```

Expected: build sem warnings/erros, todos os testes de `test_config`
passando (incluindo os novos `EXPECT`s acima).

- [ ] **Step 10: Commit**

```bash
git add src/config.h src/config.c build/tests/test_config.c
git commit -m "$(cat <<'EOF'
feat: add per-particle-kind customized flags to Config

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Aplicar valores padrão por tipo na aba Partículas

**Files:**
- Modify: `src/config_dialog.c:1297-1376` (seção Partículas: `particles_labels`,
  `particles_proc`)

**Interfaces:**
- Consumes: os 4 campos `Config` da Task 1
  (`particles_dust_customized` etc.), `g_work` (variável global `Config`
  já existente em todo o arquivo), `set_slider(HWND h, int id, int lo,
  int hi, int pos)` (já declarada em `config_dialog.c:160`),
  `particles_labels(HWND h)` (já existe em `config_dialog.c:1297`),
  `preview_dirty(HWND child)` (já existe em `config_dialog.c:120`).
- Produces: `static void particles_apply_kind_default(HWND h)`, chamada
  de dentro de `particles_proc`.

- [ ] **Step 1: Escrever `particles_apply_kind_default()`**

Em `src/config_dialog.c`, logo antes de `static INT_PTR CALLBACK
particles_proc(...)` (linha 1337), adicionar:

```c
static void particles_apply_kind_default(HWND h)
{
    switch (g_work.particles_kind) {
        case 0: /* Poeira */
            if (g_work.particles_dust_customized) return;
            g_work.particles_density = 0.43f;
            g_work.particles_speed = 0.76f;
            g_work.particles_size_scale = 0.80f;
            g_work.particles_opacity = 0.50f;
            break;
        case 1: /* Bokeh */
            if (g_work.particles_bokeh_customized) return;
            g_work.particles_density = 0.43f;
            g_work.particles_speed = 0.76f;
            g_work.particles_size_scale = 0.81f;
            g_work.particles_opacity = 0.42f;
            break;
        case 2: /* Faiscas */
            if (g_work.particles_sparks_customized) return;
            g_work.particles_density = 0.42f;
            g_work.particles_speed = 0.76f;
            g_work.particles_size_scale = 0.72f;
            g_work.particles_opacity = 0.42f;
            break;
        case 3: /* Estrelas */
            if (g_work.particles_stars_customized) return;
            g_work.particles_density = 0.90f;
            g_work.particles_speed = 0.76f;
            g_work.particles_size_scale = 0.70f;
            g_work.particles_opacity = 0.78f;
            break;
        default:
            return;
    }
    set_slider(h, IDC_PARTDENS, 0, 100, (int)(g_work.particles_density * 100.0f + 0.5f));
    set_slider(h, IDC_PARTSPEED, 0, 200, (int)(g_work.particles_speed * 100.0f + 0.5f));
    set_slider(h, IDC_PARTSIZE, 0, 200, (int)(g_work.particles_size_scale * 100.0f + 0.5f));
    set_slider(h, IDC_PARTOPACITY, 0, 200, (int)(g_work.particles_opacity * 100.0f + 0.5f));
    particles_labels(h);
}
```

Nota: a função retorna cedo (sem tocar sliders) quando a flag do tipo já
está marcada — preserva exatamente o ajuste manual do usuário.

- [ ] **Step 2: Chamar a função no handler `CBN_SELCHANGE` de `IDC_PARTKIND`**

Em `particles_proc`, dentro do `case IDC_PARTKIND:` (linha 1366-1371),
mudar de:

```c
                case IDC_PARTKIND:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.particles_kind = (int)SendDlgItemMessageW(h, IDC_PARTKIND, CB_GETCURSEL, 0, 0);
                        preview_dirty(h);
                    }
                    break;
```

para:

```c
                case IDC_PARTKIND:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.particles_kind = (int)SendDlgItemMessageW(h, IDC_PARTKIND, CB_GETCURSEL, 0, 0);
                        particles_apply_kind_default(h);
                        preview_dirty(h);
                    }
                    break;
```

- [ ] **Step 3: Marcar a flag "customizado" do tipo atual no `WM_HSCROLL`**

Em `particles_proc`, no `case WM_HSCROLL:` (linhas 1351-1358), mudar de:

```c
        case WM_HSCROLL:
            g_work.particles_density    = (float)SendDlgItemMessageW(h, IDC_PARTDENS, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.particles_speed      = (float)SendDlgItemMessageW(h, IDC_PARTSPEED, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.particles_size_scale = (float)SendDlgItemMessageW(h, IDC_PARTSIZE, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.particles_opacity    = (float)SendDlgItemMessageW(h, IDC_PARTOPACITY, TBM_GETPOS, 0, 0) / 100.0f;
            particles_labels(h);
            preview_dirty(h);
            return TRUE;
```

para:

```c
        case WM_HSCROLL:
            g_work.particles_density    = (float)SendDlgItemMessageW(h, IDC_PARTDENS, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.particles_speed      = (float)SendDlgItemMessageW(h, IDC_PARTSPEED, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.particles_size_scale = (float)SendDlgItemMessageW(h, IDC_PARTSIZE, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.particles_opacity    = (float)SendDlgItemMessageW(h, IDC_PARTOPACITY, TBM_GETPOS, 0, 0) / 100.0f;
            switch (g_work.particles_kind) {
                case 0: g_work.particles_dust_customized = 1; break;
                case 1: g_work.particles_bokeh_customized = 1; break;
                case 2: g_work.particles_sparks_customized = 1; break;
                case 3: g_work.particles_stars_customized = 1; break;
            }
            particles_labels(h);
            preview_dirty(h);
            return TRUE;
```

- [ ] **Step 4: Build limpo e checagem de warnings**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile release
```

Expected: build sem warnings/erros.

```bash
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

- [ ] **Step 5: Verificação visual real — cada tipo do zero**

Fazer backup do registro se `HKCU\Software\Modern3DText` existir
(`reg export` para um `.reg` temporário), depois limpar a chave para
simular instalação limpa. Avisar o usuário (PushNotification, senão
chat + `AskUserQuestion` aguardando confirmação) antes de capturar.

Abrir o config dialog (`/c`), ir para a aba Partículas, e para cada um
dos 4 tipos (Poeira/Bokeh/Faíscas/Estrelas) selecioná-lo via simulação
de UI real (`CB_SETCURSEL` + `WM_COMMAND`/`CBN_SELCHANGE`, técnica já
usada nas fases anteriores — não apenas presetar o registro e reabrir,
que só exercitaria `WM_INITDIALOG`) e capturar a tela, conferindo que
os 4 sliders/rótulos batem com a tabela da Task 1 (ex.: Estrelas deve
mostrar Densidade 0.90, Opacidade 0.78).

- [ ] **Step 6: Verificação visual real — preservação de ajuste manual**

Ainda na mesma sessão do dialog: selecionar Poeira, mexer manualmente o
slider de Densidade para um valor diferente do padrão (ex.: 0.20) via
`TBM_SETPOS` + `WM_HSCROLL`, trocar para Bokeh (via `CB_SETCURSEL` +
`CBN_SELCHANGE`) e depois voltar para Poeira (mesma técnica). Capturar a
tela e confirmar que a Densidade de Poeira permanece 0.20 (não foi
resetada para 0.43).

- [ ] **Step 7: Restaurar o registro (se houve backup) e enviar o `.exe`**

Se um backup foi feito no Step 5, restaurar com `reg import`. Enviar
`dist/Modern3DText.exe` ao usuário via `SendUserFile`.

- [ ] **Step 8: Commit**

```bash
git add src/config_dialog.c
git commit -m "$(cat <<'EOF'
feat: apply per-kind particle defaults on first select, preserve edits

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)"
```
