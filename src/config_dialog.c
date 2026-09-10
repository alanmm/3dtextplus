#include "config_dialog.h"
#include "resource.h"
#include "config.h"
#include "gl_window.h"
#include "util/log.h"

#include <windows.h>
#include <commctrl.h>
#include <glad/gl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "stb_image_write.h"

#define WM_PREVIEW_DIRTY (WM_APP + 1)
#define TIMER_SELFTEST 1
#define TIMER_PREVIEW  2

static Config     g_work;        /* config sendo editada */
static HWND       g_content;     /* sub-dialogo da aba Conteudo */
static HWND       g_motion;      /* sub-dialogo da aba Movimento */
static bool       g_selftest;
static GlWindow  *g_preview;
static bool       g_dirty;
static LARGE_INTEGER g_pstart, g_pfreq;

static void preview_teardown(HWND h)
{
    if (g_preview) {
        KillTimer(h, TIMER_PREVIEW);
        gl_window_destroy(g_preview);
        g_preview = NULL;
    }
}

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

/* ---------------- aba Movimento ---------------- */

static void set_slider(HWND h, int id, int lo, int hi, int pos)
{
    SendDlgItemMessageW(h, id, TBM_SETRANGE, TRUE, MAKELPARAM(lo, hi));
    SendDlgItemMessageW(h, id, TBM_SETPOS, TRUE, pos);
}

static void motion_labels(HWND h)
{
    wchar_t b[32];
    swprintf(b, 32, L"%.2f", (double)g_work.depth);       SetDlgItemTextW(h, IDC_DEPTH_VAL, b);
    swprintf(b, 32, L"%.0f", (double)g_work.max_angle_y); SetDlgItemTextW(h, IDC_ANGLE_VAL, b);
    swprintf(b, 32, L"%.0f", (double)g_work.tilt_x);      SetDlgItemTextW(h, IDC_TILT_VAL, b);
    swprintf(b, 32, L"%.1f", (double)g_work.period);      SetDlgItemTextW(h, IDC_PERIOD_VAL, b);
}

static INT_PTR CALLBACK motion_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)w; (void)l;
    switch (m) {
        case WM_INITDIALOG:
            set_slider(h, IDC_DEPTH,  2, 200, (int)(g_work.depth * 100.0f + 0.5f));
            set_slider(h, IDC_ANGLE,  5, 170, (int)(g_work.max_angle_y + 0.5f));
            set_slider(h, IDC_TILT,   0, 30,  (int)(g_work.tilt_x + 0.5f));
            set_slider(h, IDC_PERIOD, 20, 300, (int)(g_work.period * 10.0f + 0.5f));
            motion_labels(h);
            return TRUE;
        case WM_HSCROLL:
            g_work.depth       = (float)SendDlgItemMessageW(h, IDC_DEPTH, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.max_angle_y = (float)SendDlgItemMessageW(h, IDC_ANGLE, TBM_GETPOS, 0, 0);
            g_work.tilt_x      = (float)SendDlgItemMessageW(h, IDC_TILT, TBM_GETPOS, 0, 0);
            g_work.period      = (float)SendDlgItemMessageW(h, IDC_PERIOD, TBM_GETPOS, 0, 0) / 10.0f;
            motion_labels(h);
            preview_dirty(h);
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
            g_motion = CreateDialogW(GetModuleHandleW(NULL),
                                     MAKEINTRESOURCEW(IDD_TAB_MOTION), h, motion_proc);
            place_tab_child(h, tabs, g_content);
            place_tab_child(h, tabs, g_motion);
            ShowWindow(g_content, SW_SHOW);
            ShowWindow(g_motion, SW_HIDE);

            /* mini-preview 3D ao vivo */
            gl_window_global_init(GetModuleHandleW(NULL));
            {
                HWND ph = GetDlgItem(h, IDC_PREVIEW);
                RECT pr; GetClientRect(ph, &pr);
                g_preview = gl_window_create(GetModuleHandleW(NULL), WS_CHILD | WS_VISIBLE, 0, ph,
                                             0, 0, pr.right, pr.bottom, L"M3DTCfgPreview",
                                             DefWindowProcW, &g_work);
            }
            QueryPerformanceFrequency(&g_pfreq);
            QueryPerformanceCounter(&g_pstart);
            g_dirty = false;
            if (g_preview) SetTimer(h, TIMER_PREVIEW, 33, NULL);

            if (g_selftest) {
                char shot[8];
                UINT ms = (GetEnvironmentVariableA("M3DT_SHOT", shot, sizeof shot) > 0) ? 1500 : 700;
                SetTimer(h, TIMER_SELFTEST, ms, NULL);
            }
            return TRUE;
        }
        case WM_TIMER:
            if (w == TIMER_SELFTEST) {
                KillTimer(h, TIMER_SELFTEST);
                preview_teardown(h);
                EndDialog(h, IDCANCEL);
                return TRUE;
            }
            if (w == TIMER_PREVIEW && g_preview) {
                if (g_dirty) { gl_window_set_config(g_preview, &g_work); g_dirty = false; }
                LARGE_INTEGER now; QueryPerformanceCounter(&now);
                double t = (double)(now.QuadPart - g_pstart.QuadPart) / (double)g_pfreq.QuadPart;
                gl_window_frame(g_preview, t);

                static int ticks = 0;
                char shot[MAX_PATH];
                if (++ticks == 20 && GetEnvironmentVariableA("M3DT_SHOT", shot, sizeof shot) > 0) {
                    gl_window_render_scene_at(g_preview, 2.25);
                    int W = 0, H = 0;
                    gl_window_size(g_preview, &W, &H);
                    unsigned char *px = (unsigned char *)malloc((size_t)W * H * 3);
                    if (px) {
                        glPixelStorei(GL_PACK_ALIGNMENT, 1);
                        glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, px);
                        stbi_flip_vertically_on_write(1);
                        stbi_write_png(shot, W, H, 3, px, W * 3);
                        free(px);
                        log_infof("config: preview shot %s (%dx%d)", shot, W, H);
                    }
                }
                return TRUE;
            }
            break;
        case WM_NOTIFY:
            if (((LPNMHDR)l)->idFrom == IDC_TABS && ((LPNMHDR)l)->code == TCN_SELCHANGE) {
                int sel = TabCtrl_GetCurSel(GetDlgItem(h, IDC_TABS));
                ShowWindow(g_content, sel == 0 ? SW_SHOW : SW_HIDE);
                ShowWindow(g_motion,  sel == 1 ? SW_SHOW : SW_HIDE);
                return TRUE;
            }
            break;
        case WM_PREVIEW_DIRTY:
            g_dirty = true;   /* aplicado no proximo tick do preview (debounce natural) */
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDOK:      config_save(&g_work); preview_teardown(h); EndDialog(h, IDOK); return TRUE;
                case IDC_APPLY: config_save(&g_work); return TRUE;
                case IDCANCEL:  preview_teardown(h); EndDialog(h, IDCANCEL); return TRUE;
            }
            break;
        case WM_CLOSE:
            preview_teardown(h);
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
