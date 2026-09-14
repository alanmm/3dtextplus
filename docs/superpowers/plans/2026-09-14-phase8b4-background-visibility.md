# Fase 8b-4 — Fundo: Ocultar + Reposicionar + Cor 1 por Tipo — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A aba Fundo oculta/reposiciona os controles conforme o tipo,
e resolve o único conflito real de campo compartilhado (Cor 1, entre
Sólido e Gradiente).

**Architecture:** Mesma técnica de captura-e-reflow da Fase 8b-3
(`src/config_dialog.c`), com 4 blocos em vez de 3, substituindo
`bg_enable()` inteiramente. Mais 2 campos novos na `Config` só pra
Cor 1, e a correção pendente dos defaults da Nebulosa (adiada de
propósito na Fase 8b-1).

**Tech Stack:** C11, WinAPI.

## Global Constraints

- Build: `mingw32-make -f build/Makefile debug` (ou `release`) da
  raiz, com w64devkit no PATH. Testes: `mingw32-make -f build/Makefile
  test`. `mingw32-make -f build/Makefile clean` antes de builds depois
  de editar um `.h`.

---

### Task 1: Campos novos + correção dos defaults da Nebulosa

**Files:**
- Modify: `src/config.h`
- Modify: `src/config.c`
- Modify: `build/tests/test_config.c`

- [ ] **Step 1: Campos no struct**

Trocar:

```c
    int          ui_language;          /* 0 automatico, 1 portugues, 2 ingles */
} Config;
```

por:

```c
    int          ui_language;          /* 0 automatico, 1 portugues, 2 ingles */
    int          bg_solid_customized;   /* 0/1 - Cor 1 do fundo Solido ja foi ajustada manualmente */
    int          bg_gradient_customized; /* 0/1 - Cor 1 do fundo Gradiente ja foi ajustada manualmente */
} Config;
```

- [ ] **Step 2: Default, load, sanitize, save em `config.c`**

Trocar `c->ui_language = 0;` (em `config_defaults`) por:

```c
    c->ui_language = 0;
    c->bg_solid_customized = 0;
    c->bg_gradient_customized = 0;
```

E, no mesmo `config_defaults()`, trocar:

```c
    c->bg_neb_color1_r = 0.03f; c->bg_neb_color1_g = 0.02f; c->bg_neb_color1_b = 0.08f;
    c->bg_neb_color2_r = 0.25f; c->bg_neb_color2_g = 0.10f; c->bg_neb_color2_b = 0.35f;
```

por (valor de fábrica pendente da Fase 8b-1, sem conflito de campo
compartilhado, então não precisa de mecanismo nenhum):

```c
    c->bg_neb_color1_r = 0.02745f; c->bg_neb_color1_g = 0.01961f; c->bg_neb_color1_b = 0.07843f;
    c->bg_neb_color2_r = 0.24706f; c->bg_neb_color2_g = 0.09804f; c->bg_neb_color2_b = 0.34902f;
```

Trocar `reg_get_i(k, L"ui_language", &c->ui_language);` (em
`config_load_from`) por:

```c
    reg_get_i(k, L"ui_language", &c->ui_language);
    reg_get_i(k, L"bg_solid_customized", &c->bg_solid_customized);
    reg_get_i(k, L"bg_gradient_customized", &c->bg_gradient_customized);
```

Trocar `if (c->ui_language < 0 || c->ui_language > 2) c->ui_language = 0;`
(sanitize) por:

```c
    if (c->ui_language < 0 || c->ui_language > 2) c->ui_language = 0;
    c->bg_solid_customized = c->bg_solid_customized ? 1 : 0;
    c->bg_gradient_customized = c->bg_gradient_customized ? 1 : 0;
```

Trocar `set_f(k, L"ui_language", (float)c->ui_language);` (em
`config_save_to`) por:

```c
    set_f(k, L"ui_language", (float)c->ui_language);
    set_f(k, L"bg_solid_customized", (float)c->bg_solid_customized);
    set_f(k, L"bg_gradient_customized", (float)c->bg_gradient_customized);
```

- [ ] **Step 3: Testes em `test_config.c`**

No bloco "defaults" (início de `run_config_tests`), trocar:

```c
    EXPECT(nearf(d.bg_color1_r, 0.06275f) && nearf(d.bg_color1_g, 0.03922f) && nearf(d.bg_color1_b, 0.03922f));
    EXPECT(nearf(d.bg_color2_r, 0.04706f) && nearf(d.bg_color2_g, 0.07451f) && nearf(d.bg_color2_b, 0.13725f));
    EXPECT(nearf(d.bg_grad_angle, 101.0f));
```

por:

```c
    EXPECT(nearf(d.bg_color1_r, 0.06275f) && nearf(d.bg_color1_g, 0.03922f) && nearf(d.bg_color1_b, 0.03922f));
    EXPECT(nearf(d.bg_color2_r, 0.04706f) && nearf(d.bg_color2_g, 0.07451f) && nearf(d.bg_color2_b, 0.13725f));
    EXPECT(nearf(d.bg_grad_angle, 101.0f));
    EXPECT(nearf(d.bg_neb_color1_r, 0.02745f) && nearf(d.bg_neb_color1_g, 0.01961f) && nearf(d.bg_neb_color1_b, 0.07843f));
    EXPECT(nearf(d.bg_neb_color2_r, 0.24706f) && nearf(d.bg_neb_color2_g, 0.09804f) && nearf(d.bg_neb_color2_b, 0.34902f));
    EXPECT(d.bg_solid_customized == 0);
    EXPECT(d.bg_gradient_customized == 0);
```

No round-trip geral (par `a`/`b`), trocar:

```c
    a.ui_language = 2;
    config_save_to(&a, TESTKEY);
```

por:

```c
    a.ui_language = 2;
    a.bg_solid_customized = 1;
    a.bg_gradient_customized = 1;
    config_save_to(&a, TESTKEY);
```

E trocar:

```c
    EXPECT(b.ui_language == 2);
```

por:

```c
    EXPECT(b.ui_language == 2);
    EXPECT(b.bg_solid_customized == 1);
    EXPECT(b.bg_gradient_customized == 1);
```

No teste de clamp (`kv[]`), trocar:

```c
            { L"ui_language", L"9" },
        };
        for (int i = 0; i < 28; ++i)
```

por:

```c
            { L"ui_language", L"9" },
            { L"bg_solid_customized", L"5" },
            { L"bg_gradient_customized", L"7" },
        };
        for (int i = 0; i < 30; ++i)
```

E, depois de `EXPECT(e.ui_language == 0);`, adicionar:

```c
    EXPECT(e.bg_solid_customized == 1);      /* 5 -> !=0 -> 1 */
    EXPECT(e.bg_gradient_customized == 1);   /* 7 -> !=0 -> 1 */
```

- [ ] **Step 4: Build + testes**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile test
```

- [ ] **Step 5: Commit**

```bash
git add src/config.h src/config.c build/tests/test_config.c
git commit -m "feat: add bg_solid/gradient_customized fields, fix nebula defaults"
```

---

### Task 2: Ocultar + reposicionar + resolver Cor 1 na aba Fundo

**Files:**
- Modify: `src/config_dialog.c`

**Interfaces:** consome os 2 campos novos da Task 1.

- [ ] **Step 1: Estruturas de layout + captura, substituindo `bg_enable`**

Trocar:

```c
static void bg_enable(HWND h)
{
    int t = g_work.background_type;
    EnableWindow(GetDlgItem(h, IDC_BGCOLOR1), t == 0 || t == 1);
    EnableWindow(GetDlgItem(h, IDC_BGCOLOR2), t == 1);
    EnableWindow(GetDlgItem(h, IDC_BGANGLE), t == 1);
    EnableWindow(GetDlgItem(h, IDC_BGIMGPICK), t == 2);
    EnableWindow(GetDlgItem(h, IDC_BGIMGCLEAR), t == 2);
    EnableWindow(GetDlgItem(h, IDC_BGFIT), t == 2);
    EnableWindow(GetDlgItem(h, IDC_BGPAN), t == 2);
    EnableWindow(GetDlgItem(h, IDC_BGNEBCOLOR1), t == 3);
    EnableWindow(GetDlgItem(h, IDC_BGNEBCOLOR2), t == 3);
}
```

por:

```c
/* 4 blocos da aba Fundo, na ordem em que ja aparecem no .rc - Cor 1 e'
   compartilhada entre Solido e Gradiente (unico campo com conflito
   real de valor-alvo entre tipos, ver spec), os demais sao exclusivos
   de um tipo so */
typedef struct { int id; int x, rel_y; } BgCtrl;

static BgCtrl g_bg_blocks[4][9] = {
    { { IDC_BGCOLOR1_LABEL, 0, 0 }, { IDC_BGCOLOR1, 0, 0 } },
    { { IDC_BGCOLOR2_LABEL, 0, 0 }, { IDC_BGCOLOR2, 0, 0 },
      { IDC_BGANGLE_LABEL, 0, 0 }, { IDC_BGANGLE_VAL, 0, 0 }, { IDC_BGANGLE, 0, 0 } },
    { { IDC_BGIMAGE_LABEL, 0, 0 }, { IDC_BGIMGPATH, 0, 0 }, { IDC_BGIMGPICK, 0, 0 },
      { IDC_BGIMGCLEAR, 0, 0 }, { IDC_BGFIT_LABEL, 0, 0 }, { IDC_BGFIT, 0, 0 },
      { IDC_BGPAN_LABEL, 0, 0 }, { IDC_BGPAN_VAL, 0, 0 }, { IDC_BGPAN, 0, 0 } },
    { { IDC_BGNEBULA_LABEL, 0, 0 }, { IDC_BGNEBCOLOR1, 0, 0 }, { IDC_BGNEBCOLOR2, 0, 0 } },
};
static const int BG_BLOCK_N[4] = { 2, 5, 9, 3 };
static int g_bg_block_top[4];
static int g_bg_block_h[4];
static int g_bg_gap_after[3];
static int g_bg_layout_ready = 0;

static void bg_layout_capture(HWND h)
{
    if (g_bg_layout_ready) return;

    for (int b = 0; b < 4; ++b) {
        int top = 0x7fffffff, bottom = -0x7fffffff;
        for (int i = 0; i < BG_BLOCK_N[b]; ++i) {
            RECT r;
            HWND ctrl = GetDlgItem(h, g_bg_blocks[b][i].id);
            GetWindowRect(ctrl, &r);
            MapWindowPoints(NULL, h, (POINT *)&r, 2);
            g_bg_blocks[b][i].x = r.left;
            g_bg_blocks[b][i].rel_y = r.top;
            if (r.top < top) top = r.top;
            if (r.bottom > bottom) bottom = r.bottom;
        }
        g_bg_block_top[b] = top;
        g_bg_block_h[b] = bottom - top;
        for (int i = 0; i < BG_BLOCK_N[b]; ++i)
            g_bg_blocks[b][i].rel_y -= top;
    }
    for (int b = 0; b < 3; ++b)
        g_bg_gap_after[b] = g_bg_block_top[b + 1] - (g_bg_block_top[b] + g_bg_block_h[b]);
    g_bg_layout_ready = 1;
}

static void bg_layout_apply(HWND h)
{
    int t = g_work.background_type;
    int visible[4] = { t == 0 || t == 1, t == 1, t == 2, t == 3 };

    int cursor = g_bg_block_top[0];
    for (int b = 0; b < 4; ++b) {
        if (!visible[b]) {
            for (int i = 0; i < BG_BLOCK_N[b]; ++i)
                ShowWindow(GetDlgItem(h, g_bg_blocks[b][i].id), SW_HIDE);
            continue;
        }
        for (int i = 0; i < BG_BLOCK_N[b]; ++i) {
            HWND ctrl = GetDlgItem(h, g_bg_blocks[b][i].id);
            SetWindowPos(ctrl, NULL, g_bg_blocks[b][i].x, cursor + g_bg_blocks[b][i].rel_y,
                         0, 0, SWP_NOSIZE | SWP_NOZORDER);
            ShowWindow(ctrl, SW_SHOW);
        }
        cursor += g_bg_block_h[b] + (b < 3 ? g_bg_gap_after[b] : 0);
    }
}
```

- [ ] **Step 2: `WM_INITDIALOG` chama captura + apply em vez de `bg_enable`**

Trocar:

```c
        case WM_INITDIALOG: {
            set_slider(h, IDC_BGANGLE, 0, 360, (int)(g_work.bg_grad_angle + 0.5f));
            set_slider(h, IDC_BGPAN, 0, 100, (int)(g_work.bg_pan_speed * 100.0f + 0.5f));
            bg_apply_i18n(h);
            bg_enable(h);
            return TRUE;
        }
```

por:

```c
        case WM_INITDIALOG: {
            set_slider(h, IDC_BGANGLE, 0, 360, (int)(g_work.bg_grad_angle + 0.5f));
            set_slider(h, IDC_BGPAN, 0, 100, (int)(g_work.bg_pan_speed * 100.0f + 0.5f));
            bg_apply_i18n(h);
            bg_layout_capture(h);
            bg_layout_apply(h);
            return TRUE;
        }
```

- [ ] **Step 3: Trocar de tipo — reflow + resolver Cor 1**

Trocar:

```c
                case IDC_BGTYPE:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.background_type = (int)SendDlgItemMessageW(h, IDC_BGTYPE, CB_GETCURSEL, 0, 0);
                        bg_enable(h);
                        preview_dirty(h);
                    }
                    break;
```

por:

```c
                case IDC_BGTYPE:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.background_type = (int)SendDlgItemMessageW(h, IDC_BGTYPE, CB_GETCURSEL, 0, 0);
                        if (g_work.background_type == 0 && !g_work.bg_solid_customized) {
                            g_work.bg_color1_r = 0.05490f;
                            g_work.bg_color1_g = 0.04706f;
                            g_work.bg_color1_b = 0.04706f;
                        } else if (g_work.background_type == 1 && !g_work.bg_gradient_customized) {
                            g_work.bg_color1_r = 0.06275f;
                            g_work.bg_color1_g = 0.03922f;
                            g_work.bg_color1_b = 0.03922f;
                        }
                        bg_layout_apply(h);
                        preview_dirty(h);
                    }
                    break;
```

- [ ] **Step 4: Marcar customizado ao escolher Cor 1 manualmente**

Trocar:

```c
                case IDC_BGCOLOR1: {
                    static COLORREF custom[16];
                    CHOOSECOLORW cc;
                    memset(&cc, 0, sizeof cc);
                    cc.lStructSize = sizeof cc;
                    cc.hwndOwner = h;
                    cc.lpCustColors = custom;
                    cc.rgbResult = RGB((int)(g_work.bg_color1_r * 255.0f),
                                       (int)(g_work.bg_color1_g * 255.0f),
                                       (int)(g_work.bg_color1_b * 255.0f));
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColorW(&cc)) {
                        g_work.bg_color1_r = GetRValue(cc.rgbResult) / 255.0f;
                        g_work.bg_color1_g = GetGValue(cc.rgbResult) / 255.0f;
                        g_work.bg_color1_b = GetBValue(cc.rgbResult) / 255.0f;
                        preview_dirty(h);
                    }
                    break;
                }
```

por:

```c
                case IDC_BGCOLOR1: {
                    static COLORREF custom[16];
                    CHOOSECOLORW cc;
                    memset(&cc, 0, sizeof cc);
                    cc.lStructSize = sizeof cc;
                    cc.hwndOwner = h;
                    cc.lpCustColors = custom;
                    cc.rgbResult = RGB((int)(g_work.bg_color1_r * 255.0f),
                                       (int)(g_work.bg_color1_g * 255.0f),
                                       (int)(g_work.bg_color1_b * 255.0f));
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColorW(&cc)) {
                        g_work.bg_color1_r = GetRValue(cc.rgbResult) / 255.0f;
                        g_work.bg_color1_g = GetGValue(cc.rgbResult) / 255.0f;
                        g_work.bg_color1_b = GetBValue(cc.rgbResult) / 255.0f;
                        if (g_work.background_type == 0) g_work.bg_solid_customized = 1;
                        else if (g_work.background_type == 1) g_work.bg_gradient_customized = 1;
                        preview_dirty(h);
                    }
                    break;
                }
```

- [ ] **Step 5: Build de depuração + smoke test**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile debug
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

```powershell
$env:M3DT_SELFTEST = "1"
$env:M3DT_TAB = "7"
$env:M3DT_HOLD_MS = "3000"
$p = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p | Wait-Process -Timeout 8
Write-Output "exitcode: $($p.ExitCode)"
```

- [ ] **Step 6: Verificação visual real — os 4 tipos + o conflito de Cor 1**

**Aviso ao usuário**: tentar `PushNotification`; se falhar, mandar
mensagem de chat e **esperar confirmação explícita** antes de
capturar. Usar a técnica de captura real de tela (`.NET
Graphics.CopyFromScreen` via `EnumWindows` por PID) já validada nesta
sessão.

Capturar a aba Fundo (`M3DT_TAB=7`) nos 4 tipos (registro limpo antes,
sem `bg_solid_customized`/`bg_gradient_customized` definidos — ambos
começam em 0): confirmar que Sólido mostra só Cor 1, Gradiente mostra
Cor 1+Cor 2+Ângulo, Imagem mostra o bloco de imagem inteiro, Nebulosa
mostra as 2 cores — todos reposicionados sem vão nem sobreposição.
Depois, testar o cenário do conflito: com o registro limpo, selecionar
Sólido (deve assumir a cor 14,12,12) e depois Gradiente (deve assumir
16,10,10) sem nunca ter clicado em "Escolher cor..." — confirma que os
dois valores de fábrica aparecem corretamente na primeira visita a
cada um, mesmo compartilhando o mesmo campo.

- [ ] **Step 7: Build release + refresh do `.exe`**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile release
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

- [ ] **Step 8: Commit**

```bash
git add src/config_dialog.c
git commit -m "feat: hide and reposition Fundo tab controls by type, fix shared Cor 1 default"
```

---

## Self-Review

1. **Cobertura do spec**: tabela de visibilidade do §3 → `visible[4]`
   no Step 1 da Task 2; conflito de Cor 1 do §4 → Steps 3-4 da Task 2 +
   os 2 campos da Task 1; correção da Nebulosa do §2 → Task 1 Step 2.
2. **Placeholders**: nenhum.
3. **Consistência**: `BgCtrl`/`g_bg_blocks`/`BG_BLOCK_N` usados de
   forma idêntica entre `bg_layout_capture` e `bg_layout_apply`, mesmo
   padrão de `MatCtrl`/`g_mat_blocks` da Fase 8b-3 (nomes diferentes,
   confirmando que não virou uma "rule of three" ainda — só a segunda
   ocorrência, decisão consciente de não abstrair prematuramente,
   mesma lição já registrada neste projeto).
4. **Risco verificado à mão**: `bg_enable()` é removida por completo
   (substituída por `bg_layout_apply()`, que já cobre mostrar/ocultar
   E reposicionar) — confirmei que não há mais nenhuma chamada a
   `bg_enable` sobrando em `bg_proc` antes de escrever o plano.
