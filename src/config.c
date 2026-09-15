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
    strcpy(c->text, "3D Text+");
    wcscpy(c->font_family, L"Segoe UI");
    c->font_bold = 1;
    c->font_italic = 0;
    c->depth = 0.10f;
    c->max_angle_y = 42.0f;
    c->tilt_x = 8.0f;
    c->period = 9.0f;
    c->base_r = 0.72157f; c->base_g = 0.74118f; c->base_b = 0.78039f;
    c->material_mode = 1;
    c->metalness = 0.9f;
    c->roughness = 0.5f;
    c->emissive_r = 1.0f; c->emissive_g = 1.0f; c->emissive_b = 1.0f;
    c->emissive_amount = 0.0f;
    c->refraction = 0.4f;
    c->env_mode = 0;
    c->env_path[0] = 0;
    c->bevel_mode = 1;
    c->bevel_size = 0.005f;
    c->bevel_depth = 0.007f;
    c->bevel_segments = 6;
    c->shell = 0;
    c->wall_thickness = 0.015f;
    c->quality = 2;
    c->bloom_on = 1;
    c->bloom_threshold = 0.64f;
    c->bloom_intensity = 0.65f;
    c->bloom_radius = 0.27f;
    c->fps_cap = 60;
    c->vsync = 1;
    c->msaa = 4;
    c->render_scale = 1.0f;
    c->auto_quality = 1;
    c->streaks_mode = 2;
    c->streaks_intensity = 0.76f;
    c->streaks_length = 0.14f;
    c->chroma_on = 1;
    c->chroma_strength = 0.52f;
    c->vignette_on = 1;
    c->vignette_amount = 0.35f;
    c->fxaa_on = 1;
    c->background_type = 1;
    c->bg_color1_r = 0.06275f; c->bg_color1_g = 0.03922f; c->bg_color1_b = 0.03922f;
    c->bg_color2_r = 0.04706f; c->bg_color2_g = 0.07451f; c->bg_color2_b = 0.13725f;
    c->bg_grad_angle = 101.0f;
    c->bg_image_path[0] = 0;
    c->bg_image_fit = 0;
    c->bg_pan_speed = 0.02f;
    c->bg_neb_color1_r = 0.02745f; c->bg_neb_color1_g = 0.01961f; c->bg_neb_color1_b = 0.07843f;
    c->bg_neb_color2_r = 0.24706f; c->bg_neb_color2_g = 0.09804f; c->bg_neb_color2_b = 0.34902f;
    c->particles_on = 1;
    c->particles_kind = 0;
    c->particles_density = 0.43f;
    c->particles_speed = 0.76f;
    c->particles_size_scale = 0.8f;
    c->particles_opacity = 0.5f;
    c->clock_show_date = 0;
    c->clock_show_seconds = 0;
    c->svg_path[0] = 0;
    c->svg_color_mode = 0;
    c->mesh_path[0] = 0;
    c->mesh_size_scale = 1.0f;
    c->mesh_use_file_materials = 0;
    c->ui_language = 0;
    c->bg_solid_customized = 0;
    c->bg_gradient_customized = 0;
    c->particles_dust_density = 0.43f;   c->particles_dust_size = 0.80f;   c->particles_dust_opacity = 0.50f;
    c->particles_bokeh_density = 0.43f;  c->particles_bokeh_size = 0.81f;  c->particles_bokeh_opacity = 0.42f;
    c->particles_sparks_density = 0.42f; c->particles_sparks_size = 0.72f; c->particles_sparks_opacity = 0.42f;
    c->particles_stars_density = 0.90f;  c->particles_stars_size = 0.70f;  c->particles_stars_opacity = 0.78f;
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
    reg_get_i(k, L"content_mode", (int *)&c->content_mode);
    reg_get_i(k, L"material_mode", &c->material_mode);
    reg_get_w(k, L"env_path", c->env_path, 512);
    int had_env_mode = reg_get_i(k, L"env_mode", &c->env_mode);
    if (!had_env_mode && c->env_path[0])
        c->env_mode = 1;   /* config salvo antes desta fase, com imagem propria -> preserva */
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
    if (reg_get_f(k, L"emissive_r", &f))      c->emissive_r = f;
    if (reg_get_f(k, L"emissive_g", &f))      c->emissive_g = f;
    if (reg_get_f(k, L"emissive_b", &f))      c->emissive_b = f;
    if (reg_get_f(k, L"emissive_amount", &f)) c->emissive_amount = f;
    if (reg_get_f(k, L"refraction", &f))      c->refraction = f;
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
    reg_get_i(k, L"streaks_mode", &c->streaks_mode);
    if (reg_get_f(k, L"streaks_intensity", &f)) c->streaks_intensity = f;
    if (reg_get_f(k, L"streaks_length", &f))    c->streaks_length = f;
    reg_get_i(k, L"chroma_on", &c->chroma_on);
    if (reg_get_f(k, L"chroma_strength", &f)) c->chroma_strength = f;
    reg_get_i(k, L"vignette_on", &c->vignette_on);
    if (reg_get_f(k, L"vignette_amount", &f)) c->vignette_amount = f;
    reg_get_i(k, L"fxaa_on", &c->fxaa_on);

    reg_get_i(k, L"background_type", &c->background_type);
    reg_get_w(k, L"bg_image_path", c->bg_image_path, 512);
    reg_get_i(k, L"bg_image_fit", &c->bg_image_fit);
    if (reg_get_f(k, L"bg_color1_r", &f)) c->bg_color1_r = f;
    if (reg_get_f(k, L"bg_color1_g", &f)) c->bg_color1_g = f;
    if (reg_get_f(k, L"bg_color1_b", &f)) c->bg_color1_b = f;
    if (reg_get_f(k, L"bg_color2_r", &f)) c->bg_color2_r = f;
    if (reg_get_f(k, L"bg_color2_g", &f)) c->bg_color2_g = f;
    if (reg_get_f(k, L"bg_color2_b", &f)) c->bg_color2_b = f;
    if (reg_get_f(k, L"bg_grad_angle", &f)) c->bg_grad_angle = f;
    if (reg_get_f(k, L"bg_pan_speed", &f)) c->bg_pan_speed = f;
    if (reg_get_f(k, L"bg_neb_color1_r", &f)) c->bg_neb_color1_r = f;
    if (reg_get_f(k, L"bg_neb_color1_g", &f)) c->bg_neb_color1_g = f;
    if (reg_get_f(k, L"bg_neb_color1_b", &f)) c->bg_neb_color1_b = f;
    if (reg_get_f(k, L"bg_neb_color2_r", &f)) c->bg_neb_color2_r = f;
    if (reg_get_f(k, L"bg_neb_color2_g", &f)) c->bg_neb_color2_g = f;
    if (reg_get_f(k, L"bg_neb_color2_b", &f)) c->bg_neb_color2_b = f;

    reg_get_i(k, L"particles_on", &c->particles_on);
    reg_get_i(k, L"particles_kind", &c->particles_kind);
    if (reg_get_f(k, L"particles_density", &f))    c->particles_density = f;
    if (reg_get_f(k, L"particles_speed", &f))      c->particles_speed = f;
    if (reg_get_f(k, L"particles_size_scale", &f)) c->particles_size_scale = f;
    if (reg_get_f(k, L"particles_opacity", &f))     c->particles_opacity = f;

    reg_get_i(k, L"clock_show_date", &c->clock_show_date);
    reg_get_i(k, L"clock_show_seconds", &c->clock_show_seconds);

    reg_get_w(k, L"svg_path", c->svg_path, 512);
    reg_get_i(k, L"svg_color_mode", &c->svg_color_mode);

    reg_get_w(k, L"mesh_path", c->mesh_path, 512);
    if (reg_get_f(k, L"mesh_size_scale", &f)) c->mesh_size_scale = f;
    reg_get_i(k, L"mesh_use_file_materials", &c->mesh_use_file_materials);
    reg_get_i(k, L"ui_language", &c->ui_language);
    reg_get_i(k, L"bg_solid_customized", &c->bg_solid_customized);
    reg_get_i(k, L"bg_gradient_customized", &c->bg_gradient_customized);
    if (reg_get_f(k, L"particles_dust_density", &f))   c->particles_dust_density = f;
    if (reg_get_f(k, L"particles_dust_size", &f))       c->particles_dust_size = f;
    if (reg_get_f(k, L"particles_dust_opacity", &f))    c->particles_dust_opacity = f;
    if (reg_get_f(k, L"particles_bokeh_density", &f))   c->particles_bokeh_density = f;
    if (reg_get_f(k, L"particles_bokeh_size", &f))      c->particles_bokeh_size = f;
    if (reg_get_f(k, L"particles_bokeh_opacity", &f))   c->particles_bokeh_opacity = f;
    if (reg_get_f(k, L"particles_sparks_density", &f))  c->particles_sparks_density = f;
    if (reg_get_f(k, L"particles_sparks_size", &f))     c->particles_sparks_size = f;
    if (reg_get_f(k, L"particles_sparks_opacity", &f))  c->particles_sparks_opacity = f;
    if (reg_get_f(k, L"particles_stars_density", &f))   c->particles_stars_density = f;
    if (reg_get_f(k, L"particles_stars_size", &f))      c->particles_stars_size = f;
    if (reg_get_f(k, L"particles_stars_opacity", &f))   c->particles_stars_opacity = f;

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
    if (c->material_mode < 0 || c->material_mode > 2) c->material_mode = 0;
    c->metalness = clampf(c->metalness, 0.0f, 1.0f);
    c->roughness = clampf(c->roughness, 0.0f, 1.0f);
    c->emissive_r = clampf(c->emissive_r, 0.0f, 1.0f);
    c->emissive_g = clampf(c->emissive_g, 0.0f, 1.0f);
    c->emissive_b = clampf(c->emissive_b, 0.0f, 1.0f);
    c->emissive_amount = clampf(c->emissive_amount, 0.0f, 1.0f);
    c->refraction = clampf(c->refraction, 0.0f, 1.0f);
    if (c->env_mode < 0 || c->env_mode > 2) c->env_mode = 0;
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
    if (c->streaks_mode < 0 || c->streaks_mode > 2) c->streaks_mode = 0;
    c->streaks_intensity = clampf(c->streaks_intensity, 0.0f, 2.0f);
    c->streaks_length = clampf(c->streaks_length, 0.0f, 1.0f);
    c->chroma_on = c->chroma_on ? 1 : 0;
    c->chroma_strength = clampf(c->chroma_strength, 0.0f, 1.0f);
    c->vignette_on = c->vignette_on ? 1 : 0;
    c->vignette_amount = clampf(c->vignette_amount, 0.0f, 1.0f);
    c->fxaa_on = c->fxaa_on ? 1 : 0;
    if (c->background_type < 0 || c->background_type > 3) c->background_type = 0;
    c->bg_color1_r = clampf(c->bg_color1_r, 0.0f, 1.0f);
    c->bg_color1_g = clampf(c->bg_color1_g, 0.0f, 1.0f);
    c->bg_color1_b = clampf(c->bg_color1_b, 0.0f, 1.0f);
    c->bg_color2_r = clampf(c->bg_color2_r, 0.0f, 1.0f);
    c->bg_color2_g = clampf(c->bg_color2_g, 0.0f, 1.0f);
    c->bg_color2_b = clampf(c->bg_color2_b, 0.0f, 1.0f);
    c->bg_grad_angle = clampf(c->bg_grad_angle, 0.0f, 360.0f);
    if (c->bg_image_fit < 0 || c->bg_image_fit > 2) c->bg_image_fit = 0;
    c->bg_pan_speed = clampf(c->bg_pan_speed, 0.0f, 1.0f);
    c->bg_neb_color1_r = clampf(c->bg_neb_color1_r, 0.0f, 1.0f);
    c->bg_neb_color1_g = clampf(c->bg_neb_color1_g, 0.0f, 1.0f);
    c->bg_neb_color1_b = clampf(c->bg_neb_color1_b, 0.0f, 1.0f);
    c->bg_neb_color2_r = clampf(c->bg_neb_color2_r, 0.0f, 1.0f);
    c->bg_neb_color2_g = clampf(c->bg_neb_color2_g, 0.0f, 1.0f);
    c->bg_neb_color2_b = clampf(c->bg_neb_color2_b, 0.0f, 1.0f);
    c->particles_on = c->particles_on ? 1 : 0;
    if (c->particles_kind < 0 || c->particles_kind > 3) c->particles_kind = 0;
    c->particles_density    = clampf(c->particles_density, 0.0f, 1.0f);
    c->particles_speed      = clampf(c->particles_speed, 0.0f, 2.0f);
    c->particles_size_scale = clampf(c->particles_size_scale, 0.0f, 2.0f);
    c->particles_opacity    = clampf(c->particles_opacity, 0.0f, 2.0f);
    if (c->content_mode < 0 || c->content_mode > 3) c->content_mode = CONTENT_TEXT;
    c->clock_show_date = c->clock_show_date ? 1 : 0;
    c->clock_show_seconds = c->clock_show_seconds ? 1 : 0;
    c->svg_color_mode = c->svg_color_mode ? 1 : 0;
    c->mesh_size_scale = clampf(c->mesh_size_scale, 0.0f, 2.0f);
    c->mesh_use_file_materials = c->mesh_use_file_materials ? 1 : 0;
    if (c->ui_language < 0 || c->ui_language > 2) c->ui_language = 0;
    c->bg_solid_customized = c->bg_solid_customized ? 1 : 0;
    c->bg_gradient_customized = c->bg_gradient_customized ? 1 : 0;
    c->particles_dust_density   = clampf(c->particles_dust_density, 0.0f, 1.0f);
    c->particles_dust_size      = clampf(c->particles_dust_size, 0.0f, 2.0f);
    c->particles_dust_opacity   = clampf(c->particles_dust_opacity, 0.0f, 2.0f);
    c->particles_bokeh_density  = clampf(c->particles_bokeh_density, 0.0f, 1.0f);
    c->particles_bokeh_size     = clampf(c->particles_bokeh_size, 0.0f, 2.0f);
    c->particles_bokeh_opacity  = clampf(c->particles_bokeh_opacity, 0.0f, 2.0f);
    c->particles_sparks_density = clampf(c->particles_sparks_density, 0.0f, 1.0f);
    c->particles_sparks_size    = clampf(c->particles_sparks_size, 0.0f, 2.0f);
    c->particles_sparks_opacity = clampf(c->particles_sparks_opacity, 0.0f, 2.0f);
    c->particles_stars_density  = clampf(c->particles_stars_density, 0.0f, 1.0f);
    c->particles_stars_size     = clampf(c->particles_stars_size, 0.0f, 2.0f);
    c->particles_stars_opacity  = clampf(c->particles_stars_opacity, 0.0f, 2.0f);
    if (c->text[0] == 0) strcpy(c->text, "3D Text+");
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
    set_f(k, L"emissive_r", c->emissive_r);
    set_f(k, L"emissive_g", c->emissive_g);
    set_f(k, L"emissive_b", c->emissive_b);
    set_f(k, L"emissive_amount", c->emissive_amount);
    set_f(k, L"refraction", c->refraction);
    set_f(k, L"env_mode", (float)c->env_mode);
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
    set_f(k, L"streaks_mode", (float)c->streaks_mode);
    set_f(k, L"streaks_intensity", c->streaks_intensity);
    set_f(k, L"streaks_length", c->streaks_length);
    set_f(k, L"chroma_on", (float)c->chroma_on);
    set_f(k, L"chroma_strength", c->chroma_strength);
    set_f(k, L"vignette_on", (float)c->vignette_on);
    set_f(k, L"vignette_amount", c->vignette_amount);
    set_f(k, L"fxaa_on", (float)c->fxaa_on);

    set_f(k, L"background_type", (float)c->background_type);
    set_f(k, L"bg_color1_r", c->bg_color1_r);
    set_f(k, L"bg_color1_g", c->bg_color1_g);
    set_f(k, L"bg_color1_b", c->bg_color1_b);
    set_f(k, L"bg_color2_r", c->bg_color2_r);
    set_f(k, L"bg_color2_g", c->bg_color2_g);
    set_f(k, L"bg_color2_b", c->bg_color2_b);
    set_f(k, L"bg_grad_angle", c->bg_grad_angle);
    set_w(k, L"bg_image_path", c->bg_image_path);
    set_f(k, L"bg_image_fit", (float)c->bg_image_fit);
    set_f(k, L"bg_pan_speed", c->bg_pan_speed);
    set_f(k, L"bg_neb_color1_r", c->bg_neb_color1_r);
    set_f(k, L"bg_neb_color1_g", c->bg_neb_color1_g);
    set_f(k, L"bg_neb_color1_b", c->bg_neb_color1_b);
    set_f(k, L"bg_neb_color2_r", c->bg_neb_color2_r);
    set_f(k, L"bg_neb_color2_g", c->bg_neb_color2_g);
    set_f(k, L"bg_neb_color2_b", c->bg_neb_color2_b);

    set_f(k, L"particles_on", (float)c->particles_on);
    set_f(k, L"particles_kind", (float)c->particles_kind);
    set_f(k, L"particles_density", c->particles_density);
    set_f(k, L"particles_speed", c->particles_speed);
    set_f(k, L"particles_size_scale", c->particles_size_scale);
    set_f(k, L"particles_opacity", c->particles_opacity);

    set_f(k, L"clock_show_date", (float)c->clock_show_date);
    set_f(k, L"clock_show_seconds", (float)c->clock_show_seconds);

    set_w(k, L"svg_path", c->svg_path);
    set_f(k, L"svg_color_mode", (float)c->svg_color_mode);

    set_w(k, L"mesh_path", c->mesh_path);
    set_f(k, L"mesh_size_scale", c->mesh_size_scale);
    set_f(k, L"mesh_use_file_materials", (float)c->mesh_use_file_materials);
    set_f(k, L"ui_language", (float)c->ui_language);
    set_f(k, L"bg_solid_customized", (float)c->bg_solid_customized);
    set_f(k, L"bg_gradient_customized", (float)c->bg_gradient_customized);
    set_f(k, L"particles_dust_density", c->particles_dust_density);
    set_f(k, L"particles_dust_size", c->particles_dust_size);
    set_f(k, L"particles_dust_opacity", c->particles_dust_opacity);
    set_f(k, L"particles_bokeh_density", c->particles_bokeh_density);
    set_f(k, L"particles_bokeh_size", c->particles_bokeh_size);
    set_f(k, L"particles_bokeh_opacity", c->particles_bokeh_opacity);
    set_f(k, L"particles_sparks_density", c->particles_sparks_density);
    set_f(k, L"particles_sparks_size", c->particles_sparks_size);
    set_f(k, L"particles_sparks_opacity", c->particles_sparks_opacity);
    set_f(k, L"particles_stars_density", c->particles_stars_density);
    set_f(k, L"particles_stars_size", c->particles_stars_size);
    set_f(k, L"particles_stars_opacity", c->particles_stars_opacity);

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
