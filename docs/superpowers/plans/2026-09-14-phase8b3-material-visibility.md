# Fase 8b-3 — Material: Ocultar + Reposicionar por Modo — Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A aba Material oculta Metalização/Rugosidade/seção de
ambiente conforme o modo selecionado (só têm efeito real no Metálico
e, parcialmente, no Vidro — confirmado no shader), e os controles que
continuam visíveis se reposicionam pra preencher o espaço.

**Architecture:** Captura, uma única vez em `WM_INITDIALOG` (antes de
qualquer ocultação), a posição em pixels de cada controle da aba —
já resolvida pelo Win32 a partir do `.rc`, sem conversão manual de
unidade de diálogo. A partir daí, uma função reaplicável percorre 3
"blocos" (Metalização/Rugosidade/Ambiente) na ordem original,
escondendo os irrelevantes e empilhando os visíveis com o mesmo
espaçamento que o `.rc` já tinha entre eles.

**Tech Stack:** C11, WinAPI (`GetWindowRect`, `MapWindowPoints`,
`SetWindowPos`).

## Global Constraints

- Nenhum campo novo na `Config` (confirmado no spec §3 — `metalness`/
  `roughness` já compartilhados, sem necessidade de "customizado").
- Build: `mingw32-make -f build/Makefile debug` (ou `release`) da
  raiz, com w64devkit no PATH. Testes: `mingw32-make -f build/Makefile
  test`. `mingw32-make -f build/Makefile clean` antes de builds depois
  de editar um `.h`.

---

### Task 1: Reposicionamento dinâmico da aba Material

**Files:**
- Modify: `src/config_dialog.c`

**Interfaces:** nenhuma — tudo interno a `config_dialog.c`.

- [ ] **Step 1: Estruturas de layout + captura, logo antes de `material_proc`**

Trocar:

```c
static INT_PTR CALLBACK material_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            set_slider(h, IDC_METAL, 0, 100, (int)(g_work.metalness * 100.0f + 0.5f));
            set_slider(h, IDC_ROUGH, 0, 100, (int)(g_work.roughness * 100.0f + 0.5f));
            material_apply_i18n(h);
            return TRUE;
        }
```

por:

```c
/* 3 blocos da aba Material, na ordem em que ja aparecem no .rc -
   Metalizacao/Rugosidade so tem efeito no Metalico e (Rugosidade
   tambem) no Vidro, verificado direto no shaders/model.frag */
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

/* captura a posicao ORIGINAL (em pixels, ja resolvida do .rc) de cada
   controle - roda uma unica vez, antes de qualquer ocultacao, entao
   sempre reflete a disposicao estatica original do .rc */
static void material_layout_capture(HWND h)
{
    if (g_mat_layout_ready) return;

    for (int b = 0; b < 3; ++b) {
        int top = 0x7fffffff, bottom = -0x7fffffff;
        for (int i = 0; i < MAT_BLOCK_N[b]; ++i) {
            RECT r;
            HWND ctrl = GetDlgItem(h, g_mat_blocks[b][i].id);
            GetWindowRect(ctrl, &r);
            MapWindowPoints(NULL, h, (POINT *)&r, 2);
            g_mat_blocks[b][i].x = r.left;
            g_mat_blocks[b][i].rel_y = r.top;   /* vira relativo ao bloco no passo seguinte */
            if (r.top < top) top = r.top;
            if (r.bottom > bottom) bottom = r.bottom;
        }
        g_mat_block_top[b] = top;
        g_mat_block_h[b] = bottom - top;
        for (int i = 0; i < MAT_BLOCK_N[b]; ++i)
            g_mat_blocks[b][i].rel_y -= top;
    }
    g_mat_gap_after[0] = g_mat_block_top[1] - (g_mat_block_top[0] + g_mat_block_h[0]);
    g_mat_gap_after[1] = g_mat_block_top[2] - (g_mat_block_top[1] + g_mat_block_h[1]);
    g_mat_layout_ready = 1;
}

/* esconde os blocos irrelevantes pro modo atual e empilha os visiveis
   a partir do topo original do primeiro bloco, preservando o mesmo
   espacamento entre blocos que o .rc ja tinha */
static void material_layout_apply(HWND h)
{
    int vis_metal = (g_work.material_mode == 1);
    int vis_rough = (g_work.material_mode == 1 || g_work.material_mode == 2);
    int vis_env   = (g_work.material_mode == 1 || g_work.material_mode == 2);
    int visible[3] = { vis_metal, vis_rough, vis_env };

    int cursor = g_mat_block_top[0];
    for (int b = 0; b < 3; ++b) {
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
        cursor += g_mat_block_h[b] + (b < 2 ? g_mat_gap_after[b] : 0);
    }
}

static INT_PTR CALLBACK material_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            set_slider(h, IDC_METAL, 0, 100, (int)(g_work.metalness * 100.0f + 0.5f));
            set_slider(h, IDC_ROUGH, 0, 100, (int)(g_work.roughness * 100.0f + 0.5f));
            material_apply_i18n(h);
            material_layout_capture(h);
            material_layout_apply(h);
            return TRUE;
        }
```

- [ ] **Step 2: Reaplicar o layout ao trocar de modo**

Trocar:

```c
                case IDC_MATMODE:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.material_mode =
                            (int)SendDlgItemMessageW(h, IDC_MATMODE, CB_GETCURSEL, 0, 0);
                        preview_dirty(h);
                    }
                    break;
```

por:

```c
                case IDC_MATMODE:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.material_mode =
                            (int)SendDlgItemMessageW(h, IDC_MATMODE, CB_GETCURSEL, 0, 0);
                        material_layout_apply(h);
                        preview_dirty(h);
                    }
                    break;
```

- [ ] **Step 3: Build de depuração + smoke test**

```bash
export PATH="/c/Users/alanm/w64devkit/bin:$PATH"
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile debug
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

```powershell
$env:M3DT_SELFTEST = "1"
$env:M3DT_TAB = "2"
$env:M3DT_HOLD_MS = "3000"
$p = Start-Process -FilePath "D:\Downloads\Screensaver\dist\Modern3DText.exe" -ArgumentList "/c" -PassThru
$p | Wait-Process -Timeout 8
Write-Output "exitcode: $($p.ExitCode)"
```

- [ ] **Step 4: Verificação visual real — os 4 modos**

**Aviso ao usuário**: tentar `PushNotification` antes; se vier "not
sent", mandar mensagem de chat e **esperar confirmação explícita**
antes de capturar. Usar a técnica de captura real de tela (`.NET
Graphics.CopyFromScreen` via `EnumWindows` por PID, já validada nesta
sessão — `Process.MainWindowHandle` não funciona pra este app) já que
é layout de diálogo, não o painel 3D.

Capturar a aba Material (`M3DT_TAB=2`) nos 4 modos (mudar
`material_mode` no registro antes de cada captura: 0 Clássico, 1
Metálico, 2 Vidro, 3 Fosco) e confirmar: Clássico e Fosco mostram só
"Material:" + o combo, sem sobra de espaço em branco; Metálico mostra
os 3 blocos empilhados sem sobreposição; Vidro mostra Rugosidade +
Ambiente (sem Metalização) reposicionados corretamente, sem vão onde
Metalização estaria.

- [ ] **Step 5: Build release + refresh do `.exe`**

```bash
mingw32-make -f build/Makefile clean
mingw32-make -f build/Makefile release
cp -f dist/Modern3DText.scr dist/Modern3DText.exe
```

- [ ] **Step 6: Commit**

```bash
git add src/config_dialog.c
git commit -m "feat: hide and reposition Material tab controls by mode"
```

---

## Self-Review

1. **Cobertura do spec**: tabela de visibilidade do §2 → `vis_metal`/
   `vis_rough`/`vis_env` no Step 1; mecanismo de captura+reflow do §4
   → `material_layout_capture`/`material_layout_apply`; "sem campos
   novos" do §3 → confirmado, nenhuma mudança em `config.h`/`.c`.
2. **Placeholders**: nenhum — os dois novos blocos de código estão
   completos, prontos pra colar.
3. **Consistência**: `MatCtrl`/`g_mat_blocks`/`MAT_BLOCK_N` usados de
   forma idêntica entre `material_layout_capture` e
   `material_layout_apply` (mesmos IDs, mesma ordem de blocos).
4. **Risco verificado à mão**: confirmei que `material_labels()` (que
   escreve texto nos rótulos numéricos e no caminho da imagem) não
   precisa de nenhuma mudança — continua escrevendo texto em
   controles que podem estar ocultos no momento, o que é inofensivo
   (`SetDlgItemTextW` funciona em uma janela oculta sem erro).
