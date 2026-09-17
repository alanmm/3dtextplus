#include "i18n.h"
#include "util/log.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "embedded.h"

static const char *const KEY_NAMES[STR_COUNT] = {
    "btn.cancel", "btn.apply",
    "tab.content", "tab.motion", "tab.material", "tab.geometry",
    "tab.effects", "tab.perf", "tab.post", "tab.bg", "tab.particles",
    "common.choose", "common.clear", "common.choose_color",
    "common.file_label", "placeholder.none_m",

    "content.mode_label", "content.mode.text", "content.mode.clock",
    "content.mode.svg", "content.mode.mesh", "content.text_label",
    "content.font_label", "content.svg_colors_label",
    "content.svg_color.preserve", "content.svg_color.single",
    "content.mesh_scale_label", "content.mesh_usemat", "content.bold",
    "content.italic", "content.clock_date", "content.clock_seconds",
    "content.color_label",

    "motion.angle_label", "motion.tilt_label", "motion.period_label",

    "material.label", "material.mode.classic", "material.mode.metallic",
    "material.mode.glass",
    "material.mode.wireframe",
    "material.metalness_label", "material.roughness_label",
    "material.emissive_label", "material.emissive_color_btn",
    "material.edgebias_label",
    "material.wire_thickness_label", "material.wire_xray",
    "material.env_label", "placeholder.procedural",
    "material.env_mode.embedded", "material.env_mode.custom", "material.env_mode.none",

    "geometry.depth_label", "geometry.bevel_label",
    "geometry.bevel.rounded", "geometry.bevel.geometric",
    "geometry.bevel.off", "geometry.bsize_label",
    "geometry.bdepth_label", "geometry.bseg_label", "geometry.shell",
    "geometry.wall_label", "geometry.quality_label",
    "geometry.quality.low", "geometry.quality.medium",
    "geometry.quality.high",

    "effects.bloom", "effects.threshold_label",
    "effects.bloom_intensity_label", "effects.spread_label",
    "effects.streaks_label", "effects.streaks.off",
    "effects.streaks.starburst", "effects.streaks.anamorphic",
    "effects.streaks_intensity_label", "effects.length_label",
    "effects.hint",

    "perf.fps_label", "perf.fps.unlimited", "perf.vsync",
    "perf.msaa_label", "perf.msaa.off", "perf.rscale_label",
    "perf.autoq", "perf.hint", "perf.language_label",
    "perf.language.auto", "perf.language.pt", "perf.language.en",

    "post.chroma", "post.chroma_intensity_label", "post.vignette",
    "post.vignette_intensity_label", "post.fxaa", "post.hint",

    "bg.type_label", "bg.type.solid", "bg.type.gradient",
    "bg.type.image", "bg.type.nebula",
    "bg.type.grid",
    "bg.color1_label",
    "bg.color2_label", "bg.angle_label", "bg.image_label",
    "placeholder.none_f", "bg.fit_label", "bg.fit.cover",
    "bg.fit.contain", "bg.fit.tile", "bg.pan_label",
    "bg.nebula_label", "bg.nebula_color1_btn", "bg.nebula_color2_btn",
    "bg.grid_label", "bg.grid_color1_btn", "bg.grid_color2_btn",

    "particles.enable", "particles.kind_label", "particles.kind.dust",
    "particles.kind.bokeh", "particles.kind.sparks",
    "particles.kind.stars", "particles.density_label",
    "particles.speed_label", "particles.size_label",
    "particles.opacity_label",

    "filter.svg", "filter.images", "filter.all_short",
    "filter.mesh_all", "filter.obj", "filter.stl", "filter.glb",
    "filter.gltf", "filter.all_long",

    "msg.file_too_big_text", "msg.file_too_big_title",

    "error.mesh_invalid", "error.svg_no_shape", "error.svg_empty",

    "preset.label", "preset.save_btn", "preset.delete_btn",
    "preset.name_title", "preset.name_label",
    "preset.apply_confirm_title", "preset.apply_confirm",
    "preset.delete_confirm_title", "preset.delete_confirm",
    "preset.overwrite_confirm_title", "preset.overwrite_confirm",
    "preset.cant_overwrite_builtin_title", "preset.cant_overwrite_builtin",
    "preset.name.inicial",
    "preset.name.classico", "preset.name.cinema",
    "preset.name.neon", "preset.name.suave",
    "filter.ini",
    "preset.restore_failed_title", "preset.restore_failed",
    "preset.backup_failed_title", "preset.backup_failed",
    "preset.backup_empty_title", "preset.backup_empty",
    "preset.restore_summary_title", "preset.restore_summary",
    "preset.restore_conflict_title", "preset.restore_conflict",
    "preset.restore_conflict_overwrite", "preset.restore_conflict_skip",
    "preset.restore_conflict_overwrite_all",

    "menu.about", "menu.restore_presets", "menu.backup_presets",
    "menu.language", "about.title", "about.version_fmt"
};

static wchar_t *g_pt[STR_COUNT];
static wchar_t *g_en[STR_COUNT];
static int g_have[2][STR_COUNT];   /* [0]=pt, [1]=en */
static int g_active_lang;          /* 0=pt, 1=en - indice interno, distinto do 0/1/2 de i18n_init */

/* decodifica \n e \\ dentro do valor; escreve em buf (UTF-16, ja
   convertido de UTF-8 pelo chamador) */
static void unescape_inplace(wchar_t *s)
{
    wchar_t *r = s, *w = s;
    while (*r) {
        if (r[0] == L'\\' && r[1] == L'n') { *w++ = L'\n'; r += 2; }
        else if (r[0] == L'\\' && r[1] == L'\\') { *w++ = L'\\'; r += 2; }
        else *w++ = *r++;
    }
    *w = 0;
}

static void parse_lang_file(const char *utf8, wchar_t **out, int *have)
{
    memset(have, 0, sizeof(int) * STR_COUNT);

    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    wchar_t *wide = (wchar_t *)malloc((size_t)wlen * sizeof(wchar_t));
    if (!wide) return;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, wlen);

    wchar_t *line = wide;
    while (*line) {
        wchar_t *nl = wcschr(line, L'\n');
        if (nl) *nl = 0;
        wchar_t *cr = wcschr(line, L'\r');
        if (cr) *cr = 0;

        wchar_t *p = line;
        while (*p == L' ' || *p == L'\t') ++p;
        if (*p && *p != L'#') {
            wchar_t *eq = wcschr(p, L'=');
            if (eq) {
                *eq = 0;
                wchar_t *key = p;
                wchar_t *val = eq + 1;

                char keyA[128];
                WideCharToMultiByte(CP_UTF8, 0, key, -1, keyA, (int)sizeof keyA, NULL, NULL);
                for (int i = 0; i < STR_COUNT; ++i) {
                    if (strcmp(keyA, KEY_NAMES[i]) == 0) {
                        wchar_t *dup = (wchar_t *)malloc((wcslen(val) + 1) * sizeof(wchar_t));
                        if (dup) {
                            wcscpy(dup, val);
                            unescape_inplace(dup);
                            free(out[i]);
                            out[i] = dup;
                            have[i] = 1;
                        }
                        break;
                    }
                }
            }
        }

        if (!nl) break;
        line = nl + 1;
    }
    free(wide);
}

void i18n_init(int language)
{
    if (g_pt[0] == NULL && g_en[0] == NULL) {
        /* primeira chamada: parseia os dois arquivos embutidos uma vez so' */
        parse_lang_file((const char *)EMBED_pt_txt, g_pt, g_have[0]);
        parse_lang_file((const char *)EMBED_en_txt, g_en, g_have[1]);
        for (int i = 0; i < STR_COUNT; ++i) {
            if (!g_have[0][i]) log_errorf("i18n: chave ausente em pt.txt: %s", KEY_NAMES[i]);
            if (!g_have[1][i]) log_errorf("i18n: chave ausente em en.txt: %s", KEY_NAMES[i]);
        }
    }

    int lang = language;
    if (lang == 0) {   /* auto */
        lang = (PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_PORTUGUESE) ? 1 : 2;
    }
    g_active_lang = (lang == 2) ? 1 : 0;
}

const wchar_t *i18n_str(StrId id)
{
    if (id < 0 || id >= STR_COUNT) return L"";
    if (g_active_lang == 1 && g_have[1][id]) return g_en[id];
    if (g_have[0][id]) return g_pt[id];
    if (g_have[1][id]) return g_en[id];

    static wchar_t fallback[160];
    swprintf(fallback, 160, L"[%hs]", KEY_NAMES[id]);
    return fallback;
}
