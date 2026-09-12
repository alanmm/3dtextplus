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
#include <math.h>

#include "stb_image_write.h"

#define WM_PREVIEW_DIRTY (WM_APP + 1)
#define TIMER_SELFTEST 1
#define TIMER_PREVIEW  2

static Config     g_work;        /* config sendo editada */
static Config     g_preview_cfg; /* copia de g_work p/ o preview, com texto fixo curto */
static HWND       g_content;     /* sub-dialogo da aba Conteudo */
static HWND       g_motion;      /* sub-dialogo da aba Movimento */
static HWND       g_material;    /* sub-dialogo da aba Material */
static HWND       g_geometry;    /* sub-dialogo da aba Geometria */
static HWND       g_effects;     /* sub-dialogo da aba Efeitos */
static HWND       g_perf;        /* sub-dialogo da aba Desempenho */
static HWND       g_post;        /* sub-dialogo da aba Pos */
static HWND       g_bg;          /* sub-dialogo da aba Fundo */
static HWND       g_particles;   /* sub-dialogo da aba Particulas */
static bool       g_selftest;
static GlWindow  *g_preview;
static bool       g_dirty;
static LARGE_INTEGER g_pstart, g_pfreq;

/* navegacao manual do preview (arrastar p/ girar, botao do meio p/ pan,
   wheel p/ zoom) - assim que o usuario interage de qualquer uma dessas
   formas, g_pv_manual liga e o zoom automatico por aba (select_tab) e o
   giro automatico (scene_set_auto_spin) param de valer ate a caixa ser
   reaberta. */
static bool  g_pv_manual;
static bool  g_pv_drag_rot, g_pv_drag_pan;
static POINT g_pv_last;
static float g_pv_zoom = 1.0f;   /* espelha o zoom efetivo atual, automatico ou manual */

#define PV_ROT_SENS  0.4f    /* graus por pixel arrastado */
#define PV_PAN_SENS  0.02f   /* unidades de mundo por pixel arrastado */
#define PV_ZOOM_MIN  0.3f
#define PV_ZOOM_MAX  15.0f

static void preview_teardown(HWND h)
{
    if (g_preview) {
        KillTimer(h, TIMER_PREVIEW);
        gl_window_destroy(g_preview);
        g_preview = NULL;
    }
}

/* WNDPROC da janela do preview 3D: arrastar com o botao esquerdo gira,
   arrastar com o botao do meio faz pan, ambos via mouse capture (funciona
   mesmo se o cursor sair da janela durante o arrasto). O wheel (zoom) e
   tratado no dialogo principal (WM_MOUSEWHEEL so chega a janela com foco,
   e este filho nunca recebe foco por tab - ver dlg_proc). */
static LRESULT CALLBACK preview_wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
        case WM_LBUTTONDOWN:
            g_pv_drag_rot = true;
            g_pv_manual = true;
            g_pv_last.x = (short)LOWORD(l);
            g_pv_last.y = (short)HIWORD(l);
            SetCapture(h);
            return 0;
        case WM_LBUTTONUP:
            g_pv_drag_rot = false;
            if (!g_pv_drag_pan) ReleaseCapture();
            return 0;
        case WM_MBUTTONDOWN:
            g_pv_drag_pan = true;
            g_pv_manual = true;
            g_pv_last.x = (short)LOWORD(l);
            g_pv_last.y = (short)HIWORD(l);
            SetCapture(h);
            return 0;
        case WM_MBUTTONUP:
            g_pv_drag_pan = false;
            if (!g_pv_drag_rot) ReleaseCapture();
            return 0;
        case WM_CAPTURECHANGED:
            g_pv_drag_rot = false;
            g_pv_drag_pan = false;
            return 0;
        case WM_MOUSEMOVE: {
            if (!g_preview || (!g_pv_drag_rot && !g_pv_drag_pan)) break;
            int x = (short)LOWORD(l), y = (short)HIWORD(l);
            int dx = x - g_pv_last.x, dy = y - g_pv_last.y;
            if (g_pv_drag_rot)
                gl_window_orbit(g_preview, (float)dx * PV_ROT_SENS, (float)-dy * PV_ROT_SENS);
            if (g_pv_drag_pan)
                gl_window_pan(g_preview, (float)-dx * PV_PAN_SENS, (float)dy * PV_PAN_SENS);
            g_pv_last.x = x; g_pv_last.y = y;
            return 0;
        }
    }
    return DefWindowProcW(h, m, w, l);
}

static bool env_selftest(void)
{
    char b[8];
    DWORD k = GetEnvironmentVariableA("M3DT_SELFTEST", b, sizeof b);
    return k > 0 && k < sizeof b && b[0] != '0';
}

static void preview_dirty(HWND child)
{
    /* child = janela do sub-dialogo da aba (o "h" recebido em cada *_proc);
       seu pai direto e o dialogo principal, que trata WM_PREVIEW_DIRTY. */
    PostMessageW(GetParent(child), WM_PREVIEW_DIRTY, 0, 0);
}

/* copia g_work para o preview com o texto fixo em "3D" - um texto configurado
   longo nao cabe no enquadramento aproximado do preview; a tela cheia real
   (/s, /p, /c) sempre usa g_work sem essa substituicao. */
static void preview_config_sync(void)
{
    g_preview_cfg = g_work;
    strcpy(g_preview_cfg.text, "3D");
}

/* ---------------- aba Conteudo ---------------- */

static int CALLBACK enum_fonts_cb(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM lp)
{
    (void)tm; (void)type;
    HWND cb = (HWND)lp;
    /* EnumFontFamiliesExW chama o callback uma vez por charset/instancia de peso
       (familias variaveis como "Playfair Display" reportam Black/ExtraBold/
       Medium/SemiBold separadamente, todas com o mesmo lfFaceName) -> deduplica
       antes de adicionar, senao a combo fica cheia de repetidos. */
    if (lf->lfFaceName[0] != L'@' &&   /* pula as fontes verticais @Font */
        SendMessageW(cb, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)lf->lfFaceName) == CB_ERR)
        SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)lf->lfFaceName);
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
    swprintf(b, 32, L"%.0f", (double)g_work.max_angle_y); SetDlgItemTextW(h, IDC_ANGLE_VAL, b);
    swprintf(b, 32, L"%.0f", (double)g_work.tilt_x);      SetDlgItemTextW(h, IDC_TILT_VAL, b);
    swprintf(b, 32, L"%.1f", (double)g_work.period);      SetDlgItemTextW(h, IDC_PERIOD_VAL, b);
}

static INT_PTR CALLBACK motion_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)w; (void)l;
    switch (m) {
        case WM_INITDIALOG:
            set_slider(h, IDC_ANGLE,  5, 170, (int)(g_work.max_angle_y + 0.5f));
            set_slider(h, IDC_TILT,   0, 30,  (int)(g_work.tilt_x + 0.5f));
            set_slider(h, IDC_PERIOD, 20, 300, (int)(g_work.period * 10.0f + 0.5f));
            motion_labels(h);
            return TRUE;
        case WM_HSCROLL:
            g_work.max_angle_y = (float)SendDlgItemMessageW(h, IDC_ANGLE, TBM_GETPOS, 0, 0);
            g_work.tilt_x      = (float)SendDlgItemMessageW(h, IDC_TILT, TBM_GETPOS, 0, 0);
            g_work.period      = (float)SendDlgItemMessageW(h, IDC_PERIOD, TBM_GETPOS, 0, 0) / 10.0f;
            motion_labels(h);
            preview_dirty(h);
            return TRUE;
    }
    return FALSE;
}

/* ---------------- aba Material ---------------- */

static void material_labels(HWND h)
{
    wchar_t b[32];
    swprintf(b, 32, L"%.2f", (double)g_work.metalness);  SetDlgItemTextW(h, IDC_METAL_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.roughness);  SetDlgItemTextW(h, IDC_ROUGH_VAL, b);
    SetDlgItemTextW(h, IDC_ENVPATH, g_work.env_path[0] ? g_work.env_path : L"(procedural)");
}

static INT_PTR CALLBACK material_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            static const wchar_t *names[] = { L"Classico", L"Metalico", L"Vidro", L"Fosco" };
            for (int i = 0; i < 4; ++i)
                SendDlgItemMessageW(h, IDC_MATMODE, CB_ADDSTRING, 0, (LPARAM)names[i]);
            SendDlgItemMessageW(h, IDC_MATMODE, CB_SETCURSEL, g_work.material_mode, 0);
            set_slider(h, IDC_METAL, 0, 100, (int)(g_work.metalness * 100.0f + 0.5f));
            set_slider(h, IDC_ROUGH, 0, 100, (int)(g_work.roughness * 100.0f + 0.5f));
            material_labels(h);
            return TRUE;
        }
        case WM_HSCROLL:
            g_work.metalness = (float)SendDlgItemMessageW(h, IDC_METAL, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.roughness = (float)SendDlgItemMessageW(h, IDC_ROUGH, TBM_GETPOS, 0, 0) / 100.0f;
            material_labels(h);
            preview_dirty(h);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDC_MATMODE:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.material_mode =
                            (int)SendDlgItemMessageW(h, IDC_MATMODE, CB_GETCURSEL, 0, 0);
                        preview_dirty(h);
                    }
                    break;
                case IDC_ENVPICK: {
                    wchar_t file[512] = L"";
                    OPENFILENAMEW ofn;
                    memset(&ofn, 0, sizeof ofn);
                    ofn.lStructSize = sizeof ofn;
                    ofn.hwndOwner = h;
                    ofn.lpstrFilter = L"Imagens\0*.jpg;*.jpeg;*.png;*.bmp;*.tga\0Todos\0*.*\0";
                    ofn.lpstrFile = file;
                    ofn.nMaxFile = 512;
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                    if (GetOpenFileNameW(&ofn)) {
                        wcsncpy(g_work.env_path, file, 511);
                        g_work.env_path[511] = 0;
                        material_labels(h);
                        preview_dirty(h);
                    }
                    break;
                }
                case IDC_ENVCLEAR:
                    g_work.env_path[0] = 0;
                    material_labels(h);
                    preview_dirty(h);
                    break;
            }
            return TRUE;
    }
    return FALSE;
}

/* ---------------- aba Geometria ---------------- */

static void geometry_labels(HWND h)
{
    wchar_t b[32];
    swprintf(b, 32, L"%.2f", (double)g_work.depth);             SetDlgItemTextW(h, IDC_DEPTH_VAL, b);
    swprintf(b, 32, L"%.03f", (double)g_work.bevel_size);      SetDlgItemTextW(h, IDC_BSIZE_VAL, b);
    swprintf(b, 32, L"%.03f", (double)g_work.bevel_depth);     SetDlgItemTextW(h, IDC_BDEPTH_VAL, b);
    swprintf(b, 32, L"%d", g_work.bevel_segments);             SetDlgItemTextW(h, IDC_BSEG_VAL, b);
    swprintf(b, 32, L"%.03f", (double)g_work.wall_thickness);  SetDlgItemTextW(h, IDC_WALL_VAL, b);
}

static void geometry_enable(HWND h)
{
    EnableWindow(GetDlgItem(h, IDC_BSEG), g_work.bevel_mode == 1);
    EnableWindow(GetDlgItem(h, IDC_WALL), g_work.shell != 0);
}

static INT_PTR CALLBACK geometry_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            static const wchar_t *bev[]  = { L"Sombreado", L"Geometrico", L"Desligado" };
            static const wchar_t *qual[] = { L"Baixa", L"Media", L"Alta" };
            for (int i = 0; i < 3; ++i)
                SendDlgItemMessageW(h, IDC_BEVELMODE, CB_ADDSTRING, 0, (LPARAM)bev[i]);
            for (int i = 0; i < 3; ++i)
                SendDlgItemMessageW(h, IDC_QUALITY, CB_ADDSTRING, 0, (LPARAM)qual[i]);
            SendDlgItemMessageW(h, IDC_BEVELMODE, CB_SETCURSEL, g_work.bevel_mode, 0);
            SendDlgItemMessageW(h, IDC_QUALITY, CB_SETCURSEL, g_work.quality, 0);
            set_slider(h, IDC_DEPTH,  2, 200, (int)(g_work.depth * 100.0f + 0.5f));
            set_slider(h, IDC_BSIZE,  0, 200, (int)(g_work.bevel_size * 1000.0f + 0.5f));
            set_slider(h, IDC_BDEPTH, 0, 200, (int)(g_work.bevel_depth * 1000.0f + 0.5f));
            set_slider(h, IDC_BSEG,   2, 8,   g_work.bevel_segments);
            set_slider(h, IDC_WALL,   10, 200, (int)(g_work.wall_thickness * 1000.0f + 0.5f));
            CheckDlgButton(h, IDC_SHELL, g_work.shell ? BST_CHECKED : BST_UNCHECKED);
            geometry_labels(h);
            geometry_enable(h);
            return TRUE;
        }
        case WM_HSCROLL:
            g_work.depth          = (float)SendDlgItemMessageW(h, IDC_DEPTH, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.bevel_size     = (float)SendDlgItemMessageW(h, IDC_BSIZE, TBM_GETPOS, 0, 0) / 1000.0f;
            g_work.bevel_depth    = (float)SendDlgItemMessageW(h, IDC_BDEPTH, TBM_GETPOS, 0, 0) / 1000.0f;
            g_work.bevel_segments = (int)SendDlgItemMessageW(h, IDC_BSEG, TBM_GETPOS, 0, 0);
            g_work.wall_thickness = (float)SendDlgItemMessageW(h, IDC_WALL, TBM_GETPOS, 0, 0) / 1000.0f;
            geometry_labels(h);
            preview_dirty(h);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDC_BEVELMODE:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.bevel_mode = (int)SendDlgItemMessageW(h, IDC_BEVELMODE, CB_GETCURSEL, 0, 0);
                        geometry_enable(h);
                        preview_dirty(h);
                    }
                    break;
                case IDC_QUALITY:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.quality = (int)SendDlgItemMessageW(h, IDC_QUALITY, CB_GETCURSEL, 0, 0);
                        preview_dirty(h);
                    }
                    break;
                case IDC_SHELL:
                    g_work.shell = (IsDlgButtonChecked(h, IDC_SHELL) == BST_CHECKED);
                    geometry_enable(h);
                    preview_dirty(h);
                    break;
            }
            return TRUE;
    }
    return FALSE;
}

/* ---------------- aba Efeitos ---------------- */

static void effects_labels(HWND h)
{
    wchar_t b[32];
    swprintf(b, 32, L"%.2f", (double)g_work.bloom_threshold); SetDlgItemTextW(h, IDC_BTHRESH_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.bloom_intensity); SetDlgItemTextW(h, IDC_BINT_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.bloom_radius);    SetDlgItemTextW(h, IDC_BRAD_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.streaks_intensity); SetDlgItemTextW(h, IDC_SINT_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.streaks_length);    SetDlgItemTextW(h, IDC_SLEN_VAL, b);
}

static void effects_enable(HWND h)
{
    BOOL on = g_work.bloom_on ? TRUE : FALSE;
    EnableWindow(GetDlgItem(h, IDC_BTHRESH), on);
    EnableWindow(GetDlgItem(h, IDC_BINT), on);
    EnableWindow(GetDlgItem(h, IDC_BRAD), on);
    BOOL st = g_work.streaks_mode != 0;
    EnableWindow(GetDlgItem(h, IDC_SINT), st);
    EnableWindow(GetDlgItem(h, IDC_SLEN), st);
}

static INT_PTR CALLBACK effects_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG:
            CheckDlgButton(h, IDC_BLOOM, g_work.bloom_on ? BST_CHECKED : BST_UNCHECKED);
            set_slider(h, IDC_BTHRESH, 20, 300, (int)(g_work.bloom_threshold * 100.0f + 0.5f));
            set_slider(h, IDC_BINT,     0, 200, (int)(g_work.bloom_intensity * 100.0f + 0.5f));
            set_slider(h, IDC_BRAD,     0, 100, (int)(g_work.bloom_radius * 100.0f + 0.5f));
            static const wchar_t *sm[] = { L"Desligado", L"Starburst", L"Anamorfico" };
            for (int i = 0; i < 3; ++i)
                SendDlgItemMessageW(h, IDC_STREAKMODE, CB_ADDSTRING, 0, (LPARAM)sm[i]);
            SendDlgItemMessageW(h, IDC_STREAKMODE, CB_SETCURSEL, g_work.streaks_mode, 0);
            set_slider(h, IDC_SINT, 0, 200, (int)(g_work.streaks_intensity * 100.0f + 0.5f));
            set_slider(h, IDC_SLEN, 0, 100, (int)(g_work.streaks_length * 100.0f + 0.5f));
            effects_labels(h);
            effects_enable(h);
            return TRUE;
        case WM_HSCROLL:
            g_work.bloom_threshold = (float)SendDlgItemMessageW(h, IDC_BTHRESH, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.bloom_intensity = (float)SendDlgItemMessageW(h, IDC_BINT, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.bloom_radius    = (float)SendDlgItemMessageW(h, IDC_BRAD, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.streaks_intensity = (float)SendDlgItemMessageW(h, IDC_SINT, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.streaks_length    = (float)SendDlgItemMessageW(h, IDC_SLEN, TBM_GETPOS, 0, 0) / 100.0f;
            effects_labels(h);
            preview_dirty(h);
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(w) == IDC_BLOOM) {
                g_work.bloom_on = (IsDlgButtonChecked(h, IDC_BLOOM) == BST_CHECKED);
                effects_enable(h);
                preview_dirty(h);
            } else if (LOWORD(w) == IDC_STREAKMODE && HIWORD(w) == CBN_SELCHANGE) {
                g_work.streaks_mode =
                    (int)SendDlgItemMessageW(h, IDC_STREAKMODE, CB_GETCURSEL, 0, 0);
                effects_enable(h);
                preview_dirty(h);
            }
            return TRUE;
    }
    return FALSE;
}

/* ---------------- aba Desempenho ---------------- */

static const int PERF_FPS[4]  = { 0, 30, 60, 120 };
static const int PERF_MSAA[4] = { 0, 2, 4, 8 };

static void perf_labels(HWND h)
{
    wchar_t b[32];
    swprintf(b, 32, L"%d%%", (int)(g_work.render_scale * 100.0f + 0.5f));
    SetDlgItemTextW(h, IDC_RSCALE_VAL, b);
}

static INT_PTR CALLBACK perf_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            static const wchar_t *fps[] = { L"Sem limite", L"30", L"60", L"120" };
            static const wchar_t *ms[]  = { L"Desligado", L"2x", L"4x", L"8x" };
            for (int i = 0; i < 4; ++i) {
                SendDlgItemMessageW(h, IDC_FPSCAP, CB_ADDSTRING, 0, (LPARAM)fps[i]);
                SendDlgItemMessageW(h, IDC_MSAA,   CB_ADDSTRING, 0, (LPARAM)ms[i]);
            }
            int fi = 2, mi = 2;
            for (int i = 0; i < 4; ++i) {
                if (PERF_FPS[i] == g_work.fps_cap) fi = i;
                if (PERF_MSAA[i] == g_work.msaa)   mi = i;
            }
            SendDlgItemMessageW(h, IDC_FPSCAP, CB_SETCURSEL, fi, 0);
            SendDlgItemMessageW(h, IDC_MSAA,   CB_SETCURSEL, mi, 0);
            CheckDlgButton(h, IDC_VSYNC, g_work.vsync ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(h, IDC_AUTOQ, g_work.auto_quality ? BST_CHECKED : BST_UNCHECKED);
            set_slider(h, IDC_RSCALE, 50, 100, (int)(g_work.render_scale * 100.0f + 0.5f));
            perf_labels(h);
            return TRUE;
        }
        case WM_HSCROLL:
            g_work.render_scale =
                (float)SendDlgItemMessageW(h, IDC_RSCALE, TBM_GETPOS, 0, 0) / 100.0f;
            perf_labels(h);
            preview_dirty(h);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDC_FPSCAP:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        int i = (int)SendDlgItemMessageW(h, IDC_FPSCAP, CB_GETCURSEL, 0, 0);
                        if (i >= 0 && i < 4) g_work.fps_cap = PERF_FPS[i];
                        preview_dirty(h);
                    }
                    break;
                case IDC_MSAA:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        int i = (int)SendDlgItemMessageW(h, IDC_MSAA, CB_GETCURSEL, 0, 0);
                        if (i >= 0 && i < 4) g_work.msaa = PERF_MSAA[i];
                        preview_dirty(h);
                    }
                    break;
                case IDC_VSYNC:
                    g_work.vsync = (IsDlgButtonChecked(h, IDC_VSYNC) == BST_CHECKED);
                    preview_dirty(h);
                    break;
                case IDC_AUTOQ:
                    g_work.auto_quality = (IsDlgButtonChecked(h, IDC_AUTOQ) == BST_CHECKED);
                    preview_dirty(h);
                    break;
            }
            return TRUE;
    }
    return FALSE;
}

/* ---------------- aba Pos ---------------- */

static void post_labels(HWND h)
{
    wchar_t b[32];
    swprintf(b, 32, L"%.2f", (double)g_work.chroma_strength);  SetDlgItemTextW(h, IDC_CSTR_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.vignette_amount);  SetDlgItemTextW(h, IDC_VAMT_VAL, b);
}

static void post_enable(HWND h)
{
    EnableWindow(GetDlgItem(h, IDC_CSTR), g_work.chroma_on ? TRUE : FALSE);
    EnableWindow(GetDlgItem(h, IDC_VAMT), g_work.vignette_on ? TRUE : FALSE);
}

static INT_PTR CALLBACK post_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG:
            CheckDlgButton(h, IDC_CHROMA, g_work.chroma_on ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(h, IDC_VIGNETTE, g_work.vignette_on ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(h, IDC_FXAA, g_work.fxaa_on ? BST_CHECKED : BST_UNCHECKED);
            set_slider(h, IDC_CSTR, 0, 100, (int)(g_work.chroma_strength * 100.0f + 0.5f));
            set_slider(h, IDC_VAMT, 0, 100, (int)(g_work.vignette_amount * 100.0f + 0.5f));
            post_labels(h);
            post_enable(h);
            return TRUE;
        case WM_HSCROLL:
            g_work.chroma_strength = (float)SendDlgItemMessageW(h, IDC_CSTR, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.vignette_amount = (float)SendDlgItemMessageW(h, IDC_VAMT, TBM_GETPOS, 0, 0) / 100.0f;
            post_labels(h);
            preview_dirty(h);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDC_CHROMA:
                    g_work.chroma_on = (IsDlgButtonChecked(h, IDC_CHROMA) == BST_CHECKED);
                    post_enable(h);
                    preview_dirty(h);
                    break;
                case IDC_VIGNETTE:
                    g_work.vignette_on = (IsDlgButtonChecked(h, IDC_VIGNETTE) == BST_CHECKED);
                    post_enable(h);
                    preview_dirty(h);
                    break;
                case IDC_FXAA:
                    g_work.fxaa_on = (IsDlgButtonChecked(h, IDC_FXAA) == BST_CHECKED);
                    preview_dirty(h);
                    break;
            }
            return TRUE;
    }
    return FALSE;
}

/* ---------------- aba Fundo ---------------- */

static void bg_labels(HWND h)
{
    wchar_t b[32];
    swprintf(b, 32, L"%.0f", (double)g_work.bg_grad_angle); SetDlgItemTextW(h, IDC_BGANGLE_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.bg_pan_speed);  SetDlgItemTextW(h, IDC_BGPAN_VAL, b);
    SetDlgItemTextW(h, IDC_BGIMGPATH, g_work.bg_image_path[0] ? g_work.bg_image_path : L"(nenhuma)");
}

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

static INT_PTR CALLBACK bg_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            static const wchar_t *types[] = { L"Solido", L"Gradiente", L"Imagem", L"Nebulosa" };
            static const wchar_t *fits[]  = { L"Cobrir", L"Conter", L"Repetir" };
            for (int i = 0; i < 4; ++i)
                SendDlgItemMessageW(h, IDC_BGTYPE, CB_ADDSTRING, 0, (LPARAM)types[i]);
            for (int i = 0; i < 3; ++i)
                SendDlgItemMessageW(h, IDC_BGFIT, CB_ADDSTRING, 0, (LPARAM)fits[i]);
            SendDlgItemMessageW(h, IDC_BGTYPE, CB_SETCURSEL, g_work.background_type, 0);
            SendDlgItemMessageW(h, IDC_BGFIT, CB_SETCURSEL, g_work.bg_image_fit, 0);
            set_slider(h, IDC_BGANGLE, 0, 360, (int)(g_work.bg_grad_angle + 0.5f));
            set_slider(h, IDC_BGPAN, 0, 100, (int)(g_work.bg_pan_speed * 100.0f + 0.5f));
            bg_labels(h);
            bg_enable(h);
            return TRUE;
        }
        case WM_HSCROLL:
            g_work.bg_grad_angle = (float)SendDlgItemMessageW(h, IDC_BGANGLE, TBM_GETPOS, 0, 0);
            g_work.bg_pan_speed  = (float)SendDlgItemMessageW(h, IDC_BGPAN, TBM_GETPOS, 0, 0) / 100.0f;
            bg_labels(h);
            preview_dirty(h);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDC_BGTYPE:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.background_type = (int)SendDlgItemMessageW(h, IDC_BGTYPE, CB_GETCURSEL, 0, 0);
                        bg_enable(h);
                        preview_dirty(h);
                    }
                    break;
                case IDC_BGFIT:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.bg_image_fit = (int)SendDlgItemMessageW(h, IDC_BGFIT, CB_GETCURSEL, 0, 0);
                        preview_dirty(h);
                    }
                    break;
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
                case IDC_BGCOLOR2: {
                    static COLORREF custom[16];
                    CHOOSECOLORW cc;
                    memset(&cc, 0, sizeof cc);
                    cc.lStructSize = sizeof cc;
                    cc.hwndOwner = h;
                    cc.lpCustColors = custom;
                    cc.rgbResult = RGB((int)(g_work.bg_color2_r * 255.0f),
                                       (int)(g_work.bg_color2_g * 255.0f),
                                       (int)(g_work.bg_color2_b * 255.0f));
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColorW(&cc)) {
                        g_work.bg_color2_r = GetRValue(cc.rgbResult) / 255.0f;
                        g_work.bg_color2_g = GetGValue(cc.rgbResult) / 255.0f;
                        g_work.bg_color2_b = GetBValue(cc.rgbResult) / 255.0f;
                        preview_dirty(h);
                    }
                    break;
                }
                case IDC_BGNEBCOLOR1: {
                    static COLORREF custom[16];
                    CHOOSECOLORW cc;
                    memset(&cc, 0, sizeof cc);
                    cc.lStructSize = sizeof cc;
                    cc.hwndOwner = h;
                    cc.lpCustColors = custom;
                    cc.rgbResult = RGB((int)(g_work.bg_neb_color1_r * 255.0f),
                                       (int)(g_work.bg_neb_color1_g * 255.0f),
                                       (int)(g_work.bg_neb_color1_b * 255.0f));
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColorW(&cc)) {
                        g_work.bg_neb_color1_r = GetRValue(cc.rgbResult) / 255.0f;
                        g_work.bg_neb_color1_g = GetGValue(cc.rgbResult) / 255.0f;
                        g_work.bg_neb_color1_b = GetBValue(cc.rgbResult) / 255.0f;
                        preview_dirty(h);
                    }
                    break;
                }
                case IDC_BGNEBCOLOR2: {
                    static COLORREF custom[16];
                    CHOOSECOLORW cc;
                    memset(&cc, 0, sizeof cc);
                    cc.lStructSize = sizeof cc;
                    cc.hwndOwner = h;
                    cc.lpCustColors = custom;
                    cc.rgbResult = RGB((int)(g_work.bg_neb_color2_r * 255.0f),
                                       (int)(g_work.bg_neb_color2_g * 255.0f),
                                       (int)(g_work.bg_neb_color2_b * 255.0f));
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColorW(&cc)) {
                        g_work.bg_neb_color2_r = GetRValue(cc.rgbResult) / 255.0f;
                        g_work.bg_neb_color2_g = GetGValue(cc.rgbResult) / 255.0f;
                        g_work.bg_neb_color2_b = GetBValue(cc.rgbResult) / 255.0f;
                        preview_dirty(h);
                    }
                    break;
                }
                case IDC_BGIMGPICK: {
                    wchar_t file[512] = L"";
                    OPENFILENAMEW ofn;
                    memset(&ofn, 0, sizeof ofn);
                    ofn.lStructSize = sizeof ofn;
                    ofn.hwndOwner = h;
                    ofn.lpstrFilter = L"Imagens\0*.jpg;*.jpeg;*.png;*.bmp;*.tga\0Todos\0*.*\0";
                    ofn.lpstrFile = file;
                    ofn.nMaxFile = 512;
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                    if (GetOpenFileNameW(&ofn)) {
                        wcsncpy(g_work.bg_image_path, file, 511);
                        g_work.bg_image_path[511] = 0;
                        bg_labels(h);
                        preview_dirty(h);
                    }
                    break;
                }
                case IDC_BGIMGCLEAR:
                    g_work.bg_image_path[0] = 0;
                    bg_labels(h);
                    preview_dirty(h);
                    break;
            }
            return TRUE;
    }
    return FALSE;
}

/* ---------------- aba Particulas ---------------- */

static void particles_labels(HWND h)
{
    wchar_t b[32];
    swprintf(b, 32, L"%.2f", (double)g_work.particles_density);    SetDlgItemTextW(h, IDC_PARTDENS_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.particles_speed);      SetDlgItemTextW(h, IDC_PARTSPEED_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.particles_size_scale); SetDlgItemTextW(h, IDC_PARTSIZE_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.particles_opacity);    SetDlgItemTextW(h, IDC_PARTOPACITY_VAL, b);
}

static void particles_enable(HWND h)
{
    BOOL on = g_work.particles_on ? TRUE : FALSE;
    EnableWindow(GetDlgItem(h, IDC_PARTKIND), on);
    EnableWindow(GetDlgItem(h, IDC_PARTDENS), on);
    EnableWindow(GetDlgItem(h, IDC_PARTSPEED), on);
    EnableWindow(GetDlgItem(h, IDC_PARTSIZE), on);
    EnableWindow(GetDlgItem(h, IDC_PARTOPACITY), on);
}

static INT_PTR CALLBACK particles_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            static const wchar_t *kinds[] = { L"Poeira", L"Bokeh", L"Faiscas", L"Estrelas" };
            for (int i = 0; i < 4; ++i)
                SendDlgItemMessageW(h, IDC_PARTKIND, CB_ADDSTRING, 0, (LPARAM)kinds[i]);
            SendDlgItemMessageW(h, IDC_PARTKIND, CB_SETCURSEL, g_work.particles_kind, 0);
            CheckDlgButton(h, IDC_PARTON, g_work.particles_on ? BST_CHECKED : BST_UNCHECKED);
            set_slider(h, IDC_PARTDENS, 0, 100, (int)(g_work.particles_density * 100.0f + 0.5f));
            set_slider(h, IDC_PARTSPEED, 0, 200, (int)(g_work.particles_speed * 100.0f + 0.5f));
            set_slider(h, IDC_PARTSIZE, 0, 200, (int)(g_work.particles_size_scale * 100.0f + 0.5f));
            set_slider(h, IDC_PARTOPACITY, 0, 200, (int)(g_work.particles_opacity * 100.0f + 0.5f));
            particles_labels(h);
            particles_enable(h);
            return TRUE;
        }
        case WM_HSCROLL:
            g_work.particles_density    = (float)SendDlgItemMessageW(h, IDC_PARTDENS, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.particles_speed      = (float)SendDlgItemMessageW(h, IDC_PARTSPEED, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.particles_size_scale = (float)SendDlgItemMessageW(h, IDC_PARTSIZE, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.particles_opacity    = (float)SendDlgItemMessageW(h, IDC_PARTOPACITY, TBM_GETPOS, 0, 0) / 100.0f;
            particles_labels(h);
            preview_dirty(h);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDC_PARTON:
                    g_work.particles_on = (IsDlgButtonChecked(h, IDC_PARTON) == BST_CHECKED);
                    particles_enable(h);
                    preview_dirty(h);
                    break;
                case IDC_PARTKIND:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.particles_kind = (int)SendDlgItemMessageW(h, IDC_PARTKIND, CB_GETCURSEL, 0, 0);
                        preview_dirty(h);
                    }
                    break;
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

static void select_tab(int sel)
{
    ShowWindow(g_content,  sel == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_motion,   sel == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_material, sel == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_geometry, sel == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_effects,  sel == 4 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_perf,     sel == 5 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_post,     sel == 6 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_bg,       sel == 7 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_particles, sel == 8 ? SW_SHOW : SW_HIDE);

    /* Material e Geometria se beneficiam de um enquadramento mais proximo -
       e onde bevel, metalizacao e reflexo de ambiente ficam visiveis no preview
       minusculo; as demais abas usam a vista ampla padrao. Uma vez que o
       usuario assume controle manual do zoom (wheel), isso para de valer -
       trocar de aba nao deve mais "puxar" o zoom de volta. */
    if (g_preview && !g_pv_manual) {
        g_pv_zoom = (sel == 2 || sel == 3) ? 4.4f : 2.0f;
        gl_window_set_zoom(g_preview, g_pv_zoom);
    }
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
            ti.pszText = L"Material";
            TabCtrl_InsertItem(tabs, 2, &ti);
            ti.pszText = L"Geometria";
            TabCtrl_InsertItem(tabs, 3, &ti);
            ti.pszText = L"Efeitos";
            TabCtrl_InsertItem(tabs, 4, &ti);
            ti.pszText = L"Desempenho";
            TabCtrl_InsertItem(tabs, 5, &ti);
            ti.pszText = L"Pos";
            TabCtrl_InsertItem(tabs, 6, &ti);
            ti.pszText = L"Fundo";
            TabCtrl_InsertItem(tabs, 7, &ti);
            ti.pszText = L"Particulas";
            TabCtrl_InsertItem(tabs, 8, &ti);

            g_content = CreateDialogW(GetModuleHandleW(NULL),
                                      MAKEINTRESOURCEW(IDD_TAB_CONTENT), h, content_proc);
            g_motion = CreateDialogW(GetModuleHandleW(NULL),
                                     MAKEINTRESOURCEW(IDD_TAB_MOTION), h, motion_proc);
            g_material = CreateDialogW(GetModuleHandleW(NULL),
                                      MAKEINTRESOURCEW(IDD_TAB_MATERIAL), h, material_proc);
            g_geometry = CreateDialogW(GetModuleHandleW(NULL),
                                       MAKEINTRESOURCEW(IDD_TAB_GEOMETRY), h, geometry_proc);
            g_effects = CreateDialogW(GetModuleHandleW(NULL),
                                      MAKEINTRESOURCEW(IDD_TAB_EFFECTS), h, effects_proc);
            g_perf = CreateDialogW(GetModuleHandleW(NULL),
                                   MAKEINTRESOURCEW(IDD_TAB_PERF), h, perf_proc);
            g_post = CreateDialogW(GetModuleHandleW(NULL),
                                   MAKEINTRESOURCEW(IDD_TAB_POST), h, post_proc);
            g_bg = CreateDialogW(GetModuleHandleW(NULL),
                                 MAKEINTRESOURCEW(IDD_TAB_BG), h, bg_proc);
            g_particles = CreateDialogW(GetModuleHandleW(NULL),
                                        MAKEINTRESOURCEW(IDD_TAB_PARTICLES), h, particles_proc);
            place_tab_child(h, tabs, g_content);
            place_tab_child(h, tabs, g_motion);
            place_tab_child(h, tabs, g_material);
            place_tab_child(h, tabs, g_geometry);
            place_tab_child(h, tabs, g_effects);
            place_tab_child(h, tabs, g_perf);
            place_tab_child(h, tabs, g_post);
            place_tab_child(h, tabs, g_bg);
            place_tab_child(h, tabs, g_particles);
            select_tab(0);

            /* mini-preview 3D ao vivo */
            gl_window_global_init(GetModuleHandleW(NULL));
            g_pv_manual = false;
            g_pv_drag_rot = false;
            g_pv_drag_pan = false;
            g_pv_zoom = 1.0f;
            {
                HWND ph = GetDlgItem(h, IDC_PREVIEW);
                RECT pr; GetClientRect(ph, &pr);
                preview_config_sync();
                g_preview = gl_window_create(GetModuleHandleW(NULL), WS_CHILD | WS_VISIBLE, 0, ph,
                                             0, 0, pr.right, pr.bottom, L"M3DTCfgPreview",
                                             preview_wndproc, &g_preview_cfg, 0);
                if (g_preview) gl_window_set_auto_spin(g_preview, 1);
            }
            QueryPerformanceFrequency(&g_pfreq);
            QueryPerformanceCounter(&g_pstart);
            g_dirty = false;
            if (g_preview) SetTimer(h, TIMER_PREVIEW, 33, NULL);

            if (g_selftest) {
                char tb[8];
                if (GetEnvironmentVariableA("M3DT_TAB", tb, sizeof tb) > 0) {
                    int sel = atoi(tb);
                    if (sel < 0) sel = 0;
                    if (sel > 8) sel = 8;
                    TabCtrl_SetCurSel(tabs, sel);
                    select_tab(sel);
                }
                char shot[8], hold[16];
                UINT ms = (GetEnvironmentVariableA("M3DT_SHOT", shot, sizeof shot) > 0) ? 1500 : 700;
                if (GetEnvironmentVariableA("M3DT_HOLD_MS", hold, sizeof hold) > 0) {
                    int v = atoi(hold);
                    if (v > 0) ms = (UINT)v;
                }
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
                if (g_dirty) { preview_config_sync(); gl_window_set_config(g_preview, &g_preview_cfg); g_dirty = false; }
                LARGE_INTEGER now; QueryPerformanceCounter(&now);
                double t = (double)(now.QuadPart - g_pstart.QuadPart) / (double)g_pfreq.QuadPart;
                gl_window_frame(g_preview, t);

                static int ticks = 0;
                char shot[MAX_PATH];
                if (++ticks == 20 && GetEnvironmentVariableA("M3DT_SHOT", shot, sizeof shot) > 0) {
                    double shot_t = 2.25;
                    char stbuf[16];
                    if (GetEnvironmentVariableA("M3DT_SHOT_T", stbuf, sizeof stbuf) > 0)
                        shot_t = atof(stbuf);
                    gl_window_render_scene_at(g_preview, shot_t);
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
                select_tab(TabCtrl_GetCurSel(GetDlgItem(h, IDC_TABS)));
                return TRUE;
            }
            break;
        case WM_PREVIEW_DIRTY:
            g_dirty = true;   /* aplicado no proximo tick do preview (debounce natural) */
            return TRUE;
        case WM_MOUSEWHEEL: {
            /* WM_MOUSEWHEEL so chega a janela com foco - a do preview nunca
               tem foco (nao e tabstop), entao tratamos aqui checando se o
               cursor esta sobre o preview no momento do evento. */
            if (!g_preview) break;
            POINT pt = { (short)LOWORD(l), (short)HIWORD(l) };   /* coordenadas de tela */
            RECT pr;
            GetWindowRect(GetDlgItem(h, IDC_PREVIEW), &pr);
            if (!PtInRect(&pr, pt)) break;
            int delta = (short)HIWORD(w);
            g_pv_manual = true;
            g_pv_zoom = g_pv_zoom * powf(1.0015f, (float)delta);
            if (g_pv_zoom < PV_ZOOM_MIN) g_pv_zoom = PV_ZOOM_MIN;
            if (g_pv_zoom > PV_ZOOM_MAX) g_pv_zoom = PV_ZOOM_MAX;
            gl_window_set_zoom(g_preview, g_pv_zoom);
            return 0;
        }
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
