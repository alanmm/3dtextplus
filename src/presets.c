#include "presets.h"
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>

void preset_scope_copy(Config *dst, const Config *src)
{
    dst->material_mode = src->material_mode;
    dst->metalness = src->metalness;
    dst->roughness = src->roughness;
    dst->env_mode = src->env_mode;
    dst->bevel_mode = src->bevel_mode;
    dst->background_type = src->background_type;
    dst->bg_color1_r = src->bg_color1_r; dst->bg_color1_g = src->bg_color1_g; dst->bg_color1_b = src->bg_color1_b;
    dst->bg_color2_r = src->bg_color2_r; dst->bg_color2_g = src->bg_color2_g; dst->bg_color2_b = src->bg_color2_b;
    dst->bg_grad_angle = src->bg_grad_angle;
    dst->bg_neb_color1_r = src->bg_neb_color1_r; dst->bg_neb_color1_g = src->bg_neb_color1_g; dst->bg_neb_color1_b = src->bg_neb_color1_b;
    dst->bg_neb_color2_r = src->bg_neb_color2_r; dst->bg_neb_color2_g = src->bg_neb_color2_g; dst->bg_neb_color2_b = src->bg_neb_color2_b;
    dst->bloom_on = src->bloom_on;
    dst->bloom_threshold = src->bloom_threshold;
    dst->bloom_intensity = src->bloom_intensity;
    dst->bloom_radius = src->bloom_radius;
    dst->streaks_mode = src->streaks_mode;
    dst->streaks_intensity = src->streaks_intensity;
    dst->streaks_length = src->streaks_length;
    dst->chroma_on = src->chroma_on;
    dst->chroma_strength = src->chroma_strength;
    dst->vignette_on = src->vignette_on;
    dst->vignette_amount = src->vignette_amount;
    dst->fxaa_on = src->fxaa_on;
    dst->particles_on = src->particles_on;
    dst->particles_kind = src->particles_kind;
    dst->particles_density = src->particles_density;
    dst->particles_speed = src->particles_speed;
    dst->particles_size_scale = src->particles_size_scale;
    dst->particles_opacity = src->particles_opacity;
    switch (src->particles_kind) {
        case 0:
            dst->particles_dust_density = src->particles_density;
            dst->particles_dust_size = src->particles_size_scale;
            dst->particles_dust_opacity = src->particles_opacity;
            break;
        case 1:
            dst->particles_bokeh_density = src->particles_density;
            dst->particles_bokeh_size = src->particles_size_scale;
            dst->particles_bokeh_opacity = src->particles_opacity;
            break;
        case 2:
            dst->particles_sparks_density = src->particles_density;
            dst->particles_sparks_size = src->particles_size_scale;
            dst->particles_sparks_opacity = src->particles_opacity;
            break;
        case 3:
            dst->particles_stars_density = src->particles_density;
            dst->particles_stars_size = src->particles_size_scale;
            dst->particles_stars_opacity = src->particles_opacity;
            break;
    }
    dst->base_r = src->base_r; dst->base_g = src->base_g; dst->base_b = src->base_b;
    dst->quality = src->quality;
}

static void preset_classico(Config *out)
{
    config_defaults(out);
    out->material_mode = 0;
    out->metalness = 0.9f; out->roughness = 0.5f; out->env_mode = 0;
    out->bevel_mode = 0;
    out->base_r = 0.72157f; out->base_g = 0.74118f; out->base_b = 0.78039f;
    out->background_type = 1;
    out->bg_color1_r = 0.06275f; out->bg_color1_g = 0.03922f; out->bg_color1_b = 0.03922f;
    out->bg_color2_r = 0.04706f; out->bg_color2_g = 0.07451f; out->bg_color2_b = 0.13725f;
    out->bg_grad_angle = 101.0f;
    out->bloom_on = 1; out->bloom_threshold = 1.0f; out->bloom_intensity = 0.3f; out->bloom_radius = 0.18f;
    out->streaks_mode = 0; out->streaks_intensity = 0.76f; out->streaks_length = 0.14f;
    out->chroma_on = 0; out->chroma_strength = 0.52f;
    out->vignette_on = 0; out->vignette_amount = 0.35f;
    out->fxaa_on = 1;
    out->particles_on = 0; out->particles_kind = 0;
    out->particles_density = 0.43f; out->particles_speed = 0.76f; out->particles_size_scale = 0.80f; out->particles_opacity = 0.50f;
    out->quality = 2;
}

static void preset_cinema(Config *out)
{
    config_defaults(out);
    out->material_mode = 1;
    out->metalness = 0.9f; out->roughness = 0.5f; out->env_mode = 0;
    out->bevel_mode = 1;
    out->base_r = 0.72157f; out->base_g = 0.74118f; out->base_b = 0.78039f;
    out->background_type = 3;
    out->bg_color1_r = 0.06275f; out->bg_color1_g = 0.03922f; out->bg_color1_b = 0.03922f;
    out->bg_color2_r = 0.04706f; out->bg_color2_g = 0.07451f; out->bg_color2_b = 0.13725f;
    out->bg_grad_angle = 101.0f;
    out->bg_neb_color1_r = 0.02745f; out->bg_neb_color1_g = 0.01961f; out->bg_neb_color1_b = 0.07843f;
    out->bg_neb_color2_r = 0.24706f; out->bg_neb_color2_g = 0.09804f; out->bg_neb_color2_b = 0.34902f;
    out->bloom_on = 1; out->bloom_threshold = 0.64f; out->bloom_intensity = 0.65f; out->bloom_radius = 0.27f;
    out->streaks_mode = 2; out->streaks_intensity = 0.76f; out->streaks_length = 0.14f;
    out->chroma_on = 1; out->chroma_strength = 0.30f;
    out->vignette_on = 1; out->vignette_amount = 0.35f;
    out->fxaa_on = 1;
    out->particles_on = 1; out->particles_kind = 1;
    out->particles_density = 0.43f; out->particles_speed = 0.76f; out->particles_size_scale = 0.81f; out->particles_opacity = 0.42f;
    out->quality = 2;
}

static void preset_neon(Config *out)
{
    config_defaults(out);
    out->material_mode = 0;
    out->metalness = 0.9f; out->roughness = 0.5f; out->env_mode = 0;
    out->bevel_mode = 1;
    out->base_r = 1.0f; out->base_g = 0.1f; out->base_b = 0.6f;
    out->background_type = 0;
    out->bg_color1_r = 0.03f; out->bg_color1_g = 0.01f; out->bg_color1_b = 0.05f;
    out->bg_color2_r = 0.04706f; out->bg_color2_g = 0.07451f; out->bg_color2_b = 0.13725f;
    out->bg_grad_angle = 101.0f;
    out->bg_neb_color1_r = 0.02745f; out->bg_neb_color1_g = 0.01961f; out->bg_neb_color1_b = 0.07843f;
    out->bg_neb_color2_r = 0.24706f; out->bg_neb_color2_g = 0.09804f; out->bg_neb_color2_b = 0.34902f;
    out->bloom_on = 1; out->bloom_threshold = 0.4f; out->bloom_intensity = 1.3f; out->bloom_radius = 0.4f;
    out->streaks_mode = 1; out->streaks_intensity = 1.2f; out->streaks_length = 0.2f;
    out->chroma_on = 1; out->chroma_strength = 0.6f;
    out->vignette_on = 1; out->vignette_amount = 0.45f;
    out->fxaa_on = 1;
    out->particles_on = 1; out->particles_kind = 2;
    out->particles_density = 0.42f; out->particles_speed = 0.76f; out->particles_size_scale = 0.72f; out->particles_opacity = 0.42f;
    out->quality = 2;
}

static void preset_suave(Config *out)
{
    config_defaults(out);
    out->material_mode = 3;
    out->metalness = 0.9f; out->roughness = 0.5f; out->env_mode = 0;
    out->bevel_mode = 1;
    out->base_r = 0.20f; out->base_g = 0.21f; out->base_b = 0.24f;
    out->background_type = 1;
    out->bg_color1_r = 0.85f; out->bg_color1_g = 0.87f; out->bg_color1_b = 0.92f;
    out->bg_color2_r = 0.78f; out->bg_color2_g = 0.82f; out->bg_color2_b = 0.90f;
    out->bg_grad_angle = 90.0f;
    out->bg_neb_color1_r = 0.02745f; out->bg_neb_color1_g = 0.01961f; out->bg_neb_color1_b = 0.07843f;
    out->bg_neb_color2_r = 0.24706f; out->bg_neb_color2_g = 0.09804f; out->bg_neb_color2_b = 0.34902f;
    out->bloom_on = 1; out->bloom_threshold = 1.6f; out->bloom_intensity = 0.12f; out->bloom_radius = 0.12f;
    out->streaks_mode = 0; out->streaks_intensity = 0.76f; out->streaks_length = 0.14f;
    out->chroma_on = 0; out->chroma_strength = 0.52f;
    out->vignette_on = 0; out->vignette_amount = 0.35f;
    out->fxaa_on = 1;
    out->particles_on = 1; out->particles_kind = 0;
    out->particles_density = 0.43f; out->particles_speed = 0.76f; out->particles_size_scale = 0.80f; out->particles_opacity = 0.50f;
    out->quality = 1;
}

const BuiltinPreset g_builtin_presets[BUILTIN_PRESET_COUNT] = {
    { STR_PRESET_NAME_CLASSICO, preset_classico },
    { STR_PRESET_NAME_CINEMA,   preset_cinema },
    { STR_PRESET_NAME_NEON,     preset_neon },
    { STR_PRESET_NAME_SUAVE,    preset_suave },
};
