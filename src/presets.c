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
    dst->emissive_r = src->emissive_r; dst->emissive_g = src->emissive_g; dst->emissive_b = src->emissive_b;
    dst->emissive_amount = src->emissive_amount;
    dst->refraction = src->refraction;
    dst->env_mode = src->env_mode;
    wcsncpy(dst->env_path, src->env_path, 511);
    dst->env_path[511] = 0;
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

/* "Inicial" = exatamente o padrao de fabrica estabelecido na Fase 8b-1
   (config_defaults() ja e' esse baseline - nenhum campo extra a sobrescrever) */
static void preset_inicial(Config *out)
{
    config_defaults(out);
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
    { STR_PRESET_NAME_INICIAL,  preset_inicial },
    { STR_PRESET_NAME_CLASSICO, preset_classico },
    { STR_PRESET_NAME_CINEMA,   preset_cinema },
    { STR_PRESET_NAME_NEON,     preset_neon },
    { STR_PRESET_NAME_SUAVE,    preset_suave },
};

static const wchar_t *PRESETS_BASE = L"Software\\Modern3DText\\Presets";

static int wcscmp_qsort(const void *a, const void *b)
{
    return wcscmp((const wchar_t *)a, (const wchar_t *)b);
}

void preset_user_save_to(const wchar_t *base, const wchar_t *name, const Config *from)
{
    Config tmp;
    config_defaults(&tmp);
    preset_scope_copy(&tmp, from);

    wchar_t path[600];
    swprintf(path, 600, L"%ls\\%ls", base, name);
    config_save_to(&tmp, path);
}

int preset_user_load_from(const wchar_t *base, const wchar_t *name, Config *out)
{
    wchar_t path[600];
    swprintf(path, 600, L"%ls\\%ls", base, name);

    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return 0;
    RegCloseKey(k);

    Config tmp;
    config_load_from(&tmp, path);
    preset_scope_copy(out, &tmp);
    return 1;
}

void preset_user_delete_from(const wchar_t *base, const wchar_t *name)
{
    wchar_t path[600];
    swprintf(path, 600, L"%ls\\%ls", base, name);
    RegDeleteKeyW(HKEY_CURRENT_USER, path);
}

int preset_user_list_from(const wchar_t *base, wchar_t names[][PRESET_NAME_MAX], int max)
{
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, base, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return 0;

    int n = 0;
    for (DWORD i = 0; n < max; ++i) {
        wchar_t name[PRESET_NAME_MAX];
        DWORD cch = PRESET_NAME_MAX;
        if (RegEnumKeyExW(k, i, name, &cch, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
            break;
        wcsncpy(names[n], name, PRESET_NAME_MAX - 1);
        names[n][PRESET_NAME_MAX - 1] = 0;
        ++n;
    }
    RegCloseKey(k);
    if (n > 1)
        qsort(names, (size_t)n, PRESET_NAME_MAX * sizeof(wchar_t), wcscmp_qsort);
    return n;
}

void preset_user_save(const wchar_t *name, const Config *from) { preset_user_save_to(PRESETS_BASE, name, from); }
int  preset_user_load(const wchar_t *name, Config *out) { return preset_user_load_from(PRESETS_BASE, name, out); }
void preset_user_delete(const wchar_t *name) { preset_user_delete_from(PRESETS_BASE, name); }
int  preset_user_list(wchar_t names[][PRESET_NAME_MAX], int max) { return preset_user_list_from(PRESETS_BASE, names, max); }

static void preset_dump_fields(FILE *f, const Config *from)
{
    unsigned r = (unsigned)(from->base_r * 255.0f + 0.5f);
    unsigned g = (unsigned)(from->base_g * 255.0f + 0.5f);
    unsigned bl = (unsigned)(from->base_b * 255.0f + 0.5f);

    fwprintf(f, L"material_mode=%d\r\n", from->material_mode);
    fwprintf(f, L"metalness=%.5f\r\n", (double)from->metalness);
    fwprintf(f, L"roughness=%.5f\r\n", (double)from->roughness);
    fwprintf(f, L"emissive_r=%.5f\r\n", (double)from->emissive_r);
    fwprintf(f, L"emissive_g=%.5f\r\n", (double)from->emissive_g);
    fwprintf(f, L"emissive_b=%.5f\r\n", (double)from->emissive_b);
    fwprintf(f, L"emissive_amount=%.5f\r\n", (double)from->emissive_amount);
    fwprintf(f, L"refraction=%.5f\r\n", (double)from->refraction);
    fwprintf(f, L"env_mode=%d\r\n", from->env_mode);
    fwprintf(f, L"env_path=%ls\r\n", from->env_path);
    fwprintf(f, L"bevel_mode=%d\r\n", from->bevel_mode);
    fwprintf(f, L"background_type=%d\r\n", from->background_type);
    fwprintf(f, L"bg_color1_r=%.5f\r\n", (double)from->bg_color1_r);
    fwprintf(f, L"bg_color1_g=%.5f\r\n", (double)from->bg_color1_g);
    fwprintf(f, L"bg_color1_b=%.5f\r\n", (double)from->bg_color1_b);
    fwprintf(f, L"bg_color2_r=%.5f\r\n", (double)from->bg_color2_r);
    fwprintf(f, L"bg_color2_g=%.5f\r\n", (double)from->bg_color2_g);
    fwprintf(f, L"bg_color2_b=%.5f\r\n", (double)from->bg_color2_b);
    fwprintf(f, L"bg_grad_angle=%.5f\r\n", (double)from->bg_grad_angle);
    fwprintf(f, L"bg_neb_color1_r=%.5f\r\n", (double)from->bg_neb_color1_r);
    fwprintf(f, L"bg_neb_color1_g=%.5f\r\n", (double)from->bg_neb_color1_g);
    fwprintf(f, L"bg_neb_color1_b=%.5f\r\n", (double)from->bg_neb_color1_b);
    fwprintf(f, L"bg_neb_color2_r=%.5f\r\n", (double)from->bg_neb_color2_r);
    fwprintf(f, L"bg_neb_color2_g=%.5f\r\n", (double)from->bg_neb_color2_g);
    fwprintf(f, L"bg_neb_color2_b=%.5f\r\n", (double)from->bg_neb_color2_b);
    fwprintf(f, L"bloom_on=%d\r\n", from->bloom_on);
    fwprintf(f, L"bloom_threshold=%.5f\r\n", (double)from->bloom_threshold);
    fwprintf(f, L"bloom_intensity=%.5f\r\n", (double)from->bloom_intensity);
    fwprintf(f, L"bloom_radius=%.5f\r\n", (double)from->bloom_radius);
    fwprintf(f, L"streaks_mode=%d\r\n", from->streaks_mode);
    fwprintf(f, L"streaks_intensity=%.5f\r\n", (double)from->streaks_intensity);
    fwprintf(f, L"streaks_length=%.5f\r\n", (double)from->streaks_length);
    fwprintf(f, L"chroma_on=%d\r\n", from->chroma_on);
    fwprintf(f, L"chroma_strength=%.5f\r\n", (double)from->chroma_strength);
    fwprintf(f, L"vignette_on=%d\r\n", from->vignette_on);
    fwprintf(f, L"vignette_amount=%.5f\r\n", (double)from->vignette_amount);
    fwprintf(f, L"fxaa_on=%d\r\n", from->fxaa_on);
    fwprintf(f, L"particles_on=%d\r\n", from->particles_on);
    fwprintf(f, L"particles_kind=%d\r\n", from->particles_kind);
    fwprintf(f, L"particles_density=%.5f\r\n", (double)from->particles_density);
    fwprintf(f, L"particles_speed=%.5f\r\n", (double)from->particles_speed);
    fwprintf(f, L"particles_size_scale=%.5f\r\n", (double)from->particles_size_scale);
    fwprintf(f, L"particles_opacity=%.5f\r\n", (double)from->particles_opacity);
    fwprintf(f, L"base_color=#%02X%02X%02X\r\n", r & 0xFF, g & 0xFF, bl & 0xFF);
    fwprintf(f, L"quality=%d\r\n", from->quality);
}

int preset_backup_export_file_from(const wchar_t *base, const wchar_t *path)
{
    wchar_t names[PRESET_BACKUP_MAX][PRESET_NAME_MAX];
    int n = preset_user_list_from(base, names, PRESET_BACKUP_MAX);

    FILE *f = _wfopen(path, L"w, ccs=UTF-8");
    if (!f) return 0;

    for (int i = 0; i < n; ++i) {
        Config cfg;
        if (!preset_user_load_from(base, names[i], &cfg)) continue;
        fwprintf(f, L"[Preset:%ls]\r\n", names[i]);
        preset_dump_fields(f, &cfg);
        fwprintf(f, L"\r\n");
    }

    fclose(f);
    return 1;
}

int preset_backup_export_file(const wchar_t *path) { return preset_backup_export_file_from(PRESETS_BASE, path); }

/* processa uma secao completa acumulada na subchave temporaria (chave
   de registro generica ja preenchida linha a linha) pro slot out[idx],
   e limpa a subchave em seguida. */
static void preset_backup_flush_section(const wchar_t *tmpkey, const wchar_t *name,
                                         PresetBackupEntry *slot)
{
    Config tmp;
    config_load_from(&tmp, tmpkey);
    RegDeleteKeyW(HKEY_CURRENT_USER, tmpkey);

    config_defaults(&slot->cfg);
    preset_scope_copy(&slot->cfg, &tmp);
    wcsncpy(slot->name, name, PRESET_NAME_MAX - 1);
    slot->name[PRESET_NAME_MAX - 1] = 0;
}

int preset_backup_parse_file(const wchar_t *path, PresetBackupEntry *out, int max)
{
    FILE *f = _wfopen(path, L"r, ccs=UTF-8");
    if (!f) return -1;

    const wchar_t *tmpkey = L"Software\\Modern3DText\\Presets\\_import_tmp";
    wchar_t cur_name[PRESET_NAME_MAX] = L"";
    HKEY k = NULL;
    int n = 0;
    wchar_t line[256];

    while (fgetws(line, 256, f)) {
        size_t len = wcslen(line);
        while (len > 0 && (line[len - 1] == L'\n' || line[len - 1] == L'\r')) line[--len] = 0;

        if (len > 9 && wcsncmp(line, L"[Preset:", 8) == 0 && line[len - 1] == L']') {
            if (k) {
                if (n < max) preset_backup_flush_section(tmpkey, cur_name, &out[n++]);
                else RegDeleteKeyW(HKEY_CURRENT_USER, tmpkey);
                RegCloseKey(k);
                k = NULL;
            }

            size_t namelen = len - 9;
            if (namelen >= PRESET_NAME_MAX) namelen = PRESET_NAME_MAX - 1;
            wcsncpy(cur_name, line + 8, namelen);
            cur_name[namelen] = 0;

            if (RegCreateKeyExW(HKEY_CURRENT_USER, tmpkey, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL) != ERROR_SUCCESS)
                k = NULL;
            continue;
        }

        if (!k) continue;
        wchar_t *eq = wcschr(line, L'=');
        if (!eq) continue;
        *eq = 0;
        wchar_t *val = eq + 1;
        RegSetValueExW(k, line, 0, REG_SZ, (const BYTE *)val, (DWORD)((wcslen(val) + 1) * sizeof(wchar_t)));
    }
    fclose(f);

    if (k) {
        if (n < max) preset_backup_flush_section(tmpkey, cur_name, &out[n++]);
        else RegDeleteKeyW(HKEY_CURRENT_USER, tmpkey);
        RegCloseKey(k);
    }

    return n;
}
