#include "config.h"
#include "util/log.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define KEY_MAIN    L"Software\\Modern3DText"
#define CFG_VERSION 2   /* v2: + campos perf (fps_cap/vsync/msaa/render_scale/auto_quality);
                           v1 migra via defaults, sem bloco dedicado */

void config_defaults(Config *c)
{
    memset(c, 0, sizeof *c);
    c->version = CFG_VERSION;
    c->content_mode = CONTENT_TEXT;
    strcpy(c->text, "Modern 3D Text");
    wcscpy(c->font_family, L"Segoe UI");
    c->font_bold = 1;
    c->font_italic = 0;
    c->depth = 0.30f;
    c->max_angle_y = 42.0f;
    c->tilt_x = 8.0f;
    c->period = 9.0f;
    c->base_r = 0.72f; c->base_g = 0.74f; c->base_b = 0.78f;
    c->material_mode = 0;
    c->metalness = 0.9f;
    c->roughness = 0.25f;
    c->env_path[0] = 0;
    c->bevel_mode = 0;
    c->bevel_size = 0.035f;
    c->bevel_depth = 0.035f;
    c->bevel_segments = 4;
    c->shell = 0;
    c->wall_thickness = 0.06f;
    c->quality = 1;
    c->bloom_on = 1;
    c->bloom_threshold = 1.05f;
    c->bloom_intensity = 0.6f;
    c->bloom_radius = 0.55f;
    c->fps_cap = 60;
    c->vsync = 1;
    c->msaa = 4;
    c->render_scale = 1.0f;
    c->auto_quality = 1;
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* snap para o valor valido mais proximo; empate -> o maior (opts em ordem crescente) */
static int snap_to(int v, const int *opts, int n)
{
    int best = opts[0], bd = 1 << 30;
    for (int i = 0; i < n; ++i) {
        int d = v > opts[i] ? v - opts[i] : opts[i] - v;
        if (d <= bd) { bd = d; best = opts[i]; }
    }
    return best;
}

static int reg_get_w(HKEY k, const wchar_t *name, wchar_t *out, int cch)
{
    DWORD type = 0, cb = (DWORD)((size_t)cch * sizeof(wchar_t));
    if (RegQueryValueExW(k, name, NULL, &type, (BYTE *)out, &cb) != ERROR_SUCCESS || type != REG_SZ)
        return 0;
    out[cch - 1] = 0;
    return 1;
}

static int reg_get_f(HKEY k, const wchar_t *name, float *out)
{
    wchar_t buf[64];
    if (!reg_get_w(k, name, buf, 64)) return 0;
    for (wchar_t *q = buf; *q; ++q) if (*q == L',') *q = L'.';   /* tolera locale pt-BR */
    wchar_t *end = NULL;
    double v = wcstod(buf, &end);
    if (end == buf) return 0;
    *out = (float)v;
    return 1;
}

static int reg_get_i(HKEY k, const wchar_t *name, int *out)
{
    float f;
    if (reg_get_f(k, name, &f)) { *out = (int)(f + 0.5f); return 1; }

    /* tolera REG_DWORD (ex.: valor criado a mao no regedit) */
    DWORD type = 0, val = 0, cb = sizeof val;
    if (RegQueryValueExW(k, name, NULL, &type, (BYTE *)&val, &cb) == ERROR_SUCCESS
        && type == REG_DWORD) {
        *out = (int)val;
        return 1;
    }
    return 0;
}

static void set_w(HKEY k, const wchar_t *name, const wchar_t *val)
{
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)val,
                   (DWORD)((wcslen(val) + 1) * sizeof(wchar_t)));
}

static void set_f(HKEY k, const wchar_t *name, float v)
{
    wchar_t buf[32];
    swprintf(buf, 32, L"%.4g", (double)v);
    set_w(k, name, buf);
}

void config_load_from(Config *c, const wchar_t *subkey)
{
    config_defaults(c);

    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subkey, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return;   /* primeira execucao: tudo default */

    wchar_t wtext[512];
    if (reg_get_w(k, L"text", wtext, 512))
        WideCharToMultiByte(CP_UTF8, 0, wtext, -1, c->text, (int)sizeof c->text, NULL, NULL);
    reg_get_w(k, L"font_family", c->font_family, 64);
    reg_get_i(k, L"font_bold", &c->font_bold);
    reg_get_i(k, L"font_italic", &c->font_italic);
    reg_get_i(k, L"version", &c->version);
    reg_get_i(k, L"material_mode", &c->material_mode);
    reg_get_w(k, L"env_path", c->env_path, 512);
    reg_get_i(k, L"bevel_mode", &c->bevel_mode);
    reg_get_i(k, L"bevel_segments", &c->bevel_segments);
    reg_get_i(k, L"shell", &c->shell);
    reg_get_i(k, L"quality", &c->quality);
    reg_get_i(k, L"bloom_on", &c->bloom_on);

    float f;
    if (reg_get_f(k, L"depth", &f))       c->depth = f;
    if (reg_get_f(k, L"max_angle_y", &f)) c->max_angle_y = f;
    if (reg_get_f(k, L"tilt_x", &f))      c->tilt_x = f;
    if (reg_get_f(k, L"period", &f))      c->period = f;
    if (reg_get_f(k, L"metalness", &f))   c->metalness = f;
    if (reg_get_f(k, L"roughness", &f))   c->roughness = f;
    if (reg_get_f(k, L"bevel_size", &f))     c->bevel_size = f;
    if (reg_get_f(k, L"bevel_depth", &f))    c->bevel_depth = f;
    if (reg_get_f(k, L"wall_thickness", &f)) c->wall_thickness = f;
    if (reg_get_f(k, L"bloom_threshold", &f)) c->bloom_threshold = f;
    if (reg_get_f(k, L"bloom_intensity", &f)) c->bloom_intensity = f;
    if (reg_get_f(k, L"bloom_radius", &f))    c->bloom_radius = f;

    reg_get_i(k, L"fps_cap", &c->fps_cap);
    reg_get_i(k, L"vsync", &c->vsync);
    reg_get_i(k, L"msaa", &c->msaa);
    reg_get_i(k, L"auto_quality", &c->auto_quality);
    if (reg_get_f(k, L"render_scale", &f)) c->render_scale = f;

    wchar_t col[16];
    if (reg_get_w(k, L"base_color", col, 16) && col[0] == L'#' && wcslen(col) >= 7) {
        unsigned rgb = (unsigned)wcstoul(col + 1, NULL, 16);
        c->base_r = ((rgb >> 16) & 0xFF) / 255.0f;
        c->base_g = ((rgb >> 8) & 0xFF) / 255.0f;
        c->base_b = (rgb & 0xFF) / 255.0f;
    }
    RegCloseKey(k);

    /* saneamento */
    c->font_bold = c->font_bold ? 1 : 0;
    c->font_italic = c->font_italic ? 1 : 0;
    c->depth = clampf(c->depth, 0.02f, 2.0f);
    c->max_angle_y = clampf(c->max_angle_y, 5.0f, 170.0f);
    c->tilt_x = clampf(c->tilt_x, 0.0f, 30.0f);
    c->period = clampf(c->period, 2.0f, 30.0f);
    c->base_r = clampf(c->base_r, 0.0f, 1.0f);
    c->base_g = clampf(c->base_g, 0.0f, 1.0f);
    c->base_b = clampf(c->base_b, 0.0f, 1.0f);
    if (c->material_mode < 0 || c->material_mode > 3) c->material_mode = 0;
    c->metalness = clampf(c->metalness, 0.0f, 1.0f);
    c->roughness = clampf(c->roughness, 0.0f, 1.0f);
    if (c->bevel_mode < 0 || c->bevel_mode > 2) c->bevel_mode = 0;
    if (c->bevel_segments < 2) c->bevel_segments = 2;
    if (c->bevel_segments > 8) c->bevel_segments = 8;
    if (c->quality < 0 || c->quality > 2) c->quality = 1;
    c->shell = c->shell ? 1 : 0;
    c->bevel_size = clampf(c->bevel_size, 0.0f, 0.2f);
    c->bevel_depth = clampf(c->bevel_depth, 0.0f, 0.2f);
    c->wall_thickness = clampf(c->wall_thickness, 0.01f, 0.2f);
    c->bloom_on = c->bloom_on ? 1 : 0;
    c->bloom_threshold = clampf(c->bloom_threshold, 0.2f, 3.0f);
    c->bloom_intensity = clampf(c->bloom_intensity, 0.0f, 2.0f);
    c->bloom_radius = clampf(c->bloom_radius, 0.0f, 1.0f);
    {
        static const int FPS_OPTS[4]  = { 0, 30, 60, 120 };
        static const int MSAA_OPTS[4] = { 0, 2, 4, 8 };
        c->fps_cap = snap_to(c->fps_cap, FPS_OPTS, 4);
        c->msaa    = snap_to(c->msaa, MSAA_OPTS, 4);
    }
    c->vsync = c->vsync ? 1 : 0;
    c->auto_quality = c->auto_quality ? 1 : 0;
    c->render_scale = clampf(c->render_scale, 0.5f, 1.0f);
    if (c->text[0] == 0) strcpy(c->text, "Modern 3D Text");
    if (c->font_family[0] == 0) wcscpy(c->font_family, L"Segoe UI");
}

void config_save_to(const Config *c, const wchar_t *subkey)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, subkey, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL)
        != ERROR_SUCCESS) {
        log_errorf("config: nao criou %ls", subkey);
        return;
    }

    wchar_t wtext[512];
    MultiByteToWideChar(CP_UTF8, 0, c->text, -1, wtext, 512);
    wtext[511] = 0;
    set_w(k, L"text", wtext);
    set_w(k, L"font_family", c->font_family);
    set_f(k, L"font_bold", (float)c->font_bold);
    set_f(k, L"font_italic", (float)c->font_italic);
    set_f(k, L"version", (float)CFG_VERSION);
    set_f(k, L"content_mode", (float)c->content_mode);
    set_f(k, L"depth", c->depth);
    set_f(k, L"max_angle_y", c->max_angle_y);
    set_f(k, L"tilt_x", c->tilt_x);
    set_f(k, L"period", c->period);
    set_f(k, L"material_mode", (float)c->material_mode);
    set_f(k, L"metalness", c->metalness);
    set_f(k, L"roughness", c->roughness);
    set_w(k, L"env_path", c->env_path);
    set_f(k, L"bevel_mode", (float)c->bevel_mode);
    set_f(k, L"bevel_size", c->bevel_size);
    set_f(k, L"bevel_depth", c->bevel_depth);
    set_f(k, L"bevel_segments", (float)c->bevel_segments);
    set_f(k, L"shell", (float)c->shell);
    set_f(k, L"wall_thickness", c->wall_thickness);
    set_f(k, L"quality", (float)c->quality);
    set_f(k, L"bloom_on", (float)c->bloom_on);
    set_f(k, L"bloom_threshold", c->bloom_threshold);
    set_f(k, L"bloom_intensity", c->bloom_intensity);
    set_f(k, L"bloom_radius", c->bloom_radius);
    set_f(k, L"fps_cap", (float)c->fps_cap);
    set_f(k, L"vsync", (float)c->vsync);
    set_f(k, L"msaa", (float)c->msaa);
    set_f(k, L"render_scale", c->render_scale);
    set_f(k, L"auto_quality", (float)c->auto_quality);

    wchar_t col[16];
    unsigned r = (unsigned)(c->base_r * 255.0f + 0.5f);
    unsigned g = (unsigned)(c->base_g * 255.0f + 0.5f);
    unsigned b = (unsigned)(c->base_b * 255.0f + 0.5f);
    swprintf(col, 16, L"#%02X%02X%02X", r & 0xFF, g & 0xFF, b & 0xFF);
    set_w(k, L"base_color", col);

    RegCloseKey(k);
}

void config_load(Config *c) { config_load_from(c, KEY_MAIN); }
void config_save(const Config *c) { config_save_to(c, KEY_MAIN); }
