#include "test.h"
#include "presets.h"
#include <windows.h>
#include <string.h>
#include <math.h>

static int nearf(float a, float b) { return fabsf(a - b) < 1e-3f; }

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
}
