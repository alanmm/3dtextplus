#include "test.h"
#include "presets.h"
#include <windows.h>
#include <string.h>
#include <math.h>

static int nearf(float a, float b) { return fabsf(a - b) < 1e-3f; }
/* base_color faz round-trip por um hex #RRGGBB de 8 bits/canal (ver
   config.c) - quantizacao introduz ate ~1/255 de erro por canal, maior
   que a tolerancia apertada de nearf() usada pro resto dos campos. */
static int nearf_color(float a, float b) { return fabsf(a - b) < 0.006f; }

void run_presets_tests(void)
{
    Config c;

    g_builtin_presets[0].build(&c);   /* Classico */
    EXPECT(c.material_mode == 0);
    EXPECT(c.bevel_mode == 0);
    EXPECT(nearf(c.bg_grad_angle, 101.0f));
    EXPECT(c.bloom_on == 1 && nearf(c.bloom_threshold, 1.0f));
    EXPECT(c.streaks_mode == 0);
    EXPECT(c.chroma_on == 0);
    EXPECT(c.vignette_on == 0);
    EXPECT(c.particles_on == 0);
    EXPECT(c.quality == 2);

    g_builtin_presets[1].build(&c);   /* Cinema */
    EXPECT(c.material_mode == 1);
    EXPECT(nearf(c.metalness, 0.9f) && nearf(c.roughness, 0.5f));
    EXPECT(c.env_mode == 0);
    EXPECT(c.background_type == 3);
    EXPECT(c.streaks_mode == 2);
    EXPECT(nearf(c.chroma_strength, 0.30f));
    EXPECT(c.particles_on == 1 && c.particles_kind == 1);
    EXPECT(nearf(c.particles_size_scale, 0.81f));

    g_builtin_presets[2].build(&c);   /* Neon */
    EXPECT(c.material_mode == 0);
    EXPECT(c.background_type == 0);
    EXPECT(nearf(c.base_r, 1.0f) && nearf(c.base_g, 0.1f) && nearf(c.base_b, 0.6f));
    EXPECT(c.streaks_mode == 1);
    EXPECT(nearf(c.bloom_intensity, 1.3f));
    EXPECT(c.particles_kind == 2);

    g_builtin_presets[3].build(&c);   /* Suave */
    EXPECT(c.material_mode == 3);
    EXPECT(c.background_type == 1);
    EXPECT(nearf(c.bg_color1_r, 0.85f));
    EXPECT(c.bloom_on == 1 && nearf(c.bloom_intensity, 0.12f));
    EXPECT(c.particles_kind == 0);
    EXPECT(c.quality == 1);

    /* preset_scope_copy: so os campos do escopo mudam, resto fica intocado */
    Config dst, src;
    config_defaults(&dst);
    config_defaults(&src);
    strcpy(src.text, "Nao deve vazar");
    src.max_angle_y = 999.0f;
    src.material_mode = 2;
    src.particles_kind = 3;
    src.particles_density = 0.9f; src.particles_speed = 0.5f;
    src.particles_size_scale = 0.7f; src.particles_opacity = 0.78f;

    preset_scope_copy(&dst, &src);
    EXPECT(strcmp(dst.text, "3D Text+") == 0);           /* nao mexeu */
    EXPECT(!nearf(dst.max_angle_y, 999.0f));               /* nao mexeu */
    EXPECT(dst.material_mode == 2);                        /* mexeu */
    EXPECT(dst.particles_kind == 3);
    EXPECT(nearf(dst.particles_stars_density, 0.9f));       /* memoria do tipo sincronizada */
    EXPECT(nearf(dst.particles_stars_size, 0.7f));
    EXPECT(nearf(dst.particles_stars_opacity, 0.78f));

    /* CRUD de presets salvos - base de teste separada */
    const wchar_t *TBASE = L"Software\\Modern3DText_test_presets";
    wchar_t names[8][PRESET_NAME_MAX];
    int n;

    /* limpeza defensiva antes (caso uma execucao anterior tenha falhado no meio) */
    n = preset_user_list_from(TBASE, names, 8);
    for (int i = 0; i < n; ++i) preset_user_delete_from(TBASE, names[i]);

    Config a;
    config_defaults(&a);
    a.material_mode = 3;
    a.background_type = 0;
    a.bg_color1_r = 0.11f; a.bg_color1_g = 0.22f; a.bg_color1_b = 0.33f;
    a.particles_on = 1; a.particles_kind = 2;
    a.particles_density = 0.55f; a.particles_speed = 0.66f;
    a.particles_size_scale = 0.77f; a.particles_opacity = 0.88f;
    a.base_r = 0.9f; a.base_g = 0.5f; a.base_b = 0.1f;
    a.quality = 1;

    preset_user_save_to(TBASE, L"Zulu", &a);
    preset_user_save_to(TBASE, L"Alfa", &a);

    n = preset_user_list_from(TBASE, names, 8);
    EXPECT(n == 2);
    EXPECT(wcscmp(names[0], L"Alfa") == 0);   /* ordem alfabetica */
    EXPECT(wcscmp(names[1], L"Zulu") == 0);

    Config b;
    EXPECT(preset_user_load_from(TBASE, L"Alfa", &b) == 1);
    EXPECT(b.material_mode == 3);
    EXPECT(nearf(b.bg_color1_r, 0.11f) && nearf(b.bg_color1_g, 0.22f) && nearf(b.bg_color1_b, 0.33f));
    EXPECT(b.particles_kind == 2);
    EXPECT(nearf(b.particles_sparks_density, 0.55f));
    EXPECT(nearf_color(b.base_r, 0.9f) && nearf_color(b.base_g, 0.5f) && nearf_color(b.base_b, 0.1f));
    EXPECT(b.quality == 1);

    EXPECT(preset_user_load_from(TBASE, L"NaoExiste", &b) == 0);

    preset_user_delete_from(TBASE, L"Alfa");
    n = preset_user_list_from(TBASE, names, 8);
    EXPECT(n == 1);
    EXPECT(wcscmp(names[0], L"Zulu") == 0);

    preset_user_delete_from(TBASE, L"Zulu");
    n = preset_user_list_from(TBASE, names, 8);
    EXPECT(n == 0);

    /* import/export via arquivo temporario */
    wchar_t tmpdir[MAX_PATH], tmpfile[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpdir);
    swprintf(tmpfile, MAX_PATH, L"%lsm3dt_preset_test.ini", tmpdir);

    Config ea;
    config_defaults(&ea);
    ea.material_mode = 1;
    ea.background_type = 3;
    ea.bg_neb_color1_r = 0.5f; ea.bg_neb_color1_g = 0.25f; ea.bg_neb_color1_b = 0.75f;
    ea.particles_on = 1; ea.particles_kind = 3;
    ea.particles_density = 0.6f; ea.particles_speed = 0.7f;
    ea.particles_size_scale = 0.8f; ea.particles_opacity = 0.9f;
    ea.base_r = 0.2f; ea.base_g = 0.4f; ea.base_b = 0.6f;
    ea.quality = 0;

    EXPECT(preset_export_file(tmpfile, &ea) == 1);

    Config eb;
    EXPECT(preset_import_file(tmpfile, &eb) == 1);
    EXPECT(eb.material_mode == 1);
    EXPECT(eb.background_type == 3);
    EXPECT(nearf(eb.bg_neb_color1_r, 0.5f) && nearf(eb.bg_neb_color1_g, 0.25f) && nearf(eb.bg_neb_color1_b, 0.75f));
    EXPECT(eb.particles_kind == 3);
    EXPECT(nearf(eb.particles_stars_density, 0.6f));
    EXPECT(nearf_color(eb.base_r, 0.2f) && nearf_color(eb.base_g, 0.4f) && nearf_color(eb.base_b, 0.6f));
    EXPECT(eb.quality == 0);

    DeleteFileW(tmpfile);
    EXPECT(preset_import_file(tmpfile, &eb) == 0);   /* arquivo nao existe mais */
}
