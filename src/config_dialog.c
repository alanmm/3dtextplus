#include "config_dialog.h"
#include "resource.h"
#include "config.h"
#include "util/log.h"

#include <windows.h>
#include <commctrl.h>
#include <stdbool.h>
#include <string.h>

#define WM_PREVIEW_DIRTY (WM_APP + 1)

static Config g_work;        /* config sendo editada */
static HWND   g_content;     /* sub-dialogo da aba Conteudo */
static bool   g_selftest;

static bool env_selftest(void)
{
    char b[8];
    DWORD k = GetEnvironmentVariableA("M3DT_SELFTEST", b, sizeof b);
    return k > 0 && k < sizeof b && b[0] != '0';
}

static void preview_dirty(HWND child)
{
    PostMessageW(GetParent(GetParent(child)), WM_PREVIEW_DIRTY, 0, 0);
}

/* ---------------- aba Conteudo ---------------- */

static int CALLBACK enum_fonts_cb(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM lp)
{
    (void)tm; (void)type;
    if (lf->lfFaceName[0] != L'@')   /* pula as fontes verticais @Font */
        SendMessageW((HWND)lp, CB_ADDSTRING, 0, (LPARAM)lf->lfFaceName);
    return 1;
}

static INT_PTR CALLBACK content_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            wchar_t wtext[512];
            MultiByteToWideChar(CP_UTF8, 0, g_work.text, -1, wtext, 512);
            SetDlgItemTextW(h, IDC_TEXT, wtext);
            CheckDlgButton(h, IDC_BOLD, g_work.font_bold ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(h, IDC_ITALIC, g_work.font_italic ? BST_CHECKED : BST_UNCHECKED);

            HWND cb = GetDlgItem(h, IDC_FONT);
            HDC dc = GetDC(h);
            LOGFONTW lf;
            memset(&lf, 0, sizeof lf);
            lf.lfCharSet = DEFAULT_CHARSET;
            EnumFontFamiliesExW(dc, &lf, enum_fonts_cb, (LPARAM)cb, 0);
            ReleaseDC(h, dc);
            if (SendMessageW(cb, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)g_work.font_family) == CB_ERR)
                SendMessageW(cb, CB_SETCURSEL, 0, 0);
            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDC_TEXT:
                    if (HIWORD(w) == EN_CHANGE) {
                        wchar_t wtext[512];
                        GetDlgItemTextW(h, IDC_TEXT, wtext, 512);
                        WideCharToMultiByte(CP_UTF8, 0, wtext, -1, g_work.text,
                                            (int)sizeof g_work.text, NULL, NULL);
                        preview_dirty(h);
                    }
                    break;
                case IDC_FONT:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        int i = (int)SendDlgItemMessageW(h, IDC_FONT, CB_GETCURSEL, 0, 0);
                        if (i >= 0) {
                            SendDlgItemMessageW(h, IDC_FONT, CB_GETLBTEXT, i,
                                                (LPARAM)g_work.font_family);
                            preview_dirty(h);
                        }
                    }
                    break;
                case IDC_BOLD:
                    g_work.font_bold = (IsDlgButtonChecked(h, IDC_BOLD) == BST_CHECKED);
                    preview_dirty(h);
                    break;
                case IDC_ITALIC:
                    g_work.font_italic = (IsDlgButtonChecked(h, IDC_ITALIC) == BST_CHECKED);
                    preview_dirty(h);
                    break;
                case IDC_COLOR: {
                    static COLORREF custom[16];
                    CHOOSECOLORW cc;
                    memset(&cc, 0, sizeof cc);
                    cc.lStructSize = sizeof cc;
                    cc.hwndOwner = h;
                    cc.lpCustColors = custom;
                    cc.rgbResult = RGB((int)(g_work.base_r * 255.0f),
                                       (int)(g_work.base_g * 255.0f),
                                       (int)(g_work.base_b * 255.0f));
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColorW(&cc)) {
                        g_work.base_r = GetRValue(cc.rgbResult) / 255.0f;
                        g_work.base_g = GetGValue(cc.rgbResult) / 255.0f;
                        g_work.base_b = GetBValue(cc.rgbResult) / 255.0f;
                        preview_dirty(h);
                    }
                    break;
                }
            }
            return TRUE;
    }
    return FALSE;
}

/* ---------------- dialogo principal ---------------- */

static void place_tab_child(HWND dlg, HWND tabs, HWND child)
{
    RECT rc;
    GetClientRect(tabs, &rc);
    TabCtrl_AdjustRect(tabs, FALSE, &rc);
    MapWindowPoints(tabs, dlg, (POINT *)&rc, 2);
    SetWindowPos(child, HWND_TOP, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, 0);
}

static INT_PTR CALLBACK dlg_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
        case WM_INITDIALOG: {
            HWND tabs = GetDlgItem(h, IDC_TABS);
            TCITEMW ti;
            memset(&ti, 0, sizeof ti);
            ti.mask = TCIF_TEXT;
            ti.pszText = L"Conteudo";
            TabCtrl_InsertItem(tabs, 0, &ti);
            ti.pszText = L"Movimento";
            TabCtrl_InsertItem(tabs, 1, &ti);

            g_content = CreateDialogW(GetModuleHandleW(NULL),
                                      MAKEINTRESOURCEW(IDD_TAB_CONTENT), h, content_proc);
            place_tab_child(h, tabs, g_content);
            ShowWindow(g_content, SW_SHOW);

            if (g_selftest) SetTimer(h, 1, 700, NULL);
            return TRUE;
        }
        case WM_TIMER:
            if (w == 1) { KillTimer(h, 1); EndDialog(h, IDCANCEL); return TRUE; }
            break;
        case WM_NOTIFY:
            if (((LPNMHDR)l)->idFrom == IDC_TABS && ((LPNMHDR)l)->code == TCN_SELCHANGE) {
                int sel = TabCtrl_GetCurSel(GetDlgItem(h, IDC_TABS));
                ShowWindow(g_content, sel == 0 ? SW_SHOW : SW_HIDE);
                return TRUE;
            }
            break;
        case WM_PREVIEW_DIRTY:
            /* Task 5 liga ao mini-preview */
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDOK:      config_save(&g_work); EndDialog(h, IDOK); return TRUE;
                case IDC_APPLY: config_save(&g_work); return TRUE;
                case IDCANCEL:  EndDialog(h, IDCANCEL); return TRUE;
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
    INITCOMMONCONTROLSEX icc = {
        sizeof icc, ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES
    };
    InitCommonControlsEx(&icc);

    config_load(&g_work);
    g_selftest = env_selftest();

    HWND owner = IsWindow(parent) ? parent : NULL;
    log_infof("config: abrindo dialogo (owner=%p)", (void *)owner);
    INT_PTR r = DialogBoxParamW(hInst, MAKEINTRESOURCEW(IDD_CONFIG), owner, dlg_proc, 0);
    log_infof("config: dialogo fechou (r=%lld)", (long long)r);
    return 0;
}
