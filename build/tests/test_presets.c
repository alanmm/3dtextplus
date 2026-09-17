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

    g_builtin_presets[0].build(&c);   /* Inicial */
    EXPECT(c.material_mode == 1);
    EXPECT(nearf(c.metalness, 0.9f) && nearf(c.roughness, 0.25f));
    EXPECT(c.env_mode == 0);
    EXPECT(c.bevel_mode == 1);
    EXPECT(nearf(c.max_angle_y, 42.0f) && nearf(c.tilt_x, 8.0f) && nearf(c.period, 12.0f));
    EXPECT(nearf(c.depth, 0.12f));
    EXPECT(nearf(c.bevel_size, 0.005f) && c.bevel_segments == 8);
    EXPECT(c.background_type == 1);
    EXPECT(nearf(c.bg_grad_angle, 101.0f));
    EXPECT(c.bloom_on == 1 && nearf(c.bloom_threshold, 0.58f));
    EXPECT(c.streaks_mode == 2);
    EXPECT(c.particles_on == 1 && c.particles_kind == 0);
    EXPECT(c.quality == 2);

    g_builtin_presets[1].build(&c);   /* Classico */
    EXPECT(c.material_mode == 0);
    EXPECT(c.bevel_mode == 0);
    EXPECT(nearf(c.max_angle_y, 86.0f));
    EXPECT(nearf(c.depth, 0.10f) && c.bevel_segments == 6);
    EXPECT(nearf(c.bg_grad_angle, 87.0f));
    EXPECT(c.bloom_on == 1 && nearf(c.bloom_threshold, 1.0f));
    EXPECT(c.streaks_mode == 0);
    EXPECT(c.chroma_on == 0);
    EXPECT(c.vignette_on == 1);
    EXPECT(c.particles_on == 0);
    EXPECT(c.quality == 2);

    g_builtin_presets[2].build(&c);   /* Cinema */
    EXPECT(c.material_mode == 1);
    EXPECT(nearf(c.metalness, 0.8f) && nearf(c.roughness, 0.44f));
    EXPECT(c.env_mode == 0);
    EXPECT(c.background_type == 3);
    EXPECT(nearf(c.tilt_x, 10.0f) && nearf(c.period, 13.0f));
    EXPECT(c.streaks_mode == 1);
    EXPECT(nearf(c.chroma_strength, 0.96f));
    EXPECT(c.particles_on == 1 && c.particles_kind == 1);
    EXPECT(nearf(c.particles_size_scale, 0.87f));

    g_builtin_presets[3].build(&c);   /* Neon */
    EXPECT(c.material_mode == 2);
    EXPECT(c.background_type == 0);
    EXPECT(nearf(c.base_r, 0.32549f) && nearf(c.base_g, 0.47451f) && nearf(c.base_b, 1.0f));
    EXPECT(nearf(c.emissive_amount, 0.62f));
    EXPECT(c.streaks_mode == 2);
    EXPECT(nearf(c.bloom_intensity, 1.3f));
    EXPECT(c.particles_kind == 0);
    EXPECT(nearf(c.depth, 0.05f));

    g_builtin_presets[4].build(&c);   /* Suave */
    EXPECT(c.material_mode == 0);
    EXPECT(c.background_type == 1);
    EXPECT(nearf_color(c.bg_color1_r, 0.851f));
    EXPECT(c.bloom_on == 0 && nearf(c.bloom_intensity, 0.12f));
    EXPECT(c.particles_kind == 1);
    EXPECT(nearf(c.particles_size_scale, 1.70f));
    EXPECT(c.quality == 1);

    g_builtin_presets[5].build(&c);   /* Blueprint */
    EXPECT(c.material_mode == 3);
    EXPECT(c.background_type == 4);
    EXPECT(c.wireframe_xray == 1 && c.wireframe_fill == 1);
    EXPECT(nearf(c.bg_grid_density, 13.0f));
    EXPECT(nearf(c.bg_grid_color2_r, 0.07451f) && nearf(c.bg_grid_color2_b, 0.85100f));
    EXPECT(c.bevel_mode == 2);
    EXPECT(c.env_mode == 0 && c.env_path[0] == 0);   /* nao herda o HDRI local do arquivo exportado */
    EXPECT(nearf(c.max_angle_y, 52.0f) && nearf(c.tilt_x, 5.0f) && nearf(c.period, 16.0f));

    /* preset_scope_copy: so os campos do escopo mudam, resto fica intocado */
    Config dst, src;
    config_defaults(&dst);
    config_defaults(&src);
    strcpy(src.text, "Nao deve vazar");
    src.content_scale = 1.99f;
    src.max_angle_y = 111.0f;
    src.tilt_x = 22.0f;
    src.period = 15.0f;
    src.depth = 0.77f;
    src.bevel_size = 0.09f; src.bevel_depth = 0.08f; src.bevel_segments = 5;
    src.shell = 1; src.wall_thickness = 0.06f;
    src.material_mode = 2;
    src.env_mode = 1;
    wcscpy(src.env_path, L"C:\\imagens\\ambiente.jpg");
    src.particles_kind = 3;
    src.particles_density = 0.9f; src.particles_speed = 0.5f;
    src.particles_size_scale = 0.7f; src.particles_opacity = 0.78f;
    src.wireframe_thickness = 4.5f; src.wireframe_xray = 1;
    src.wireframe_fill = 1;
    src.wireframe_fill_r = 0.11f; src.wireframe_fill_g = 0.22f; src.wireframe_fill_b = 0.33f;
    src.bg_grid_color1_r = 0.44f; src.bg_grid_color1_g = 0.55f; src.bg_grid_color1_b = 0.66f;
    src.bg_grid_color2_r = 0.77f; src.bg_grid_color2_g = 0.88f; src.bg_grid_color2_b = 0.99f;
    src.bg_grid_density = 40.0f; src.bg_grid_dots = 1;
    src.bg_image_fit = 2; src.bg_pan_speed = 0.42f;

    preset_scope_copy(&dst, &src);
    EXPECT(strcmp(dst.text, "3D Text+") == 0);           /* nao mexeu (conteudo, fora do escopo) */
    EXPECT(!nearf(dst.content_scale, 1.99f));              /* nao mexeu (idem) */
    EXPECT(nearf(dst.max_angle_y, 111.0f));                /* mexeu (Movimento) */
    EXPECT(nearf(dst.tilt_x, 22.0f) && nearf(dst.period, 15.0f));
    EXPECT(nearf(dst.depth, 0.77f));                       /* mexeu (Geometria) */
    EXPECT(nearf(dst.bevel_size, 0.09f) && nearf(dst.bevel_depth, 0.08f) && dst.bevel_segments == 5);
    EXPECT(dst.shell == 1 && nearf(dst.wall_thickness, 0.06f));
    EXPECT(dst.material_mode == 2);                        /* mexeu */
    EXPECT(dst.env_mode == 1);
    EXPECT(wcscmp(dst.env_path, L"C:\\imagens\\ambiente.jpg") == 0);
    EXPECT(dst.particles_kind == 3);
    EXPECT(nearf(dst.particles_stars_density, 0.9f));       /* memoria do tipo sincronizada */
    EXPECT(nearf(dst.particles_stars_size, 0.7f));
    EXPECT(nearf(dst.particles_stars_opacity, 0.78f));
    EXPECT(nearf(dst.wireframe_thickness, 4.5f) && dst.wireframe_xray == 1);
    EXPECT(dst.wireframe_fill == 1);
    EXPECT(nearf(dst.wireframe_fill_r, 0.11f) && nearf(dst.wireframe_fill_g, 0.22f) && nearf(dst.wireframe_fill_b, 0.33f));
    EXPECT(nearf(dst.bg_grid_color1_r, 0.44f) && nearf(dst.bg_grid_color2_b, 0.99f));
    EXPECT(nearf(dst.bg_grid_density, 40.0f) && dst.bg_grid_dots == 1);
    EXPECT(dst.bg_image_fit == 2 && nearf(dst.bg_pan_speed, 0.42f));
    EXPECT(dst.bg_solid_customized == 1 && dst.bg_gradient_customized == 1);   /* aplicar preset = escolha deliberada */

    /* CRUD de presets salvos - base de teste separada */
    const wchar_t *TBASE = L"Software\\Modern3DText_test_presets";
    wchar_t names[8][PRESET_NAME_MAX];
    int n;

    /* limpeza defensiva antes (caso uma execucao anterior tenha falhado no meio) */
    n = preset_user_list_from(TBASE, names, 8);
    for (int i = 0; i < n; ++i) preset_user_delete_from(TBASE, names[i]);

    Config a;
    config_defaults(&a);
    a.material_mode = 2;
    a.env_mode = 1;
    wcscpy(a.env_path, L"D:\\hdri\\estudio.hdr");
    a.background_type = 0;
    a.bg_color1_r = 0.11f; a.bg_color1_g = 0.22f; a.bg_color1_b = 0.33f;
    a.particles_on = 1; a.particles_kind = 2;
    a.particles_density = 0.55f; a.particles_speed = 0.66f;
    a.particles_size_scale = 0.77f; a.particles_opacity = 0.88f;
    a.base_r = 0.9f; a.base_g = 0.5f; a.base_b = 0.1f;
    a.quality = 1;
    a.wireframe_thickness = 3.5f; a.wireframe_xray = 1;
    a.wireframe_fill = 1;
    a.wireframe_fill_r = 0.15f; a.wireframe_fill_g = 0.25f; a.wireframe_fill_b = 0.65f;
    a.bg_grid_color1_r = 0.1f; a.bg_grid_color1_g = 0.2f; a.bg_grid_color1_b = 0.3f;
    a.bg_grid_color2_r = 0.4f; a.bg_grid_color2_g = 0.5f; a.bg_grid_color2_b = 0.6f;
    a.bg_grid_density = 32.0f; a.bg_grid_dots = 1;
    a.max_angle_y = 60.0f; a.tilt_x = 12.0f; a.period = 7.0f;
    a.depth = 0.35f;
    a.bevel_size = 0.05f; a.bevel_depth = 0.04f; a.bevel_segments = 4;
    a.shell = 1; a.wall_thickness = 0.03f;
    a.bg_image_fit = 1; a.bg_pan_speed = 0.2f;

    preset_user_save_to(TBASE, L"Zulu", &a);
    preset_user_save_to(TBASE, L"Alfa", &a);

    n = preset_user_list_from(TBASE, names, 8);
    EXPECT(n == 2);
    EXPECT(wcscmp(names[0], L"Alfa") == 0);   /* ordem alfabetica */
    EXPECT(wcscmp(names[1], L"Zulu") == 0);

    Config b;
    EXPECT(preset_user_load_from(TBASE, L"Alfa", &b) == 1);
    EXPECT(b.material_mode == 2);
    EXPECT(b.env_mode == 1);
    EXPECT(wcscmp(b.env_path, L"D:\\hdri\\estudio.hdr") == 0);
    EXPECT(nearf(b.bg_color1_r, 0.11f) && nearf(b.bg_color1_g, 0.22f) && nearf(b.bg_color1_b, 0.33f));
    EXPECT(b.particles_kind == 2);
    EXPECT(nearf(b.particles_sparks_density, 0.55f));
    EXPECT(nearf_color(b.base_r, 0.9f) && nearf_color(b.base_g, 0.5f) && nearf_color(b.base_b, 0.1f));
    EXPECT(b.quality == 1);
    EXPECT(nearf(b.wireframe_thickness, 3.5f) && b.wireframe_xray == 1);
    EXPECT(b.wireframe_fill == 1);
    EXPECT(nearf(b.wireframe_fill_r, 0.15f) && nearf(b.wireframe_fill_g, 0.25f) && nearf(b.wireframe_fill_b, 0.65f));
    EXPECT(nearf(b.bg_grid_color1_r, 0.1f) && nearf(b.bg_grid_color2_g, 0.5f));
    EXPECT(nearf(b.bg_grid_density, 32.0f) && b.bg_grid_dots == 1);
    EXPECT(nearf(b.max_angle_y, 60.0f) && nearf(b.tilt_x, 12.0f) && nearf(b.period, 7.0f));
    EXPECT(nearf(b.depth, 0.35f));
    EXPECT(nearf(b.bevel_size, 0.05f) && nearf(b.bevel_depth, 0.04f) && b.bevel_segments == 4);
    EXPECT(b.shell == 1 && nearf(b.wall_thickness, 0.03f));
    EXPECT(b.bg_image_fit == 1 && nearf(b.bg_pan_speed, 0.2f));

    EXPECT(preset_user_load_from(TBASE, L"NaoExiste", &b) == 0);

    preset_user_delete_from(TBASE, L"Alfa");
    n = preset_user_list_from(TBASE, names, 8);
    EXPECT(n == 1);
    EXPECT(wcscmp(names[0], L"Zulu") == 0);

    preset_user_delete_from(TBASE, L"Zulu");
    n = preset_user_list_from(TBASE, names, 8);
    EXPECT(n == 0);

    /* backup/restauracao via arquivo temporario - varios presets num arquivo so */
    wchar_t tmpdir[MAX_PATH], tmpfile[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpdir);
    swprintf(tmpfile, MAX_PATH, L"%lsm3dt_preset_test.ini", tmpdir);

    Config ea;
    config_defaults(&ea);
    ea.material_mode = 1;
    ea.env_mode = 1;
    wcscpy(ea.env_path, L"E:\\fotos\\hdri_quintal.jpg");
    ea.background_type = 3;
    ea.bg_neb_color1_r = 0.5f; ea.bg_neb_color1_g = 0.25f; ea.bg_neb_color1_b = 0.75f;
    ea.particles_on = 1; ea.particles_kind = 3;
    ea.particles_density = 0.6f; ea.particles_speed = 0.7f;
    ea.particles_size_scale = 0.8f; ea.particles_opacity = 0.9f;
    ea.base_r = 0.2f; ea.base_g = 0.4f; ea.base_b = 0.6f;
    ea.quality = 0;
    ea.wireframe_thickness = 5.0f; ea.wireframe_xray = 1;
    ea.wireframe_fill = 1;
    ea.wireframe_fill_r = 0.05f; ea.wireframe_fill_g = 0.35f; ea.wireframe_fill_b = 0.85f;
    ea.bg_grid_color1_r = 0.15f; ea.bg_grid_color1_g = 0.35f; ea.bg_grid_color1_b = 0.55f;
    ea.bg_grid_color2_r = 0.65f; ea.bg_grid_color2_g = 0.75f; ea.bg_grid_color2_b = 0.95f;
    ea.bg_grid_density = 48.0f; ea.bg_grid_dots = 1;
    ea.max_angle_y = 75.0f; ea.tilt_x = 18.0f; ea.period = 11.0f;
    ea.depth = 0.5f;
    ea.bevel_size = 0.07f; ea.bevel_depth = 0.06f; ea.bevel_segments = 6;
    ea.shell = 1; ea.wall_thickness = 0.045f;
    ea.bg_image_fit = 2; ea.bg_pan_speed = 0.33f;

    Config ec;
    config_defaults(&ec);
    ec.material_mode = 2;
    ec.background_type = 0;
    ec.quality = 2;

    preset_user_save_to(TBASE, L"Bravo", &ea);
    preset_user_save_to(TBASE, L"Charlie", &ec);

    EXPECT(preset_backup_export_file_from(TBASE, tmpfile) == 1);

    preset_user_delete_from(TBASE, L"Bravo");
    preset_user_delete_from(TBASE, L"Charlie");
    n = preset_user_list_from(TBASE, names, 8);
    EXPECT(n == 0);

    PresetBackupEntry entries[8];
    int en = preset_backup_parse_file(tmpfile, entries, 8);
    EXPECT(en == 2);
    /* preset_user_list_from devolve em ordem alfabetica; preset_backup_export_file_from
       usa a mesma listagem, entao a ordem no arquivo (e no parse) tambem e' alfabetica */
    EXPECT(wcscmp(entries[0].name, L"Bravo") == 0);
    EXPECT(entries[0].cfg.material_mode == 1);
    EXPECT(entries[0].cfg.env_mode == 1);
    EXPECT(wcscmp(entries[0].cfg.env_path, L"E:\\fotos\\hdri_quintal.jpg") == 0);
    EXPECT(entries[0].cfg.background_type == 3);
    EXPECT(nearf(entries[0].cfg.bg_neb_color1_r, 0.5f) && nearf(entries[0].cfg.bg_neb_color1_g, 0.25f) && nearf(entries[0].cfg.bg_neb_color1_b, 0.75f));
    EXPECT(entries[0].cfg.particles_kind == 3);
    EXPECT(nearf(entries[0].cfg.particles_stars_density, 0.6f));
    EXPECT(nearf_color(entries[0].cfg.base_r, 0.2f) && nearf_color(entries[0].cfg.base_g, 0.4f) && nearf_color(entries[0].cfg.base_b, 0.6f));
    EXPECT(entries[0].cfg.quality == 0);
    EXPECT(nearf(entries[0].cfg.wireframe_thickness, 5.0f) && entries[0].cfg.wireframe_xray == 1);
    EXPECT(entries[0].cfg.wireframe_fill == 1);
    EXPECT(nearf(entries[0].cfg.wireframe_fill_r, 0.05f) && nearf(entries[0].cfg.wireframe_fill_g, 0.35f) && nearf(entries[0].cfg.wireframe_fill_b, 0.85f));
    EXPECT(nearf(entries[0].cfg.bg_grid_color1_r, 0.15f) && nearf(entries[0].cfg.bg_grid_color2_g, 0.75f));
    EXPECT(nearf(entries[0].cfg.bg_grid_density, 48.0f) && entries[0].cfg.bg_grid_dots == 1);
    EXPECT(nearf(entries[0].cfg.max_angle_y, 75.0f) && nearf(entries[0].cfg.tilt_x, 18.0f) && nearf(entries[0].cfg.period, 11.0f));
    EXPECT(nearf(entries[0].cfg.depth, 0.5f));
    EXPECT(nearf(entries[0].cfg.bevel_size, 0.07f) && nearf(entries[0].cfg.bevel_depth, 0.06f) && entries[0].cfg.bevel_segments == 6);
    EXPECT(entries[0].cfg.shell == 1 && nearf(entries[0].cfg.wall_thickness, 0.045f));
    EXPECT(entries[0].cfg.bg_image_fit == 2 && nearf(entries[0].cfg.bg_pan_speed, 0.33f));

    EXPECT(wcscmp(entries[1].name, L"Charlie") == 0);
    EXPECT(entries[1].cfg.material_mode == 2);
    EXPECT(entries[1].cfg.quality == 2);

    DeleteFileW(tmpfile);
    EXPECT(preset_backup_parse_file(tmpfile, entries, 8) == -1);   /* arquivo nao existe mais */
}
