#include "test.h"
#include "config.h"

#include <windows.h>
#include <string.h>
#include <math.h>

#define TESTKEY L"Software\\Modern3DText_test"

static int nearf(float a, float b) { return fabsf(a - b) < 1e-3f; }

void run_config_tests(void)
{
    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);

    /* defaults */
    Config d;
    config_defaults(&d);
    EXPECT(strcmp(d.text, "Modern 3D Text") == 0);
    EXPECT(wcscmp(d.font_family, L"Segoe UI") == 0);
    EXPECT(nearf(d.depth, 0.30f));
    EXPECT(d.version == 2);
    EXPECT(d.background_type == 0);
    EXPECT(nearf(d.bg_color1_r, 0.02f) && nearf(d.bg_color1_g, 0.03f) && nearf(d.bg_color1_b, 0.05f));
    EXPECT(d.particles_on == 0);
    EXPECT(nearf(d.particles_density, 0.5f));
    EXPECT(nearf(d.particles_opacity, 1.0f));
    EXPECT(d.content_mode == CONTENT_TEXT);
    EXPECT(d.clock_show_date == 0);
    EXPECT(d.clock_show_seconds == 0);
    EXPECT(d.svg_path[0] == 0);
    EXPECT(d.svg_color_mode == 0);
    EXPECT(d.mesh_path[0] == 0);
    EXPECT(nearf(d.mesh_size_scale, 1.0f));

    /* round-trip */
    Config a;
    config_defaults(&a);
    strcpy(a.text, "Ola Mundo");
    wcscpy(a.font_family, L"Arial");
    a.font_bold = 0;
    a.font_italic = 1;
    a.depth = 0.55f;
    a.max_angle_y = 30.0f;
    a.tilt_x = 12.0f;
    a.period = 6.5f;
    a.base_r = 0.10f; a.base_g = 0.20f; a.base_b = 0.90f;
    a.material_mode = 2;
    a.metalness = 0.4f;
    a.roughness = 0.7f;
    wcscpy(a.env_path, L"C:\\img\\studio.jpg");
    a.bevel_mode = 1;
    a.bevel_size = 0.08f;
    a.bevel_depth = 0.06f;
    a.bevel_segments = 6;
    a.shell = 1;
    a.wall_thickness = 0.09f;
    a.quality = 2;
    a.bloom_on = 0;
    a.bloom_threshold = 1.8f;
    a.bloom_intensity = 1.2f;
    a.bloom_radius = 0.3f;
    a.fps_cap = 30;
    a.vsync = 0;
    a.msaa = 8;
    a.render_scale = 0.75f;
    a.auto_quality = 0;
    a.streaks_mode = 2;
    a.streaks_intensity = 1.4f;
    a.streaks_length = 0.8f;
    a.chroma_on = 1;
    a.chroma_strength = 0.65f;
    a.vignette_on = 1;
    a.vignette_amount = 0.5f;
    a.fxaa_on = 0;
    a.background_type = 3;
    a.bg_color1_r = 0.11f; a.bg_color1_g = 0.22f; a.bg_color1_b = 0.33f;
    a.bg_color2_r = 0.44f; a.bg_color2_g = 0.55f; a.bg_color2_b = 0.66f;
    a.bg_grad_angle = 135.0f;
    wcscpy(a.bg_image_path, L"C:\\img\\fundo.jpg");
    a.bg_image_fit = 2;
    a.bg_pan_speed = 0.35f;
    a.bg_neb_color1_r = 0.05f; a.bg_neb_color1_g = 0.05f; a.bg_neb_color1_b = 0.20f;
    a.bg_neb_color2_r = 0.80f; a.bg_neb_color2_g = 0.30f; a.bg_neb_color2_b = 0.10f;
    a.particles_on = 1;
    a.particles_kind = 2;
    a.particles_density = 0.75f;
    a.particles_speed = 1.6f;
    a.particles_size_scale = 0.4f;
    a.particles_opacity = 0.6f;
    a.content_mode = CONTENT_CLOCK;
    a.clock_show_date = 1;
    a.clock_show_seconds = 1;
    config_save_to(&a, TESTKEY);

    Config b;
    config_load_from(&b, TESTKEY);
    EXPECT(strcmp(b.text, "Ola Mundo") == 0);
    EXPECT(wcscmp(b.font_family, L"Arial") == 0);
    EXPECT(b.font_bold == 0 && b.font_italic == 1);
    EXPECT(nearf(b.depth, 0.55f));
    EXPECT(nearf(b.max_angle_y, 30.0f));
    EXPECT(nearf(b.period, 6.5f));
    EXPECT(fabsf(b.base_b - 0.90f) < 0.01f);   /* cor tem erro de quantizacao 8-bit */
    EXPECT(fabsf(b.base_r - 0.10f) < 0.01f);
    EXPECT(b.material_mode == 2);
    EXPECT(nearf(b.metalness, 0.4f));
    EXPECT(nearf(b.roughness, 0.7f));
    EXPECT(wcscmp(b.env_path, L"C:\\img\\studio.jpg") == 0);
    EXPECT(b.bevel_mode == 1);
    EXPECT(nearf(b.bevel_size, 0.08f));
    EXPECT(nearf(b.bevel_depth, 0.06f));
    EXPECT(b.bevel_segments == 6);
    EXPECT(b.shell == 1);
    EXPECT(nearf(b.wall_thickness, 0.09f));
    EXPECT(b.quality == 2);
    EXPECT(b.bloom_on == 0);
    EXPECT(nearf(b.bloom_threshold, 1.8f));
    EXPECT(nearf(b.bloom_intensity, 1.2f));
    EXPECT(nearf(b.bloom_radius, 0.3f));
    EXPECT(b.fps_cap == 30);
    EXPECT(b.vsync == 0);
    EXPECT(b.msaa == 8);
    EXPECT(nearf(b.render_scale, 0.75f));
    EXPECT(b.auto_quality == 0);
    EXPECT(b.streaks_mode == 2);
    EXPECT(nearf(b.streaks_intensity, 1.4f));
    EXPECT(nearf(b.streaks_length, 0.8f));
    EXPECT(b.chroma_on == 1);
    EXPECT(nearf(b.chroma_strength, 0.65f));
    EXPECT(b.vignette_on == 1);
    EXPECT(nearf(b.vignette_amount, 0.5f));
    EXPECT(b.fxaa_on == 0);
    EXPECT(b.version == 2);
    EXPECT(b.background_type == 3);
    EXPECT(nearf(b.bg_color1_r, 0.11f) && nearf(b.bg_color1_g, 0.22f) && nearf(b.bg_color1_b, 0.33f));
    EXPECT(nearf(b.bg_color2_r, 0.44f) && nearf(b.bg_color2_g, 0.55f) && nearf(b.bg_color2_b, 0.66f));
    EXPECT(nearf(b.bg_grad_angle, 135.0f));
    EXPECT(wcscmp(b.bg_image_path, L"C:\\img\\fundo.jpg") == 0);
    EXPECT(b.bg_image_fit == 2);
    EXPECT(nearf(b.bg_pan_speed, 0.35f));
    EXPECT(nearf(b.bg_neb_color1_b, 0.20f));
    EXPECT(nearf(b.bg_neb_color2_r, 0.80f));
    EXPECT(b.particles_on == 1);
    EXPECT(b.particles_kind == 2);
    EXPECT(nearf(b.particles_density, 0.75f));
    EXPECT(nearf(b.particles_speed, 1.6f));
    EXPECT(nearf(b.particles_size_scale, 0.4f));
    EXPECT(nearf(b.particles_opacity, 0.6f));
    EXPECT(b.content_mode == CONTENT_CLOCK);
    EXPECT(b.clock_show_date == 1);
    EXPECT(b.clock_show_seconds == 1);

    /* round-trip dedicado ao SVG - nao reusa o par a/b acima, que ja
       fixa content_mode em CONTENT_CLOCK para provar o relogio */
    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);
    Config sa, sb;
    config_defaults(&sa);
    sa.content_mode = CONTENT_SVG;
    wcscpy(sa.svg_path, L"C:\\icones\\logo.svg");
    sa.svg_color_mode = 1;
    config_save_to(&sa, TESTKEY);

    config_load_from(&sb, TESTKEY);
    EXPECT(sb.content_mode == CONTENT_SVG);
    EXPECT(wcscmp(sb.svg_path, L"C:\\icones\\logo.svg") == 0);
    EXPECT(sb.svg_color_mode == 1);

    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);

    /* round-trip dedicado a malha importada - mesmo motivo do SVG acima:
       nao reusa pares que ja fixam content_mode noutro valor */
    Config ma, mb;
    config_defaults(&ma);
    ma.content_mode = CONTENT_MESH;
    wcscpy(ma.mesh_path, L"C:\\modelos\\objeto.obj");
    ma.mesh_size_scale = 1.5f;
    config_save_to(&ma, TESTKEY);

    config_load_from(&mb, TESTKEY);
    EXPECT(mb.content_mode == CONTENT_MESH);
    EXPECT(wcscmp(mb.mesh_path, L"C:\\modelos\\objeto.obj") == 0);
    EXPECT(nearf(mb.mesh_size_scale, 1.5f));

    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);

    /* valor ausente -> default; fora de faixa -> clamp; lixo -> default */
    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);
    HKEY k;
    RegCreateKeyExW(HKEY_CURRENT_USER, TESTKEY, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL);
    RegSetValueExW(k, L"depth", 0, REG_SZ, (const BYTE *)L"999", 4 * sizeof(wchar_t));
    RegSetValueExW(k, L"period", 0, REG_SZ, (const BYTE *)L"lixo", 5 * sizeof(wchar_t));
    RegSetValueExW(k, L"material_mode", 0, REG_SZ, (const BYTE *)L"7", 2 * sizeof(wchar_t));
    RegSetValueExW(k, L"metalness", 0, REG_SZ, (const BYTE *)L"5", 2 * sizeof(wchar_t));
    RegSetValueExW(k, L"bevel_mode", 0, REG_SZ, (const BYTE *)L"9", 2 * sizeof(wchar_t));
    RegSetValueExW(k, L"bevel_segments", 0, REG_SZ, (const BYTE *)L"1", 2 * sizeof(wchar_t));
    RegSetValueExW(k, L"quality", 0, REG_SZ, (const BYTE *)L"5", 2 * sizeof(wchar_t));
    RegSetValueExW(k, L"bevel_size", 0, REG_SZ, (const BYTE *)L"9", 2 * sizeof(wchar_t));
    RegSetValueExW(k, L"bloom_intensity", 0, REG_SZ, (const BYTE *)L"-1", 3 * sizeof(wchar_t));
    RegSetValueExW(k, L"bloom_radius", 0, REG_SZ, (const BYTE *)L"5", 2 * sizeof(wchar_t));
    RegCloseKey(k);

    Config c;
    config_load_from(&c, TESTKEY);
    EXPECT(c.depth >= 0.02f && c.depth <= 2.0f);
    EXPECT(nearf(c.period, 9.0f));
    EXPECT(strcmp(c.text, "Modern 3D Text") == 0);
    EXPECT(c.material_mode == 0);              /* 7 fora de 0..3 -> 0 */
    EXPECT(c.metalness >= 0.0f && c.metalness <= 1.0f);   /* 5 -> clamp */
    EXPECT(c.bevel_mode == 0);                 /* 9 -> 0 */
    EXPECT(c.bevel_segments == 2);             /* 1 -> 2 */
    EXPECT(c.quality == 1);                    /* 5 -> default 1 */
    EXPECT(c.bevel_size <= 0.2f);              /* 9 -> clamp */
    EXPECT(c.bloom_intensity >= 0.0f);         /* -1 -> clamp */
    EXPECT(c.bloom_radius <= 1.0f);            /* 5 -> clamp */

    /* clamp perf: valores crus invalidos no registro */
    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);
    {
        HKEY kp;
        RegCreateKeyExW(HKEY_CURRENT_USER, TESTKEY, 0, NULL, 0, KEY_WRITE, NULL, &kp, NULL);
        struct { const wchar_t *n, *v; } kv[] = {
            { L"fps_cap", L"999" }, { L"msaa", L"7" }, { L"render_scale", L"3.0" },
            { L"vsync", L"5" }, { L"auto_quality", L"0" },
            { L"streaks_mode", L"9" }, { L"streaks_intensity", L"-1" },
            { L"streaks_length", L"5" },
            { L"chroma_on", L"7" }, { L"chroma_strength", L"9" },
            { L"vignette_amount", L"-3" }, { L"fxaa_on", L"5" },
            { L"background_type", L"9" }, { L"bg_grad_angle", L"999" },
            { L"bg_image_fit", L"9" }, { L"bg_pan_speed", L"-1" },
            { L"particles_kind", L"9" }, { L"particles_density", L"-1" },
            { L"particles_speed", L"9" }, { L"particles_size_scale", L"-1" },
            { L"particles_opacity", L"9" },
            { L"content_mode", L"9" }, { L"clock_show_date", L"5" },
            { L"clock_show_seconds", L"5" },
            { L"svg_color_mode", L"9" },
            { L"mesh_size_scale", L"9" },
        };
        for (int i = 0; i < 26; ++i)
            RegSetValueExW(kp, kv[i].n, 0, REG_SZ, (const BYTE *)kv[i].v,
                           (DWORD)((wcslen(kv[i].v) + 1) * sizeof(wchar_t)));
        RegCloseKey(kp);
    }
    Config e;
    config_load_from(&e, TESTKEY);
    EXPECT(e.fps_cap == 120);                  /* 999 -> valido mais proximo */
    EXPECT(e.msaa == 8);                       /* 7 -> 8 */
    EXPECT(nearf(e.render_scale, 1.0f));       /* 3.0 -> clamp 1.0 */
    EXPECT(e.vsync == 1);                      /* 5 -> 1 */
    EXPECT(e.auto_quality == 0);
    EXPECT(e.streaks_mode == 0);                /* 9 -> fora de 0..2 -> 0 */
    EXPECT(e.streaks_intensity >= 0.0f);        /* -1 -> clamp */
    EXPECT(e.streaks_length <= 1.0f);           /* 5 -> clamp */
    EXPECT(e.chroma_on == 1);                    /* 7 -> !=0 -> 1 */
    EXPECT(e.chroma_strength <= 1.0f);           /* 9 -> clamp */
    EXPECT(e.vignette_amount >= 0.0f);           /* -3 -> clamp */
    EXPECT(e.fxaa_on == 1);                      /* 5 -> !=0 -> 1 */
    EXPECT(e.background_type == 0);              /* 9 -> fora de 0..3 -> 0 */
    EXPECT(e.bg_grad_angle <= 360.0f);           /* 999 -> clamp */
    EXPECT(e.bg_image_fit == 0);                 /* 9 -> fora de 0..2 -> 0 */
    EXPECT(e.bg_pan_speed >= 0.0f);              /* -1 -> clamp */
    EXPECT(e.particles_kind == 0);                /* 9 -> fora de 0..3 -> 0 */
    EXPECT(e.particles_density >= 0.0f);           /* -1 -> clamp */
    EXPECT(e.particles_speed <= 2.0f);             /* 9 -> clamp */
    EXPECT(e.particles_size_scale >= 0.0f);        /* -1 -> clamp */
    EXPECT(e.particles_opacity <= 2.0f);           /* 9 -> clamp */
    EXPECT(e.content_mode == CONTENT_TEXT);         /* 9 -> fora de 0..3 -> 0 */
    EXPECT(e.clock_show_date == 1);                 /* 5 -> !=0 -> 1 */
    EXPECT(e.clock_show_seconds == 1);              /* 5 -> !=0 -> 1 */
    EXPECT(e.svg_color_mode == 1);                  /* 9 -> !=0 -> 1 */
    EXPECT(e.mesh_size_scale <= 2.0f);              /* 9 -> clamp */

    RegDeleteKeyW(HKEY_CURRENT_USER, TESTKEY);
}
