#include "config_dialog.h"
#include "resource.h"
#include "config.h"
#include "gl_window.h"
#include "util/log.h"
#include "geometry/mesh_import.h"
#include "geometry/font_outline.h"
#include "i18n.h"
#include "presets.h"

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <winver.h>
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
static HWND       g_dlg;         /* dialogo principal */
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

/* ferramenta de debug visual (tipo os modos de visualizacao de material do
   Blender) - botao direito no preview alterna entre elas. So' de sessao,
   nunca persistida em Config. Escolhido botao direito (em vez de atalho de
   teclado) porque o preview nunca recebe foco de teclado (ver comentario
   de preview_wndproc mais abaixo) - qualquer tecla so' chegaria aqui se
   nenhum outro controle do dialogo estivesse com foco, o que nao da pra
   garantir. Mouse no proprio preview ja funciona de forma confiavel (o
   arrastar de orbita/pan ja usa esse canal). */
static int   g_pv_debug_view;   /* 0 normal, 1 fresnel, 2 aresta, 3 normal RGB, 4 tipo de superficie */

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
   mesmo se o cursor sair da janela durante o arrasto). Botao direito
   alterna a visualizacao de debug (g_pv_debug_view). O wheel (zoom) e
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
        case WM_RBUTTONDOWN:
            g_pv_debug_view = (g_pv_debug_view + 1) % 5;
            if (g_preview) gl_window_set_debug_view(g_preview, g_pv_debug_view);
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
static int g_cur_tab = 0;

static void preview_config_sync(void)
{
    g_preview_cfg = g_work;
    /* fora da aba Conteudo, o preview usa um texto curto fixo pro
       enquadramento (zoom fechado de Material/Geometria, texto real
       do usuario pode ser longo e estourar); na aba Conteudo o usuario
       esta literalmente editando o texto, entao o preview precisa
       refletir o que foi digitado. */
    if (g_cur_tab != 0)
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
       antes de adicionar, senao a combo fica cheia de repetidos. Tambem tira da
       lista fontes que nao renderizariam o que dizem ser (ver font_is_usable):
       variaveis, cujo negrito/italico o GDI classico nao consegue selecionar
       de verdade, ou com dados inconsistentes o bastante pra cair num
       fallback silencioso (ou pior, derrubar o processo). */
    if (lf->lfFaceName[0] != L'@' &&   /* pula as fontes verticais @Font */
        SendMessageW(cb, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)lf->lfFaceName) == CB_ERR &&
        font_is_usable(lf->lfFaceName))
        SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)lf->lfFaceName);
    return 1;
}

static void set_slider(HWND h, int id, int lo, int hi, int pos);

static void content_svg_label(HWND h)
{
    SetDlgItemTextW(h, IDC_SVGPATH, g_work.svg_path[0] ? g_work.svg_path : i18n_str(STR_PLACEHOLDER_NONE_M));
}

static void content_mesh_label(HWND h)
{
    SetDlgItemTextW(h, IDC_MESHPATH, g_work.mesh_path[0] ? g_work.mesh_path : i18n_str(STR_PLACEHOLDER_NONE_M));
    wchar_t b[32];
    swprintf(b, 32, L"%.2f", (double)g_work.mesh_size_scale);
    SetDlgItemTextW(h, IDC_MESHSCALE_VAL, b);
}

static void content_enable(HWND h)
{
    int is_text  = g_work.content_mode == CONTENT_TEXT;
    int is_clock = g_work.content_mode == CONTENT_CLOCK;
    int is_svg   = g_work.content_mode == CONTENT_SVG;
    int is_mesh  = g_work.content_mode == CONTENT_MESH;

    ShowWindow(GetDlgItem(h, IDC_TEXTLABEL), is_text ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_TEXT),      is_text ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_FONTLABEL), (is_text || is_clock) ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_FONT),      (is_text || is_clock) ? SW_SHOW : SW_HIDE);

    ShowWindow(GetDlgItem(h, IDC_SVGPATHLABEL),  is_svg ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_SVGPATH),       is_svg ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_SVGPICK),       is_svg ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_SVGCLEAR),      is_svg ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_SVGCOLORLABEL), is_svg ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_SVGCOLORMODE),  is_svg ? SW_SHOW : SW_HIDE);

    ShowWindow(GetDlgItem(h, IDC_MESHPATHLABEL),  is_mesh ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_MESHPATH),       is_mesh ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_MESHPICK),       is_mesh ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_MESHCLEAR),      is_mesh ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_MESHSCALELABEL), is_mesh ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_MESHSCALE_VAL),  is_mesh ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_MESHSCALE),      is_mesh ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(h, IDC_MESHUSEMAT),     is_mesh ? SW_SHOW : SW_HIDE);

    EnableWindow(GetDlgItem(h, IDC_CLOCKDATE), g_work.content_mode == CONTENT_CLOCK);
    EnableWindow(GetDlgItem(h, IDC_CLOCKSEC), g_work.content_mode == CONTENT_CLOCK);
}

static void filter_append(wchar_t *buf, int *pos, int cap, const wchar_t *s)
{
    int n = (int)wcslen(s);
    if (*pos + n + 1 > cap) return;
    wcscpy(buf + *pos, s);
    *pos += n + 1;
}

static void content_apply_i18n(HWND h)
{
    SetDlgItemTextW(h, IDC_MODE_LABEL, i18n_str(STR_CONTENT_MODE_LABEL));
    SetDlgItemTextW(h, IDC_TEXTLABEL, i18n_str(STR_CONTENT_TEXT_LABEL));
    SetDlgItemTextW(h, IDC_FONTLABEL, i18n_str(STR_CONTENT_FONT_LABEL));
    SetDlgItemTextW(h, IDC_SVGPATHLABEL, i18n_str(STR_COMMON_FILE_LABEL));
    SetDlgItemTextW(h, IDC_SVGPICK, i18n_str(STR_COMMON_CHOOSE));
    SetDlgItemTextW(h, IDC_SVGCLEAR, i18n_str(STR_COMMON_CLEAR));
    SetDlgItemTextW(h, IDC_SVGCOLORLABEL, i18n_str(STR_CONTENT_SVG_COLORS_LABEL));
    SetDlgItemTextW(h, IDC_MESHPATHLABEL, i18n_str(STR_COMMON_FILE_LABEL));
    SetDlgItemTextW(h, IDC_MESHPICK, i18n_str(STR_COMMON_CHOOSE));
    SetDlgItemTextW(h, IDC_MESHCLEAR, i18n_str(STR_COMMON_CLEAR));
    SetDlgItemTextW(h, IDC_MESHSCALELABEL, i18n_str(STR_CONTENT_MESH_SCALE_LABEL));
    SetDlgItemTextW(h, IDC_MESHUSEMAT, i18n_str(STR_CONTENT_MESH_USEMAT));
    SetDlgItemTextW(h, IDC_BOLD, i18n_str(STR_CONTENT_BOLD));
    SetDlgItemTextW(h, IDC_ITALIC, i18n_str(STR_CONTENT_ITALIC));
    SetDlgItemTextW(h, IDC_CLOCKDATE, i18n_str(STR_CONTENT_CLOCK_DATE));
    SetDlgItemTextW(h, IDC_CLOCKSEC, i18n_str(STR_CONTENT_CLOCK_SECONDS));
    SetDlgItemTextW(h, IDC_COLOR_LABEL, i18n_str(STR_CONTENT_COLOR_LABEL));
    SetDlgItemTextW(h, IDC_COLOR, i18n_str(STR_COMMON_CHOOSE_COLOR));

    HWND cm = GetDlgItem(h, IDC_CONTMODE);
    int cur = (int)SendMessageW(cm, CB_GETCURSEL, 0, 0);
    SendMessageW(cm, CB_RESETCONTENT, 0, 0);
    SendMessageW(cm, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_CONTENT_MODE_TEXT));
    SendMessageW(cm, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_CONTENT_MODE_CLOCK));
    SendMessageW(cm, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_CONTENT_MODE_SVG));
    SendMessageW(cm, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_CONTENT_MODE_MESH));
    SendMessageW(cm, CB_SETCURSEL, cur < 0 ? (int)g_work.content_mode : cur, 0);

    HWND svgc = GetDlgItem(h, IDC_SVGCOLORMODE);
    cur = (int)SendMessageW(svgc, CB_GETCURSEL, 0, 0);
    SendMessageW(svgc, CB_RESETCONTENT, 0, 0);
    SendMessageW(svgc, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_CONTENT_SVG_COLOR_PRESERVE));
    SendMessageW(svgc, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_CONTENT_SVG_COLOR_SINGLE));
    SendMessageW(svgc, CB_SETCURSEL, cur < 0 ? g_work.svg_color_mode : cur, 0);

    content_svg_label(h);
    content_mesh_label(h);
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
            CheckDlgButton(h, IDC_CLOCKDATE, g_work.clock_show_date ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(h, IDC_CLOCKSEC, g_work.clock_show_seconds ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(h, IDC_MESHUSEMAT, g_work.mesh_use_file_materials ? BST_CHECKED : BST_UNCHECKED);
            set_slider(h, IDC_MESHSCALE, 0, 200, (int)(g_work.mesh_size_scale * 100.0f + 0.5f));
            content_apply_i18n(h);
            content_enable(h);

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
        case WM_HSCROLL:
            g_work.mesh_size_scale =
                (float)SendDlgItemMessageW(h, IDC_MESHSCALE, TBM_GETPOS, 0, 0) / 100.0f;
            content_mesh_label(h);
            preview_dirty(h);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
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
                case IDC_SVGPICK: {
                    wchar_t file[512] = L"";
                    OPENFILENAMEW ofn;
                    memset(&ofn, 0, sizeof ofn);
                    ofn.lStructSize = sizeof ofn;
                    ofn.hwndOwner = h;
                    wchar_t filter[128]; int fp = 0;
                    filter_append(filter, &fp, 128, i18n_str(STR_FILTER_SVG));
                    filter_append(filter, &fp, 128, L"*.svg");
                    filter_append(filter, &fp, 128, i18n_str(STR_FILTER_ALL_SHORT));
                    filter_append(filter, &fp, 128, L"*.*");
                    filter[fp] = 0;
                    ofn.lpstrFilter = filter;
                    ofn.lpstrFile = file;
                    ofn.nMaxFile = 512;
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                    if (GetOpenFileNameW(&ofn)) {
                        wcsncpy(g_work.svg_path, file, 511);
                        g_work.svg_path[511] = 0;
                        content_svg_label(h);
                        preview_dirty(h);
                    }
                    break;
                }
                case IDC_SVGCLEAR:
                    g_work.svg_path[0] = 0;
                    content_svg_label(h);
                    preview_dirty(h);
                    break;
                case IDC_SVGCOLORMODE:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.svg_color_mode =
                            (int)SendDlgItemMessageW(h, IDC_SVGCOLORMODE, CB_GETCURSEL, 0, 0);
                        preview_dirty(h);
                    }
                    break;
                case IDC_MESHPICK: {
                    wchar_t file[512] = L"";
                    OPENFILENAMEW ofn;
                    memset(&ofn, 0, sizeof ofn);
                    ofn.lStructSize = sizeof ofn;
                    ofn.hwndOwner = h;
                    wchar_t filter[512]; int fp = 0;
                    filter_append(filter, &fp, 512, i18n_str(STR_FILTER_MESH_ALL));
                    filter_append(filter, &fp, 512, L"*.obj;*.stl;*.glb;*.gltf");
                    filter_append(filter, &fp, 512, i18n_str(STR_FILTER_OBJ));
                    filter_append(filter, &fp, 512, L"*.obj");
                    filter_append(filter, &fp, 512, i18n_str(STR_FILTER_STL));
                    filter_append(filter, &fp, 512, L"*.stl");
                    filter_append(filter, &fp, 512, i18n_str(STR_FILTER_GLB));
                    filter_append(filter, &fp, 512, L"*.glb");
                    filter_append(filter, &fp, 512, i18n_str(STR_FILTER_GLTF));
                    filter_append(filter, &fp, 512, L"*.gltf");
                    filter_append(filter, &fp, 512, i18n_str(STR_FILTER_ALL_LONG));
                    filter_append(filter, &fp, 512, L"*.*");
                    filter[fp] = 0;
                    ofn.lpstrFilter = filter;
                    ofn.lpstrFile = file;
                    ofn.nMaxFile = 512;
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                    if (GetOpenFileNameW(&ofn)) {
                        if (mesh_import_file_too_big(file)) {
                            MessageBoxW(h, i18n_str(STR_MSG_FILE_TOO_BIG_TEXT),
                                        i18n_str(STR_MSG_FILE_TOO_BIG_TITLE),
                                        MB_OK | MB_ICONWARNING);
                        } else {
                            wcsncpy(g_work.mesh_path, file, 511);
                            g_work.mesh_path[511] = 0;
                            content_mesh_label(h);
                            preview_dirty(h);
                        }
                    }
                    break;
                }
                case IDC_MESHCLEAR:
                    g_work.mesh_path[0] = 0;
                    content_mesh_label(h);
                    preview_dirty(h);
                    break;
                case IDC_MESHUSEMAT:
                    g_work.mesh_use_file_materials =
                        (IsDlgButtonChecked(h, IDC_MESHUSEMAT) == BST_CHECKED);
                    preview_dirty(h);
                    break;
                case IDC_TEXT:
                    if (HIWORD(w) == EN_CHANGE) {
                        wchar_t wtext[512];
                        GetDlgItemTextW(h, IDC_TEXT, wtext, 512);
                        /* controles multilinha do Win32 devolvem \r\n nas quebras
                           de linha; sem remover o \r ele sobra como um codepoint
                           sem glifo real (a fonte desenha o .notdef, um retangulo
                           quebrado) entre o fim de uma linha e o inicio da outra */
                        wchar_t *src = wtext, *dst = wtext;
                        while (*src) { if (*src != L'\r') *dst++ = *src; src++; }
                        *dst = 0;
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

static void motion_apply_i18n(HWND h)
{
    SetDlgItemTextW(h, IDC_ANGLE_LABEL, i18n_str(STR_MOTION_ANGLE_LABEL));
    SetDlgItemTextW(h, IDC_TILT_LABEL, i18n_str(STR_MOTION_TILT_LABEL));
    SetDlgItemTextW(h, IDC_PERIOD_LABEL, i18n_str(STR_MOTION_PERIOD_LABEL));
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
            motion_apply_i18n(h);
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
    swprintf(b, 32, L"%.2f", (double)g_work.emissive_amount); SetDlgItemTextW(h, IDC_EMISSIVE_VAL, b);
    swprintf(b, 32, L"%.2f", (double)g_work.edge_bias); SetDlgItemTextW(h, IDC_EDGEBIAS_VAL, b);
    swprintf(b, 32, L"%.1fpx", (double)g_work.wireframe_thickness); SetDlgItemTextW(h, IDC_WIRE_THICK_VAL, b);
    SetDlgItemTextW(h, IDC_ENVPATH, g_work.env_path[0] ? g_work.env_path : i18n_str(STR_PLACEHOLDER_PROCEDURAL));
}

static void material_apply_i18n(HWND h)
{
    SetDlgItemTextW(h, IDC_MATERIAL_LABEL, i18n_str(STR_MATERIAL_LABEL));
    SetDlgItemTextW(h, IDC_METAL_LABEL, i18n_str(STR_MATERIAL_METALNESS_LABEL));
    SetDlgItemTextW(h, IDC_ROUGH_LABEL, i18n_str(STR_MATERIAL_ROUGHNESS_LABEL));
    SetDlgItemTextW(h, IDC_EMISSIVE_LABEL, i18n_str(STR_MATERIAL_EMISSIVE_LABEL));
    SetDlgItemTextW(h, IDC_EMISSIVE_COLOR, i18n_str(STR_MATERIAL_EMISSIVE_COLOR_BTN));
    SetDlgItemTextW(h, IDC_EDGEBIAS_LABEL, i18n_str(STR_MATERIAL_EDGEBIAS_LABEL));
    SetDlgItemTextW(h, IDC_WIRE_THICK_LABEL, i18n_str(STR_MATERIAL_WIRE_THICKNESS_LABEL));
    SetDlgItemTextW(h, IDC_WIRE_XRAY, i18n_str(STR_MATERIAL_WIRE_XRAY));
    SetDlgItemTextW(h, IDC_ENV_LABEL, i18n_str(STR_MATERIAL_ENV_LABEL));
    SetDlgItemTextW(h, IDC_ENVMODE_EMBED, i18n_str(STR_MATERIAL_ENV_MODE_EMBEDDED));
    SetDlgItemTextW(h, IDC_ENVMODE_CUSTOM, i18n_str(STR_MATERIAL_ENV_MODE_CUSTOM));
    SetDlgItemTextW(h, IDC_ENVMODE_NONE, i18n_str(STR_MATERIAL_ENV_MODE_NONE));
    SetDlgItemTextW(h, IDC_ENVPICK, i18n_str(STR_COMMON_CHOOSE));
    SetDlgItemTextW(h, IDC_ENVCLEAR, i18n_str(STR_COMMON_CLEAR));

    HWND cb = GetDlgItem(h, IDC_MATMODE);
    int cur = (int)SendMessageW(cb, CB_GETCURSEL, 0, 0);
    SendMessageW(cb, CB_RESETCONTENT, 0, 0);
    SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_MATERIAL_MODE_CLASSIC));
    SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_MATERIAL_MODE_METALLIC));
    SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_MATERIAL_MODE_GLASS));
    SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_MATERIAL_MODE_WIREFRAME));
    SendMessageW(cb, CB_SETCURSEL, cur < 0 ? g_work.material_mode : cur, 0);

    material_labels(h);
}

/* blocos da aba Material, na ordem em que ja aparecem no .rc - cada
   propriedade so' faz sentido (e so' tem efeito real no
   shaders/model.frag) em alguns modos:
   Metalizacao: so' Metalico.
   Rugosidade: Classico, Metalico e Vidro (todos os modos).
   Emissivo: Classico e Vidro (Metalico ja' reflete o ambiente, brilho
   proprio por cima ficaria estranho).
   Vies aresta/plano: so' Vidro (controla o quanto a aspereza aresta/
   plano do Vidro pende pra transparente ou opaco).
   Ambiente: Metalico e Vidro (unico jeito de refletir alguma coisa).
   (Fosco foi removido - feedback do usuario apos a aspereza do
   Classico passar a cobrir liso->fosco de verdade, o Fosco separado
   ficou redundante.)
   (Verniz e Anisotropia existiram brevemente mas foram removidos -
   feedback do usuario apos testar: efeito pouco distintivo pra
   justificar mais 2 controles na UI. Depois, uma Distorcao/refracao
   real do Vidro tambem foi tentada e removida - mesmo depois de
   corrigir 3 bugs reais na implementacao (formula presa as bordas,
   filtro invalido de blit MSAA, layout quebrado), o resultado nunca
   convenceu o usuario como efeito visual - abandonado de vez.)
   (Cada bloco tem sua PROPRIA posicao original no .rc - dois blocos
   compartilhando coordenadas ja causou um bug real: o avanco de
   cursor do bloco anterior zerava, empurrando o bloco seguinte pra
   cima do bloco compartilhado. NUNCA reaproveitar coordenadas entre
   blocos por economia de espaco.) */
typedef struct { int id; int x, rel_y; } MatCtrl;

#define MAT_BLOCKS 8
static MatCtrl g_mat_blocks[MAT_BLOCKS][4] = {
    { { IDC_METAL_LABEL, 0, 0 }, { IDC_METAL_VAL, 0, 0 }, { IDC_METAL, 0, 0 } },
    { { IDC_ROUGH_LABEL, 0, 0 }, { IDC_ROUGH_VAL, 0, 0 }, { IDC_ROUGH, 0, 0 } },
    { { IDC_EMISSIVE_LABEL, 0, 0 }, { IDC_EMISSIVE_COLOR, 0, 0 }, { IDC_EMISSIVE_VAL, 0, 0 }, { IDC_EMISSIVE, 0, 0 } },
    { { IDC_EDGEBIAS_LABEL, 0, 0 }, { IDC_EDGEBIAS_VAL, 0, 0 }, { IDC_EDGEBIAS, 0, 0 } },
    { { IDC_WIRE_THICK_LABEL, 0, 0 }, { IDC_WIRE_THICK_VAL, 0, 0 }, { IDC_WIRE_THICK, 0, 0 } },
    { { IDC_WIRE_XRAY, 0, 0 } },
    { { IDC_ENV_LABEL, 0, 0 }, { IDC_ENVMODE_EMBED, 0, 0 }, { IDC_ENVMODE_CUSTOM, 0, 0 }, { IDC_ENVMODE_NONE, 0, 0 } },
    { { IDC_ENVPATH, 0, 0 }, { IDC_ENVPICK, 0, 0 }, { IDC_ENVCLEAR, 0, 0 } },
};
static const int MAT_BLOCK_N[MAT_BLOCKS] = { 3, 3, 4, 3, 3, 1, 4, 3 };
static int g_mat_block_top[MAT_BLOCKS];
static int g_mat_block_h[MAT_BLOCKS];
static int g_mat_gap = 0;   /* espacamento uniforme entre blocos - todo o .rc usa o
                               mesmo ritmo (6 DU); um unico valor, em vez de um gap
                               por par original, evita o bug de blocos que
                               compartilham coordenadas "zerarem" o avanco do
                               cursor pro bloco seguinte (ja aconteceu de verdade -
                               ver comentario acima). */
static int g_mat_layout_ready = 0;

/* captura a posicao ORIGINAL (em pixels, ja resolvida do .rc) de cada
   controle - roda uma unica vez, antes de qualquer ocultacao, entao
   sempre reflete a disposicao estatica original do .rc */
static void material_layout_capture(HWND h)
{
    if (g_mat_layout_ready) return;

    for (int b = 0; b < MAT_BLOCKS; ++b) {
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
    /* Metalizacao (0) e Rugosidade (1) nunca compartilham posicao com
       nada - par confiavel pra medir o ritmo real do .rc. */
    g_mat_gap = g_mat_block_top[1] - (g_mat_block_top[0] + g_mat_block_h[0]);
    g_mat_layout_ready = 1;
}

/* esconde os blocos irrelevantes pro modo atual e empilha os visiveis
   a partir do topo original do primeiro bloco, preservando o mesmo
   espacamento entre blocos que o .rc ja tinha */
static void material_layout_apply(HWND h)
{
    int mode = g_work.material_mode;
    int vis_metal      = (mode == 1);
    int vis_rough      = (mode == 0 || mode == 1 || mode == 2);
    int vis_emissive   = (mode == 0 || mode == 2 || mode == 3);
    int vis_edgebias   = (mode == 2);
    int vis_wire_thick = (mode == 3);
    int vis_wire_xray  = (mode == 3);
    int vis_env_hdr    = (mode == 1 || mode == 2);
    int vis_env_pick   = vis_env_hdr && (g_work.env_mode == 1);
    int visible[MAT_BLOCKS] = { vis_metal, vis_rough, vis_emissive, vis_edgebias,
                                 vis_wire_thick, vis_wire_xray, vis_env_hdr, vis_env_pick };

    int cursor = g_mat_block_top[0];
    for (int b = 0; b < MAT_BLOCKS; ++b) {
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
        cursor += g_mat_block_h[b] + g_mat_gap;
    }
}

static INT_PTR CALLBACK material_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            set_slider(h, IDC_METAL, 0, 100, (int)(g_work.metalness * 100.0f + 0.5f));
            set_slider(h, IDC_ROUGH, 0, 100, (int)(g_work.roughness * 100.0f + 0.5f));
            set_slider(h, IDC_EMISSIVE, 0, 100, (int)(g_work.emissive_amount * 100.0f + 0.5f));
            set_slider(h, IDC_EDGEBIAS, 0, 100, (int)(g_work.edge_bias * 100.0f + 0.5f));
            set_slider(h, IDC_WIRE_THICK, 10, 60, (int)(g_work.wireframe_thickness * 10.0f + 0.5f));
            material_apply_i18n(h);
            CheckRadioButton(h, IDC_ENVMODE_EMBED, IDC_ENVMODE_NONE,
                              g_work.env_mode == 1 ? IDC_ENVMODE_CUSTOM :
                              g_work.env_mode == 2 ? IDC_ENVMODE_NONE : IDC_ENVMODE_EMBED);
            CheckDlgButton(h, IDC_WIRE_XRAY, g_work.wireframe_xray ? BST_CHECKED : BST_UNCHECKED);
            material_layout_capture(h);
            material_layout_apply(h);
            return TRUE;
        }
        case WM_HSCROLL:
            g_work.metalness = (float)SendDlgItemMessageW(h, IDC_METAL, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.roughness = (float)SendDlgItemMessageW(h, IDC_ROUGH, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.emissive_amount = (float)SendDlgItemMessageW(h, IDC_EMISSIVE, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.edge_bias = (float)SendDlgItemMessageW(h, IDC_EDGEBIAS, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.wireframe_thickness = (float)SendDlgItemMessageW(h, IDC_WIRE_THICK, TBM_GETPOS, 0, 0) / 10.0f;
            material_labels(h);
            preview_dirty(h);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDC_MATMODE:
                    if (HIWORD(w) == CBN_SELCHANGE) {
                        g_work.material_mode =
                            (int)SendDlgItemMessageW(h, IDC_MATMODE, CB_GETCURSEL, 0, 0);
                        material_layout_apply(h);
                        preview_dirty(h);
                    }
                    break;
                case IDC_EMISSIVE_COLOR: {
                    static COLORREF custom[16];
                    CHOOSECOLORW cc;
                    memset(&cc, 0, sizeof cc);
                    cc.lStructSize = sizeof cc;
                    cc.hwndOwner = h;
                    cc.lpCustColors = custom;
                    cc.rgbResult = RGB((int)(g_work.emissive_r * 255.0f),
                                       (int)(g_work.emissive_g * 255.0f),
                                       (int)(g_work.emissive_b * 255.0f));
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColorW(&cc)) {
                        g_work.emissive_r = GetRValue(cc.rgbResult) / 255.0f;
                        g_work.emissive_g = GetGValue(cc.rgbResult) / 255.0f;
                        g_work.emissive_b = GetBValue(cc.rgbResult) / 255.0f;
                        preview_dirty(h);
                    }
                    break;
                }
                case IDC_ENVPICK: {
                    wchar_t file[512] = L"";
                    OPENFILENAMEW ofn;
                    memset(&ofn, 0, sizeof ofn);
                    ofn.lStructSize = sizeof ofn;
                    ofn.hwndOwner = h;
                    wchar_t filter[128]; int fp = 0;
                    filter_append(filter, &fp, 128, i18n_str(STR_FILTER_IMAGES));
                    filter_append(filter, &fp, 128, L"*.jpg;*.jpeg;*.png;*.bmp;*.tga");
                    filter_append(filter, &fp, 128, i18n_str(STR_FILTER_ALL_SHORT));
                    filter_append(filter, &fp, 128, L"*.*");
                    filter[fp] = 0;
                    ofn.lpstrFilter = filter;
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
                case IDC_WIRE_XRAY:
                    g_work.wireframe_xray = (IsDlgButtonChecked(h, IDC_WIRE_XRAY) == BST_CHECKED);
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
    EnableWindow(GetDlgItem(h, IDC_BSEG), g_work.bevel_mode == 0 || g_work.bevel_mode == 1);
    EnableWindow(GetDlgItem(h, IDC_WALL), g_work.shell != 0);
}

static void geometry_apply_i18n(HWND h)
{
    SetDlgItemTextW(h, IDC_DEPTH_LABEL, i18n_str(STR_GEOMETRY_DEPTH_LABEL));
    SetDlgItemTextW(h, IDC_BEVEL_LABEL, i18n_str(STR_GEOMETRY_BEVEL_LABEL));
    SetDlgItemTextW(h, IDC_BSIZE_LABEL, i18n_str(STR_GEOMETRY_BSIZE_LABEL));
    SetDlgItemTextW(h, IDC_BDEPTH_LABEL, i18n_str(STR_GEOMETRY_BDEPTH_LABEL));
    SetDlgItemTextW(h, IDC_BSEG_LABEL, i18n_str(STR_GEOMETRY_BSEG_LABEL));
    SetDlgItemTextW(h, IDC_SHELL, i18n_str(STR_GEOMETRY_SHELL));
    SetDlgItemTextW(h, IDC_WALL_LABEL, i18n_str(STR_GEOMETRY_WALL_LABEL));
    SetDlgItemTextW(h, IDC_QUALITY_LABEL, i18n_str(STR_GEOMETRY_QUALITY_LABEL));

    HWND bev = GetDlgItem(h, IDC_BEVELMODE);
    int cur = (int)SendMessageW(bev, CB_GETCURSEL, 0, 0);
    SendMessageW(bev, CB_RESETCONTENT, 0, 0);
    SendMessageW(bev, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_GEOMETRY_BEVEL_ROUNDED));
    SendMessageW(bev, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_GEOMETRY_BEVEL_GEOMETRIC));
    SendMessageW(bev, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_GEOMETRY_BEVEL_OFF));
    SendMessageW(bev, CB_SETCURSEL, cur < 0 ? g_work.bevel_mode : cur, 0);

    HWND qual = GetDlgItem(h, IDC_QUALITY);
    cur = (int)SendMessageW(qual, CB_GETCURSEL, 0, 0);
    SendMessageW(qual, CB_RESETCONTENT, 0, 0);
    SendMessageW(qual, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_GEOMETRY_QUALITY_LOW));
    SendMessageW(qual, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_GEOMETRY_QUALITY_MEDIUM));
    SendMessageW(qual, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_GEOMETRY_QUALITY_HIGH));
    SendMessageW(qual, CB_SETCURSEL, cur < 0 ? g_work.quality : cur, 0);
}

static INT_PTR CALLBACK geometry_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            set_slider(h, IDC_DEPTH,  2, 200, (int)(g_work.depth * 100.0f + 0.5f));
            set_slider(h, IDC_BSIZE,  0, 200, (int)(g_work.bevel_size * 1000.0f + 0.5f));
            set_slider(h, IDC_BDEPTH, 0, 200, (int)(g_work.bevel_depth * 1000.0f + 0.5f));
            set_slider(h, IDC_BSEG,   2, 8,   g_work.bevel_segments);
            set_slider(h, IDC_WALL,   10, 200, (int)(g_work.wall_thickness * 1000.0f + 0.5f));
            CheckDlgButton(h, IDC_SHELL, g_work.shell ? BST_CHECKED : BST_UNCHECKED);
            geometry_apply_i18n(h);
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

static void effects_apply_i18n(HWND h)
{
    SetDlgItemTextW(h, IDC_BLOOM, i18n_str(STR_EFFECTS_BLOOM));
    SetDlgItemTextW(h, IDC_BTHRESH_LABEL, i18n_str(STR_EFFECTS_THRESHOLD_LABEL));
    SetDlgItemTextW(h, IDC_BINT_LABEL, i18n_str(STR_EFFECTS_BLOOM_INTENSITY_LABEL));
    SetDlgItemTextW(h, IDC_BRAD_LABEL, i18n_str(STR_EFFECTS_SPREAD_LABEL));
    SetDlgItemTextW(h, IDC_STREAKS_LABEL, i18n_str(STR_EFFECTS_STREAKS_LABEL));
    SetDlgItemTextW(h, IDC_SINT_LABEL, i18n_str(STR_EFFECTS_STREAKS_INTENSITY_LABEL));
    SetDlgItemTextW(h, IDC_SLEN_LABEL, i18n_str(STR_EFFECTS_LENGTH_LABEL));
    SetDlgItemTextW(h, IDC_EFFECTS_HINT, i18n_str(STR_EFFECTS_HINT));

    HWND sm = GetDlgItem(h, IDC_STREAKMODE);
    int cur = (int)SendMessageW(sm, CB_GETCURSEL, 0, 0);
    SendMessageW(sm, CB_RESETCONTENT, 0, 0);
    SendMessageW(sm, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_EFFECTS_STREAKS_OFF));
    SendMessageW(sm, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_EFFECTS_STREAKS_STARBURST));
    SendMessageW(sm, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_EFFECTS_STREAKS_ANAMORPHIC));
    SendMessageW(sm, CB_SETCURSEL, cur < 0 ? g_work.streaks_mode : cur, 0);
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
            set_slider(h, IDC_SINT, 0, 200, (int)(g_work.streaks_intensity * 100.0f + 0.5f));
            set_slider(h, IDC_SLEN, 0, 100, (int)(g_work.streaks_length * 100.0f + 0.5f));
            effects_apply_i18n(h);
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

static void perf_apply_i18n(HWND h)
{
    SetDlgItemTextW(h, IDC_FPS_LABEL, i18n_str(STR_PERF_FPS_LABEL));
    SetDlgItemTextW(h, IDC_VSYNC, i18n_str(STR_PERF_VSYNC));
    SetDlgItemTextW(h, IDC_MSAA_LABEL, i18n_str(STR_PERF_MSAA_LABEL));
    SetDlgItemTextW(h, IDC_RSCALE_LABEL, i18n_str(STR_PERF_RSCALE_LABEL));
    SetDlgItemTextW(h, IDC_AUTOQ, i18n_str(STR_PERF_AUTOQ));
    SetDlgItemTextW(h, IDC_PERF_HINT, i18n_str(STR_PERF_HINT));

    HWND fps = GetDlgItem(h, IDC_FPSCAP);
    int cur = (int)SendMessageW(fps, CB_GETCURSEL, 0, 0);
    SendMessageW(fps, CB_RESETCONTENT, 0, 0);
    SendMessageW(fps, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_PERF_FPS_UNLIMITED));
    SendMessageW(fps, CB_ADDSTRING, 0, (LPARAM)L"30");
    SendMessageW(fps, CB_ADDSTRING, 0, (LPARAM)L"60");
    SendMessageW(fps, CB_ADDSTRING, 0, (LPARAM)L"120");
    SendMessageW(fps, CB_SETCURSEL, cur < 0 ? 2 : cur, 0);

    HWND msaa = GetDlgItem(h, IDC_MSAA);
    cur = (int)SendMessageW(msaa, CB_GETCURSEL, 0, 0);
    SendMessageW(msaa, CB_RESETCONTENT, 0, 0);
    SendMessageW(msaa, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_PERF_MSAA_OFF));
    SendMessageW(msaa, CB_ADDSTRING, 0, (LPARAM)L"2x");
    SendMessageW(msaa, CB_ADDSTRING, 0, (LPARAM)L"4x");
    SendMessageW(msaa, CB_ADDSTRING, 0, (LPARAM)L"8x");
    SendMessageW(msaa, CB_SETCURSEL, cur < 0 ? 2 : cur, 0);

    perf_labels(h);
}

static void apply_language_change(void);

static INT_PTR CALLBACK perf_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            CheckDlgButton(h, IDC_VSYNC, g_work.vsync ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(h, IDC_AUTOQ, g_work.auto_quality ? BST_CHECKED : BST_UNCHECKED);
            set_slider(h, IDC_RSCALE, 50, 100, (int)(g_work.render_scale * 100.0f + 0.5f));
            perf_apply_i18n(h);
            int fi = 2, mi = 2;
            for (int i = 0; i < 4; ++i) {
                if (PERF_FPS[i] == g_work.fps_cap) fi = i;
                if (PERF_MSAA[i] == g_work.msaa)   mi = i;
            }
            SendDlgItemMessageW(h, IDC_FPSCAP, CB_SETCURSEL, fi, 0);
            SendDlgItemMessageW(h, IDC_MSAA,   CB_SETCURSEL, mi, 0);
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

static void post_apply_i18n(HWND h)
{
    SetDlgItemTextW(h, IDC_CHROMA, i18n_str(STR_POST_CHROMA));
    SetDlgItemTextW(h, IDC_CSTR_LABEL, i18n_str(STR_POST_CHROMA_INTENSITY_LABEL));
    SetDlgItemTextW(h, IDC_VIGNETTE, i18n_str(STR_POST_VIGNETTE));
    SetDlgItemTextW(h, IDC_VAMT_LABEL, i18n_str(STR_POST_VIGNETTE_INTENSITY_LABEL));
    SetDlgItemTextW(h, IDC_FXAA, i18n_str(STR_POST_FXAA));
    SetDlgItemTextW(h, IDC_POST_HINT, i18n_str(STR_POST_HINT));
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
            post_apply_i18n(h);
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
    SetDlgItemTextW(h, IDC_BGIMGPATH, g_work.bg_image_path[0] ? g_work.bg_image_path : i18n_str(STR_PLACEHOLDER_NONE_F));
}

/* 4 blocos da aba Fundo, na ordem em que ja aparecem no .rc - Cor 1 e'
   compartilhada entre Solido e Gradiente (unico campo com conflito
   real de valor-alvo entre tipos, ver spec), os demais sao exclusivos
   de um tipo so */
typedef struct { int id; int x, rel_y; } BgCtrl;

static BgCtrl g_bg_blocks[5][9] = {
    { { IDC_BGCOLOR1_LABEL, 0, 0 }, { IDC_BGCOLOR1, 0, 0 } },
    { { IDC_BGCOLOR2_LABEL, 0, 0 }, { IDC_BGCOLOR2, 0, 0 },
      { IDC_BGANGLE_LABEL, 0, 0 }, { IDC_BGANGLE_VAL, 0, 0 }, { IDC_BGANGLE, 0, 0 } },
    { { IDC_BGIMAGE_LABEL, 0, 0 }, { IDC_BGIMGPATH, 0, 0 }, { IDC_BGIMGPICK, 0, 0 },
      { IDC_BGIMGCLEAR, 0, 0 }, { IDC_BGFIT_LABEL, 0, 0 }, { IDC_BGFIT, 0, 0 },
      { IDC_BGPAN_LABEL, 0, 0 }, { IDC_BGPAN_VAL, 0, 0 }, { IDC_BGPAN, 0, 0 } },
    { { IDC_BGNEBULA_LABEL, 0, 0 }, { IDC_BGNEBCOLOR1, 0, 0 }, { IDC_BGNEBCOLOR2, 0, 0 } },
    { { IDC_BGGRID_LABEL, 0, 0 }, { IDC_BGGRIDCOLOR1, 0, 0 }, { IDC_BGGRIDCOLOR2, 0, 0 } },
};
static const int BG_BLOCK_N[5] = { 2, 5, 9, 3, 3 };
static int g_bg_block_top[5];
static int g_bg_block_h[5];
static int g_bg_gap_after[4];
static int g_bg_layout_ready = 0;

static void bg_layout_capture(HWND h)
{
    if (g_bg_layout_ready) return;

    for (int b = 0; b < 5; ++b) {
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
    for (int b = 0; b < 4; ++b)
        g_bg_gap_after[b] = g_bg_block_top[b + 1] - (g_bg_block_top[b] + g_bg_block_h[b]);
    g_bg_layout_ready = 1;
}

static void bg_layout_apply(HWND h)
{
    int t = g_work.background_type;
    int visible[5] = { t == 0 || t == 1, t == 1, t == 2, t == 3, t == 4 };

    int cursor = g_bg_block_top[0];
    for (int b = 0; b < 5; ++b) {
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
        cursor += g_bg_block_h[b] + (b < 4 ? g_bg_gap_after[b] : 0);
    }
}

static void bg_apply_i18n(HWND h)
{
    SetDlgItemTextW(h, IDC_BGTYPE_LABEL, i18n_str(STR_BG_TYPE_LABEL));
    SetDlgItemTextW(h, IDC_BGCOLOR1_LABEL, i18n_str(STR_BG_COLOR1_LABEL));
    SetDlgItemTextW(h, IDC_BGCOLOR1, i18n_str(STR_COMMON_CHOOSE_COLOR));
    SetDlgItemTextW(h, IDC_BGCOLOR2_LABEL, i18n_str(STR_BG_COLOR2_LABEL));
    SetDlgItemTextW(h, IDC_BGCOLOR2, i18n_str(STR_COMMON_CHOOSE_COLOR));
    SetDlgItemTextW(h, IDC_BGANGLE_LABEL, i18n_str(STR_BG_ANGLE_LABEL));
    SetDlgItemTextW(h, IDC_BGIMAGE_LABEL, i18n_str(STR_BG_IMAGE_LABEL));
    SetDlgItemTextW(h, IDC_BGIMGPICK, i18n_str(STR_COMMON_CHOOSE));
    SetDlgItemTextW(h, IDC_BGIMGCLEAR, i18n_str(STR_COMMON_CLEAR));
    SetDlgItemTextW(h, IDC_BGFIT_LABEL, i18n_str(STR_BG_FIT_LABEL));
    SetDlgItemTextW(h, IDC_BGPAN_LABEL, i18n_str(STR_BG_PAN_LABEL));
    SetDlgItemTextW(h, IDC_BGNEBULA_LABEL, i18n_str(STR_BG_NEBULA_LABEL));
    SetDlgItemTextW(h, IDC_BGNEBCOLOR1, i18n_str(STR_BG_NEBULA_COLOR1_BTN));
    SetDlgItemTextW(h, IDC_BGNEBCOLOR2, i18n_str(STR_BG_NEBULA_COLOR2_BTN));
    SetDlgItemTextW(h, IDC_BGGRID_LABEL, i18n_str(STR_BG_GRID_LABEL));
    SetDlgItemTextW(h, IDC_BGGRIDCOLOR1, i18n_str(STR_BG_GRID_COLOR1_BTN));
    SetDlgItemTextW(h, IDC_BGGRIDCOLOR2, i18n_str(STR_BG_GRID_COLOR2_BTN));

    HWND ty = GetDlgItem(h, IDC_BGTYPE);
    int cur = (int)SendMessageW(ty, CB_GETCURSEL, 0, 0);
    SendMessageW(ty, CB_RESETCONTENT, 0, 0);
    SendMessageW(ty, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_BG_TYPE_SOLID));
    SendMessageW(ty, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_BG_TYPE_GRADIENT));
    SendMessageW(ty, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_BG_TYPE_IMAGE));
    SendMessageW(ty, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_BG_TYPE_NEBULA));
    SendMessageW(ty, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_BG_TYPE_GRID));
    SendMessageW(ty, CB_SETCURSEL, cur < 0 ? g_work.background_type : cur, 0);

    HWND fit = GetDlgItem(h, IDC_BGFIT);
    cur = (int)SendMessageW(fit, CB_GETCURSEL, 0, 0);
    SendMessageW(fit, CB_RESETCONTENT, 0, 0);
    SendMessageW(fit, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_BG_FIT_COVER));
    SendMessageW(fit, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_BG_FIT_CONTAIN));
    SendMessageW(fit, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_BG_FIT_TILE));
    SendMessageW(fit, CB_SETCURSEL, cur < 0 ? g_work.bg_image_fit : cur, 0);

    bg_labels(h);
}

static INT_PTR CALLBACK bg_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            set_slider(h, IDC_BGANGLE, 0, 360, (int)(g_work.bg_grad_angle + 0.5f));
            set_slider(h, IDC_BGPAN, 0, 100, (int)(g_work.bg_pan_speed * 100.0f + 0.5f));
            bg_apply_i18n(h);
            bg_layout_capture(h);
            bg_layout_apply(h);
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
                        if (g_work.background_type == 0) g_work.bg_solid_customized = 1;
                        else if (g_work.background_type == 1) g_work.bg_gradient_customized = 1;
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
                case IDC_BGGRIDCOLOR1: {
                    static COLORREF custom[16];
                    CHOOSECOLORW cc;
                    memset(&cc, 0, sizeof cc);
                    cc.lStructSize = sizeof cc;
                    cc.hwndOwner = h;
                    cc.lpCustColors = custom;
                    cc.rgbResult = RGB((int)(g_work.bg_grid_color1_r * 255.0f),
                                       (int)(g_work.bg_grid_color1_g * 255.0f),
                                       (int)(g_work.bg_grid_color1_b * 255.0f));
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColorW(&cc)) {
                        g_work.bg_grid_color1_r = GetRValue(cc.rgbResult) / 255.0f;
                        g_work.bg_grid_color1_g = GetGValue(cc.rgbResult) / 255.0f;
                        g_work.bg_grid_color1_b = GetBValue(cc.rgbResult) / 255.0f;
                        preview_dirty(h);
                    }
                    break;
                }
                case IDC_BGGRIDCOLOR2: {
                    static COLORREF custom[16];
                    CHOOSECOLORW cc;
                    memset(&cc, 0, sizeof cc);
                    cc.lStructSize = sizeof cc;
                    cc.hwndOwner = h;
                    cc.lpCustColors = custom;
                    cc.rgbResult = RGB((int)(g_work.bg_grid_color2_r * 255.0f),
                                       (int)(g_work.bg_grid_color2_g * 255.0f),
                                       (int)(g_work.bg_grid_color2_b * 255.0f));
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColorW(&cc)) {
                        g_work.bg_grid_color2_r = GetRValue(cc.rgbResult) / 255.0f;
                        g_work.bg_grid_color2_g = GetGValue(cc.rgbResult) / 255.0f;
                        g_work.bg_grid_color2_b = GetBValue(cc.rgbResult) / 255.0f;
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
                    wchar_t filter[128]; int fp = 0;
                    filter_append(filter, &fp, 128, i18n_str(STR_FILTER_IMAGES));
                    filter_append(filter, &fp, 128, L"*.jpg;*.jpeg;*.png;*.bmp;*.tga");
                    filter_append(filter, &fp, 128, i18n_str(STR_FILTER_ALL_SHORT));
                    filter_append(filter, &fp, 128, L"*.*");
                    filter[fp] = 0;
                    ofn.lpstrFilter = filter;
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

static void particles_apply_i18n(HWND h)
{
    SetDlgItemTextW(h, IDC_PARTON, i18n_str(STR_PARTICLES_ENABLE));
    SetDlgItemTextW(h, IDC_PARTKIND_LABEL, i18n_str(STR_PARTICLES_KIND_LABEL));
    SetDlgItemTextW(h, IDC_PARTDENS_LABEL, i18n_str(STR_PARTICLES_DENSITY_LABEL));
    SetDlgItemTextW(h, IDC_PARTSPEED_LABEL, i18n_str(STR_PARTICLES_SPEED_LABEL));
    SetDlgItemTextW(h, IDC_PARTSIZE_LABEL, i18n_str(STR_PARTICLES_SIZE_LABEL));
    SetDlgItemTextW(h, IDC_PARTOPACITY_LABEL, i18n_str(STR_PARTICLES_OPACITY_LABEL));

    HWND k = GetDlgItem(h, IDC_PARTKIND);
    int cur = (int)SendMessageW(k, CB_GETCURSEL, 0, 0);
    SendMessageW(k, CB_RESETCONTENT, 0, 0);
    SendMessageW(k, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_PARTICLES_KIND_DUST));
    SendMessageW(k, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_PARTICLES_KIND_BOKEH));
    SendMessageW(k, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_PARTICLES_KIND_SPARKS));
    SendMessageW(k, CB_ADDSTRING, 0, (LPARAM)i18n_str(STR_PARTICLES_KIND_STARS));
    SendMessageW(k, CB_SETCURSEL, cur < 0 ? g_work.particles_kind : cur, 0);

    particles_labels(h);
}

/* Carrega, nos campos "atuais" (usados pelo renderer), a memoria propria
 * do tipo selecionado - cada tipo guarda sua propria Densidade/Tamanho/
 * Opacidade, entao um ajuste manual num tipo nunca e' sobrescrito ao
 * visitar outro tipo e voltar. Velocidade nao tem memoria por tipo (e'
 * 0.76 em todos, sem conflito). */
static void particles_apply_kind_default(HWND h)
{
    switch (g_work.particles_kind) {
        case 0: /* Poeira */
            g_work.particles_density = g_work.particles_dust_density;
            g_work.particles_size_scale = g_work.particles_dust_size;
            g_work.particles_opacity = g_work.particles_dust_opacity;
            break;
        case 1: /* Bokeh */
            g_work.particles_density = g_work.particles_bokeh_density;
            g_work.particles_size_scale = g_work.particles_bokeh_size;
            g_work.particles_opacity = g_work.particles_bokeh_opacity;
            break;
        case 2: /* Faiscas */
            g_work.particles_density = g_work.particles_sparks_density;
            g_work.particles_size_scale = g_work.particles_sparks_size;
            g_work.particles_opacity = g_work.particles_sparks_opacity;
            break;
        case 3: /* Estrelas */
            g_work.particles_density = g_work.particles_stars_density;
            g_work.particles_size_scale = g_work.particles_stars_size;
            g_work.particles_opacity = g_work.particles_stars_opacity;
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

static INT_PTR CALLBACK particles_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG: {
            CheckDlgButton(h, IDC_PARTON, g_work.particles_on ? BST_CHECKED : BST_UNCHECKED);
            set_slider(h, IDC_PARTDENS, 0, 100, (int)(g_work.particles_density * 100.0f + 0.5f));
            set_slider(h, IDC_PARTSPEED, 0, 200, (int)(g_work.particles_speed * 100.0f + 0.5f));
            set_slider(h, IDC_PARTSIZE, 0, 200, (int)(g_work.particles_size_scale * 100.0f + 0.5f));
            set_slider(h, IDC_PARTOPACITY, 0, 200, (int)(g_work.particles_opacity * 100.0f + 0.5f));
            particles_apply_i18n(h);
            particles_enable(h);
            return TRUE;
        }
        case WM_HSCROLL:
            g_work.particles_density    = (float)SendDlgItemMessageW(h, IDC_PARTDENS, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.particles_speed      = (float)SendDlgItemMessageW(h, IDC_PARTSPEED, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.particles_size_scale = (float)SendDlgItemMessageW(h, IDC_PARTSIZE, TBM_GETPOS, 0, 0) / 100.0f;
            g_work.particles_opacity    = (float)SendDlgItemMessageW(h, IDC_PARTOPACITY, TBM_GETPOS, 0, 0) / 100.0f;
            switch (g_work.particles_kind) {
                case 0:
                    g_work.particles_dust_density = g_work.particles_density;
                    g_work.particles_dust_size = g_work.particles_size_scale;
                    g_work.particles_dust_opacity = g_work.particles_opacity;
                    break;
                case 1:
                    g_work.particles_bokeh_density = g_work.particles_density;
                    g_work.particles_bokeh_size = g_work.particles_size_scale;
                    g_work.particles_bokeh_opacity = g_work.particles_opacity;
                    break;
                case 2:
                    g_work.particles_sparks_density = g_work.particles_density;
                    g_work.particles_sparks_size = g_work.particles_size_scale;
                    g_work.particles_sparks_opacity = g_work.particles_opacity;
                    break;
                case 3:
                    g_work.particles_stars_density = g_work.particles_density;
                    g_work.particles_stars_size = g_work.particles_size_scale;
                    g_work.particles_stars_opacity = g_work.particles_opacity;
                    break;
            }
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
                        particles_apply_kind_default(h);
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
    g_cur_tab = sel;
    g_dirty = true;   /* re-sincroniza o preview ja na troca (ex.: sair/entrar na aba Conteudo) */

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

/* le a versao (FILEVERSION) do proprio .scr via a API de version info do
   Windows, pra nao duplicar o numero de versao num 2o lugar alem do
   VERSIONINFO do .rc */
static void about_get_version(wchar_t *out, int cap)
{
    wcsncpy(out, L"?", (size_t)cap - 1);
    out[cap - 1] = 0;

    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) return;

    DWORD dummy;
    DWORD sz = GetFileVersionInfoSizeW(path, &dummy);
    if (sz == 0) return;

    void *buf = malloc(sz);
    if (!buf) return;

    if (GetFileVersionInfoW(path, 0, sz, buf)) {
        VS_FIXEDFILEINFO *ffi = NULL;
        UINT ffiLen = 0;
        if (VerQueryValueW(buf, L"\\", (LPVOID *)&ffi, &ffiLen) && ffi) {
            swprintf(out, cap, L"%u.%u.%u.%u",
                     HIWORD(ffi->dwFileVersionMS), LOWORD(ffi->dwFileVersionMS),
                     HIWORD(ffi->dwFileVersionLS), LOWORD(ffi->dwFileVersionLS));
        }
    }
    free(buf);
}

static INT_PTR CALLBACK about_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
        case WM_INITDIALOG: {
            SetWindowTextW(h, i18n_str(STR_ABOUT_TITLE));
            wchar_t ver[32];
            about_get_version(ver, 32);
            wchar_t line[64];
            swprintf(line, 64, i18n_str(STR_ABOUT_VERSION_FMT), ver);
            SetDlgItemTextW(h, IDC_ABOUT_VERSION, line);
            return TRUE;
        }
        case WM_NOTIFY: {
            NMHDR *nm = (NMHDR *)l;
            if (nm->idFrom == IDC_ABOUT_LINK && (nm->code == NM_CLICK || nm->code == NM_RETURN)) {
                ShellExecuteW(h, L"open", L"https://github.com/alanmm/modern3dtext",
                              NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            return FALSE;
        }
        case WM_COMMAND:
            if (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL) {
                EndDialog(h, IDOK);
                return TRUE;
            }
            return TRUE;
        case WM_CLOSE:
            EndDialog(h, IDOK);
            return TRUE;
    }
    return FALSE;
}

static void show_about_dialog(HWND owner)
{
    DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_ABOUT), owner, about_proc, 0);
}

/* monta e exibe o menu popup do botao "..." (Sobre / Importar / Exportar /
   Idioma>) - as selecoes chegam de volta como WM_COMMAND normal pro
   dialogo principal (comportamento padrao do TrackPopupMenu sem
   TPM_RETURNCMD), tratadas no switch de dlg_proc como qualquer outro
   controle. */
static void show_main_menu(HWND dlg, HWND button)
{
    RECT r;
    GetWindowRect(button, &r);

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_ABOUT, i18n_str(STR_MENU_ABOUT));
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, IDM_MENU_RESTORE, i18n_str(STR_MENU_RESTORE_PRESETS));
    AppendMenuW(menu, MF_STRING, IDM_MENU_BACKUP, i18n_str(STR_MENU_BACKUP_PRESETS));
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);

    HMENU langMenu = CreatePopupMenu();
    AppendMenuW(langMenu, MF_STRING | (g_work.ui_language == 0 ? MF_CHECKED : 0),
                IDM_LANG_AUTO, i18n_str(STR_PERF_LANGUAGE_AUTO));
    AppendMenuW(langMenu, MF_STRING | (g_work.ui_language == 1 ? MF_CHECKED : 0),
                IDM_LANG_PT, i18n_str(STR_PERF_LANGUAGE_PT));
    AppendMenuW(langMenu, MF_STRING | (g_work.ui_language == 2 ? MF_CHECKED : 0),
                IDM_LANG_EN, i18n_str(STR_PERF_LANGUAGE_EN));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)langMenu, i18n_str(STR_MENU_LANGUAGE));

    TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, 0, dlg, NULL);
    DestroyMenu(menu);
}

static wchar_t *g_preset_name_out;
static int      g_preset_name_cap;

static INT_PTR CALLBACK preset_name_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    (void)l;
    switch (m) {
        case WM_INITDIALOG:
            SetWindowTextW(h, i18n_str(STR_PRESET_NAME_TITLE));
            SetDlgItemTextW(h, IDC_PRESET_NAME_LABEL, i18n_str(STR_PRESET_NAME_LABEL));
            SetDlgItemTextW(h, IDCANCEL, i18n_str(STR_BTN_CANCEL));
            /* IDOK fica "OK" fixo, sem traducao - mesma convencao ja usada
               no dialogo principal (apply_language_change nunca retraduz
               IDOK, so IDCANCEL/IDC_APPLY). */
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case IDOK: {
                    wchar_t buf[PRESET_NAME_MAX];
                    GetDlgItemTextW(h, IDC_PRESET_NAME_EDIT, buf, PRESET_NAME_MAX);
                    if (buf[0] == 0 || wcschr(buf, L'\\') || wcschr(buf, L'[') || wcschr(buf, L']')) {
                        MessageBeep(MB_ICONWARNING);
                        return TRUE;   /* nome vazio ou com \\, [ ou ] - nao fecha */
                    }
                    wcsncpy(g_preset_name_out, buf, (size_t)g_preset_name_cap - 1);
                    g_preset_name_out[g_preset_name_cap - 1] = 0;
                    EndDialog(h, IDOK);
                    return TRUE;
                }
                case IDCANCEL:
                    EndDialog(h, IDCANCEL);
                    return TRUE;
            }
            return TRUE;
    }
    return FALSE;
}

/* pede um nome de preset ao usuario; devolve 1 e preenche out[0..outCap)
   se confirmado, 0 se cancelado. Rejeita nome vazio ou com '\\' (quebraria
   o caminho da subchave do registro), ou com '[' / ']' (quebraria o
   cabecalho de secao "[Preset:Nome]" usado no arquivo de backup). */
static int prompt_preset_name(HWND owner, wchar_t *out, int outCap)
{
    g_preset_name_out = out;
    g_preset_name_cap = outCap;
    out[0] = 0;
    return DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_PRESET_NAME),
                            owner, preset_name_proc, 0) == IDOK;
}

/* diálogo de 3 botões pra conflito de nome ao restaurar um backup em
   lote - TaskDialogIndirect (nao MessageBoxW) porque precisamos de um
   terceiro botao com texto proprio ("sobrescrever todos"), nao coberto
   pelos conjuntos padrao do MessageBox. O botao "Pular" usa o ID
   IDCANCEL de proposito, pra ESC/fechar a janela equivaler a pular
   este preset (TDF_ALLOW_DIALOG_CANCELLATION). Devolve 0=pular,
   1=sobrescrever este, 2=sobrescrever este e todos os demais conflitos. */
static int show_preset_conflict_dialog(HWND owner, const wchar_t *name)
{
    enum { TDBTN_OVERWRITE = 1001, TDBTN_OVERWRITE_ALL = 1002 };

    wchar_t content[300];
    swprintf(content, 300, i18n_str(STR_PRESET_RESTORE_CONFLICT), name);

    TASKDIALOG_BUTTON buttons[3] = {
        { TDBTN_OVERWRITE,     i18n_str(STR_PRESET_RESTORE_CONFLICT_OVERWRITE) },
        { IDCANCEL,            i18n_str(STR_PRESET_RESTORE_CONFLICT_SKIP) },
        { TDBTN_OVERWRITE_ALL, i18n_str(STR_PRESET_RESTORE_CONFLICT_OVERWRITE_ALL) },
    };

    TASKDIALOGCONFIG cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.cbSize = sizeof cfg;
    cfg.hwndParent = owner;
    cfg.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
    cfg.pszWindowTitle = i18n_str(STR_PRESET_RESTORE_CONFLICT_TITLE);
    cfg.pszMainIcon = TD_WARNING_ICON;
    cfg.pszContent = content;
    cfg.cButtons = 3;
    cfg.pButtons = buttons;
    cfg.nDefaultButton = TDBTN_OVERWRITE;

    int pressed = IDCANCEL;
    TaskDialogIndirect(&cfg, &pressed, NULL, NULL);

    if (pressed == TDBTN_OVERWRITE_ALL) return 2;
    if (pressed == TDBTN_OVERWRITE) return 1;
    return 0;
}

static int g_preset_sel = -1;   /* -1 = nenhum preset selecionado ainda */

/* repopula o combo: os 4 embutidos primeiro (traduzidos), depois os
   salvos em ordem alfabetica. Preserva a selecao visual em g_preset_sel
   se ainda for valida, senao limpa. */
static void preset_refresh_combo(HWND dlg)
{
    HWND cb = GetDlgItem(dlg, IDC_PRESET_COMBO);
    SendMessageW(cb, CB_RESETCONTENT, 0, 0);

    for (int i = 0; i < BUILTIN_PRESET_COUNT; ++i)
        SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)i18n_str(g_builtin_presets[i].name));

    wchar_t names[64][PRESET_NAME_MAX];
    int n = preset_user_list(names, 64);
    for (int i = 0; i < n; ++i)
        SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)names[i]);

    int total = BUILTIN_PRESET_COUNT + n;
    if (g_preset_sel >= total) g_preset_sel = -1;
    SendMessageW(cb, CB_SETCURSEL, g_preset_sel, 0);
    EnableWindow(GetDlgItem(dlg, IDC_PRESET_DELETE), g_preset_sel >= BUILTIN_PRESET_COUNT);
}

static void preset_apply_i18n(HWND dlg)
{
    SetDlgItemTextW(dlg, IDC_PRESET_LABEL, i18n_str(STR_PRESET_LABEL));
    SetDlgItemTextW(dlg, IDC_PRESET_SAVE, i18n_str(STR_PRESET_SAVE_BTN));
    SetDlgItemTextW(dlg, IDC_PRESET_DELETE, i18n_str(STR_PRESET_DELETE_BTN));
    preset_refresh_combo(dlg);
}

/* destroi e recria os 9 sub-dialogos das abas - reaproveita 100% da
   logica de carregamento ja existente em cada *_proc's WM_INITDIALOG
   (sliders/radios/combos/labels a partir de g_work), sem precisar
   duplicar essa logica numa mensagem nova. Usado depois de aplicar
   um preset, quando varios campos de g_work mudam de uma vez. */
static void reload_all_tabs(HWND dlg)
{
    HWND tabs = GetDlgItem(dlg, IDC_TABS);

    DestroyWindow(g_content);
    DestroyWindow(g_motion);
    DestroyWindow(g_material);
    DestroyWindow(g_geometry);
    DestroyWindow(g_effects);
    DestroyWindow(g_perf);
    DestroyWindow(g_post);
    DestroyWindow(g_bg);
    DestroyWindow(g_particles);

    g_content = CreateDialogW(GetModuleHandleW(NULL),
                              MAKEINTRESOURCEW(IDD_TAB_CONTENT), dlg, content_proc);
    g_motion = CreateDialogW(GetModuleHandleW(NULL),
                             MAKEINTRESOURCEW(IDD_TAB_MOTION), dlg, motion_proc);
    g_material = CreateDialogW(GetModuleHandleW(NULL),
                              MAKEINTRESOURCEW(IDD_TAB_MATERIAL), dlg, material_proc);
    g_geometry = CreateDialogW(GetModuleHandleW(NULL),
                               MAKEINTRESOURCEW(IDD_TAB_GEOMETRY), dlg, geometry_proc);
    g_effects = CreateDialogW(GetModuleHandleW(NULL),
                              MAKEINTRESOURCEW(IDD_TAB_EFFECTS), dlg, effects_proc);
    g_perf = CreateDialogW(GetModuleHandleW(NULL),
                           MAKEINTRESOURCEW(IDD_TAB_PERF), dlg, perf_proc);
    g_post = CreateDialogW(GetModuleHandleW(NULL),
                           MAKEINTRESOURCEW(IDD_TAB_POST), dlg, post_proc);
    g_bg = CreateDialogW(GetModuleHandleW(NULL),
                         MAKEINTRESOURCEW(IDD_TAB_BG), dlg, bg_proc);
    g_particles = CreateDialogW(GetModuleHandleW(NULL),
                                MAKEINTRESOURCEW(IDD_TAB_PARTICLES), dlg, particles_proc);

    place_tab_child(dlg, tabs, g_content);
    place_tab_child(dlg, tabs, g_motion);
    place_tab_child(dlg, tabs, g_material);
    place_tab_child(dlg, tabs, g_geometry);
    place_tab_child(dlg, tabs, g_effects);
    place_tab_child(dlg, tabs, g_perf);
    place_tab_child(dlg, tabs, g_post);
    place_tab_child(dlg, tabs, g_bg);
    place_tab_child(dlg, tabs, g_particles);

    select_tab(g_cur_tab);   /* mantem a aba atual selecionada, marca o preview sujo */
}

static void apply_language_change(void)
{
    i18n_init(g_work.ui_language);

    content_apply_i18n(g_content);
    motion_apply_i18n(g_motion);
    material_apply_i18n(g_material);
    geometry_apply_i18n(g_geometry);
    effects_apply_i18n(g_effects);
    perf_apply_i18n(g_perf);
    post_apply_i18n(g_post);
    bg_apply_i18n(g_bg);
    particles_apply_i18n(g_particles);

    if (g_dlg) {
        SetDlgItemTextW(g_dlg, IDCANCEL, i18n_str(STR_BTN_CANCEL));
        SetDlgItemTextW(g_dlg, IDC_APPLY, i18n_str(STR_BTN_APPLY));

        HWND tabs = GetDlgItem(g_dlg, IDC_TABS);
        TCITEMW ti; memset(&ti, 0, sizeof ti); ti.mask = TCIF_TEXT;
        const StrId tab_ids[9] = {
            STR_TAB_CONTENT, STR_TAB_MOTION, STR_TAB_MATERIAL, STR_TAB_GEOMETRY,
            STR_TAB_EFFECTS, STR_TAB_PERF, STR_TAB_POST, STR_TAB_BG, STR_TAB_PARTICLES
        };
        for (int i = 0; i < 9; ++i) {
            ti.pszText = (wchar_t *)i18n_str(tab_ids[i]);
            TabCtrl_SetItem(tabs, i, &ti);
        }

        preset_apply_i18n(g_dlg);
    }
}

static INT_PTR CALLBACK dlg_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
        case WM_INITDIALOG: {
            g_dlg = h;
            i18n_init(g_work.ui_language);

            HWND tabs = GetDlgItem(h, IDC_TABS);
            TCITEMW ti;
            memset(&ti, 0, sizeof ti);
            ti.mask = TCIF_TEXT;
            const StrId tab_ids[9] = {
                STR_TAB_CONTENT, STR_TAB_MOTION, STR_TAB_MATERIAL, STR_TAB_GEOMETRY,
                STR_TAB_EFFECTS, STR_TAB_PERF, STR_TAB_POST, STR_TAB_BG, STR_TAB_PARTICLES
            };
            for (int i = 0; i < 9; ++i) {
                ti.pszText = (wchar_t *)i18n_str(tab_ids[i]);
                TabCtrl_InsertItem(tabs, i, &ti);
            }
            SetDlgItemTextW(h, IDCANCEL, i18n_str(STR_BTN_CANCEL));
            SetDlgItemTextW(h, IDC_APPLY, i18n_str(STR_BTN_APPLY));
            EnableWindow(GetDlgItem(h, IDC_APPLY), FALSE);
            SetDlgItemTextW(h, IDC_MENU_BUTTON, L"≡");   /* "identical to" - hamburger, ja existe no Segoe UI, sem i18n */
            {
                /* negrito so' no rotulo do menu, pra destacar sem mudar a
                   fonte do resto do dialogo - .rc so' tem 1 fonte por
                   template, entao isso precisa ser feito por codigo */
                static HFONT s_menu_font;
                if (!s_menu_font) {
                    HWND lbl = GetDlgItem(h, IDC_MENU_BUTTON);
                    LOGFONTW lf;
                    GetObjectW((HFONT)SendMessageW(lbl, WM_GETFONT, 0, 0), sizeof lf, &lf);
                    lf.lfWeight = FW_BOLD;
                    s_menu_font = CreateFontIndirectW(&lf);
                }
                if (s_menu_font)
                    SendDlgItemMessageW(h, IDC_MENU_BUTTON, WM_SETFONT, (WPARAM)s_menu_font, TRUE);
            }
            g_preset_sel = -1;
            preset_apply_i18n(h);

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
            g_pv_debug_view = 0;
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
            EnableWindow(GetDlgItem(h, IDC_APPLY), TRUE);
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
                case IDC_APPLY:
                    config_save(&g_work);
                    EnableWindow(GetDlgItem(h, IDC_APPLY), FALSE);
                    return TRUE;
                case IDCANCEL:  preview_teardown(h); EndDialog(h, IDCANCEL); return TRUE;
                case IDC_MENU_BUTTON:
                    show_main_menu(h, GetDlgItem(h, IDC_MENU_BUTTON));
                    break;
                case IDM_ABOUT:
                    show_about_dialog(h);
                    break;
                case IDM_LANG_AUTO:
                case IDM_LANG_PT:
                case IDM_LANG_EN:
                    g_work.ui_language = (LOWORD(w) == IDM_LANG_AUTO) ? 0 :
                                          (LOWORD(w) == IDM_LANG_PT)   ? 1 : 2;
                    apply_language_change();
                    preview_dirty(h);
                    break;
                case IDC_PRESET_COMBO: {
                    if (HIWORD(w) != CBN_SELCHANGE) break;
                    HWND cb = GetDlgItem(h, IDC_PRESET_COMBO);
                    int sel = (int)SendMessageW(cb, CB_GETCURSEL, 0, 0);
                    if (sel < 0 || sel == g_preset_sel) break;

                    wchar_t display[PRESET_NAME_MAX];
                    SendMessageW(cb, CB_GETLBTEXT, sel, (LPARAM)display);

                    wchar_t msg[256];
                    swprintf(msg, 256, i18n_str(STR_PRESET_APPLY_CONFIRM), display);
                    if (MessageBoxW(h, msg, i18n_str(STR_PRESET_APPLY_CONFIRM_TITLE),
                                    MB_YESNO | MB_ICONQUESTION) != IDYES) {
                        SendMessageW(cb, CB_SETCURSEL, g_preset_sel, 0);
                        break;
                    }

                    Config tmp;
                    if (sel < BUILTIN_PRESET_COUNT) {
                        g_builtin_presets[sel].build(&tmp);
                    } else {
                        wchar_t names[64][PRESET_NAME_MAX];
                        int n = preset_user_list(names, 64);
                        int idx = sel - BUILTIN_PRESET_COUNT;
                        if (idx >= n || !preset_user_load(names[idx], &tmp)) break;
                    }
                    preset_scope_copy(&g_work, &tmp);
                    g_preset_sel = sel;
                    EnableWindow(GetDlgItem(h, IDC_PRESET_DELETE), sel >= BUILTIN_PRESET_COUNT);
                    reload_all_tabs(h);
                    EnableWindow(GetDlgItem(h, IDC_APPLY), TRUE);
                    break;
                }
                case IDC_PRESET_SAVE: {
                    wchar_t name[PRESET_NAME_MAX];
                    if (!prompt_preset_name(h, name, PRESET_NAME_MAX)) break;

                    int is_builtin_name = 0;
                    for (int i = 0; i < BUILTIN_PRESET_COUNT; ++i)
                        if (wcscmp(name, i18n_str(g_builtin_presets[i].name)) == 0) is_builtin_name = 1;
                    if (is_builtin_name) {
                        wchar_t msg[256];
                        swprintf(msg, 256, i18n_str(STR_PRESET_CANT_OVERWRITE_BUILTIN), name);
                        MessageBoxW(h, msg, i18n_str(STR_PRESET_CANT_OVERWRITE_BUILTIN_TITLE), MB_OK | MB_ICONWARNING);
                        break;
                    }

                    Config existing;
                    if (preset_user_load(name, &existing)) {
                        wchar_t msg[256];
                        swprintf(msg, 256, i18n_str(STR_PRESET_OVERWRITE_CONFIRM), name);
                        if (MessageBoxW(h, msg, i18n_str(STR_PRESET_OVERWRITE_CONFIRM_TITLE),
                                        MB_YESNO | MB_ICONQUESTION) != IDYES)
                            break;
                    }

                    preset_user_save(name, &g_work);
                    preset_refresh_combo(h);

                    wchar_t names[64][PRESET_NAME_MAX];
                    int n = preset_user_list(names, 64);
                    for (int i = 0; i < n; ++i)
                        if (wcscmp(names[i], name) == 0) { g_preset_sel = BUILTIN_PRESET_COUNT + i; break; }
                    SendMessageW(GetDlgItem(h, IDC_PRESET_COMBO), CB_SETCURSEL, g_preset_sel, 0);
                    EnableWindow(GetDlgItem(h, IDC_PRESET_DELETE), TRUE);
                    break;
                }
                case IDC_PRESET_DELETE: {
                    if (g_preset_sel < BUILTIN_PRESET_COUNT) break;
                    wchar_t names[64][PRESET_NAME_MAX];
                    int n = preset_user_list(names, 64);
                    int idx = g_preset_sel - BUILTIN_PRESET_COUNT;
                    if (idx >= n) break;

                    wchar_t msg[256];
                    swprintf(msg, 256, i18n_str(STR_PRESET_DELETE_CONFIRM), names[idx]);
                    if (MessageBoxW(h, msg, i18n_str(STR_PRESET_DELETE_CONFIRM_TITLE),
                                    MB_YESNO | MB_ICONQUESTION) != IDYES)
                        break;

                    preset_user_delete(names[idx]);
                    g_preset_sel = -1;
                    preset_refresh_combo(h);
                    break;
                }
                case IDM_MENU_RESTORE: {
                    wchar_t file[512] = L"";
                    OPENFILENAMEW ofn;
                    memset(&ofn, 0, sizeof ofn);
                    ofn.lStructSize = sizeof ofn;
                    ofn.hwndOwner = h;
                    wchar_t filter[128]; int fp = 0;
                    filter_append(filter, &fp, 128, i18n_str(STR_FILTER_INI));
                    filter_append(filter, &fp, 128, L"*.ini");
                    filter[fp] = 0;
                    ofn.lpstrFilter = filter;
                    ofn.lpstrFile = file;
                    ofn.nMaxFile = 512;
                    ofn.lpstrDefExt = L"ini";
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                    if (!GetOpenFileNameW(&ofn)) break;

                    static PresetBackupEntry entries[PRESET_BACKUP_MAX];
                    int n = preset_backup_parse_file(file, entries, PRESET_BACKUP_MAX);
                    if (n <= 0) {
                        MessageBoxW(h, i18n_str(STR_PRESET_RESTORE_FAILED),
                                    i18n_str(STR_PRESET_RESTORE_FAILED_TITLE), MB_OK | MB_ICONERROR);
                        break;
                    }

                    int overwrite_all = 0, restored = 0, skipped = 0;
                    for (int i = 0; i < n; ++i) {
                        int is_builtin_name = 0;
                        for (int j = 0; j < BUILTIN_PRESET_COUNT; ++j)
                            if (wcscmp(entries[i].name, i18n_str(g_builtin_presets[j].name)) == 0) is_builtin_name = 1;
                        if (is_builtin_name) { ++skipped; continue; }

                        Config existing;
                        if (preset_user_load(entries[i].name, &existing) && !overwrite_all) {
                            int choice = show_preset_conflict_dialog(h, entries[i].name);
                            if (choice == 0) { ++skipped; continue; }
                            if (choice == 2) overwrite_all = 1;
                        }

                        preset_user_save(entries[i].name, &entries[i].cfg);
                        ++restored;
                    }

                    preset_refresh_combo(h);

                    wchar_t summary[256];
                    swprintf(summary, 256, i18n_str(STR_PRESET_RESTORE_SUMMARY), restored, skipped);
                    MessageBoxW(h, summary, i18n_str(STR_PRESET_RESTORE_SUMMARY_TITLE), MB_OK | MB_ICONINFORMATION);
                    break;
                }
                case IDM_MENU_BACKUP: {
                    wchar_t any[1][PRESET_NAME_MAX];
                    if (preset_user_list(any, 1) == 0) {
                        MessageBoxW(h, i18n_str(STR_PRESET_BACKUP_EMPTY),
                                    i18n_str(STR_PRESET_BACKUP_EMPTY_TITLE), MB_OK | MB_ICONINFORMATION);
                        break;
                    }

                    wchar_t file[512] = L"modern3dtext_presets.ini";
                    OPENFILENAMEW ofn;
                    memset(&ofn, 0, sizeof ofn);
                    ofn.lStructSize = sizeof ofn;
                    ofn.hwndOwner = h;
                    wchar_t filter[128]; int fp = 0;
                    filter_append(filter, &fp, 128, i18n_str(STR_FILTER_INI));
                    filter_append(filter, &fp, 128, L"*.ini");
                    filter[fp] = 0;
                    ofn.lpstrFilter = filter;
                    ofn.lpstrFile = file;
                    ofn.nMaxFile = 512;
                    ofn.lpstrDefExt = L"ini";
                    ofn.Flags = OFN_OVERWRITEPROMPT;
                    if (!GetSaveFileNameW(&ofn)) break;

                    if (!preset_backup_export_file(file))
                        MessageBoxW(h, i18n_str(STR_PRESET_BACKUP_FAILED),
                                    i18n_str(STR_PRESET_BACKUP_FAILED_TITLE), MB_OK | MB_ICONERROR);
                    break;
                }
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
        sizeof icc, ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_LINK_CLASS
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
