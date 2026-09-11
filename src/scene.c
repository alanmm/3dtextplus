#include "scene.h"
#include "material.h"
#include "gl_core.h"
#include "env.h"
#include "geometry/font_outline.h"
#include "geometry/contour_mesh.h"
#include "util/mathx.h"
#include "util/log.h"

#include <glad/gl.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <math.h>

#define SC_FLATTEN 0.004f

struct SceneRenderer {
    Material mat;
    GlMesh   mesh;
    int      have_mesh;
    float    hx, hy, hz;
    float    zoom;

    /* snapshot da config corrente */
    char     text[512];
    wchar_t  font_family[64];
    int      bold, italic;
    float    depth;
    float    max_angle_y, tilt_x, period;
    v3       base_color;
    int      material_mode;
    float    metalness, roughness;
    wchar_t  env_path[512];
    unsigned env_tex;

    int      bevel_mode;
    float    bevel_size, bevel_depth, wall_thickness;
    int      bevel_segments, shell, quality;

    unsigned sdf_tex;
    int      has_sdf;
    float    sdf_min_x, sdf_min_y, sdf_size_x, sdf_size_y;
};

static void upload_sdf(SceneRenderer *s, const Sdf *sdf)
{
    if (s->sdf_tex) { glDeleteTextures(1, &s->sdf_tex); s->sdf_tex = 0; }
    s->has_sdf = 0;
    if (!sdf || sdf->res <= 0) return;

    int n = sdf->res * sdf->res;
    float *rgba = (float *)malloc((size_t)n * 4 * sizeof(float));
    if (!rgba) return;
    for (int i = 0; i < n; ++i) {
        rgba[i * 4 + 0] = sdf->dist[i];
        rgba[i * 4 + 1] = sdf->gx[i];
        rgba[i * 4 + 2] = sdf->gy[i];
        rgba[i * 4 + 3] = 0.0f;
    }
    s->sdf_tex = gl_texture_2d_rgba32f(sdf->res, sdf->res, rgba);
    free(rgba);
    s->has_sdf = 1;
    s->sdf_min_x = sdf->min_x;   s->sdf_min_y = sdf->min_y;
    s->sdf_size_x = sdf->size_x; s->sdf_size_y = sdf->size_y;
}

static MeshParams scene_mesh_params(const SceneRenderer *s)
{
    MeshParams p;
    memset(&p, 0, sizeof p);
    p.depth          = s->depth;
    p.bevel_mode     = s->bevel_mode;
    p.bevel_size     = s->bevel_size;
    p.bevel_depth    = s->bevel_depth;
    p.bevel_segments = s->bevel_segments;
    p.shell          = s->shell;
    p.wall_thickness = s->wall_thickness;
    p.quality        = s->quality;
    return p;
}

static int rebuild_mesh(SceneRenderer *s)
{
    float tol = s->quality <= 0 ? 0.010f : (s->quality == 1 ? 0.004f : 0.0018f);

    ContourSet cs;
    if (!font_build_contours(s->text, s->font_family, s->bold, s->italic, tol, &cs))
        return 0;

    MeshData md;
    int ok = contour_mesh_build(&cs, scene_mesh_params(s), &md);
    contourset_free(&cs);
    if (!ok) return 0;

    if (s->have_mesh) gl_mesh_free(&s->mesh);
    s->mesh = gl_mesh_upload(md.verts, md.nverts, md.idx, md.nidx);
    s->hx = 0.5f * (md.maxx - md.minx);
    s->hy = 0.5f * (md.maxy - md.miny);
    s->hz = 0.5f * (md.maxz - md.minz);
    if (s->hx < 1e-3f) s->hx = 1.0f;
    if (s->hy < 1e-3f) s->hy = 1.0f;

    upload_sdf(s, md.has_sdf ? &md.sdf : NULL);
    mesh_data_free(&md);
    s->have_mesh = 1;
    log_infof("scene: mesh '%s' (%ls%s%s) hx=%.2f hy=%.2f hz=%.2f bevel=%d sdf=%d",
              s->text, s->font_family, s->bold ? " b" : "", s->italic ? " i" : "",
              s->hx, s->hy, s->hz, s->bevel_mode, s->has_sdf);
    return 1;
}

SceneRenderer *scene_create(const Config *cfg)
{
    SceneRenderer *s = (SceneRenderer *)calloc(1, sizeof *s);
    if (!s) return NULL;
    s->zoom = 1.0f;
    if (!material_init(&s->mat)) { free(s); return NULL; }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);   /* extrusao two-sided */

    scene_set_config(s, cfg);
    if (!s->have_mesh) {
        log_errorf("scene: sem malha inicial");
        material_destroy(&s->mat);
        free(s);
        return NULL;
    }
    return s;
}

void scene_set_config(SceneRenderer *s, const Config *cfg)
{
    int mesh_dirty = !s->have_mesh
        || strcmp(s->text, cfg->text) != 0
        || wcscmp(s->font_family, cfg->font_family) != 0
        || s->bold != cfg->font_bold
        || s->italic != cfg->font_italic
        || s->depth != cfg->depth
        || s->bevel_mode != cfg->bevel_mode
        || s->bevel_size != cfg->bevel_size
        || s->bevel_depth != cfg->bevel_depth
        || s->bevel_segments != cfg->bevel_segments
        || s->shell != cfg->shell
        || s->wall_thickness != cfg->wall_thickness
        || s->quality != cfg->quality;

    strncpy(s->text, cfg->text, sizeof s->text - 1);
    s->text[sizeof s->text - 1] = 0;
    wcsncpy(s->font_family, cfg->font_family, 63);
    s->font_family[63] = 0;
    s->bold = cfg->font_bold;
    s->italic = cfg->font_italic;
    s->depth = cfg->depth;
    s->max_angle_y = cfg->max_angle_y;
    s->tilt_x = cfg->tilt_x;
    s->period = cfg->period;
    s->base_color = (v3){ cfg->base_r, cfg->base_g, cfg->base_b };
    s->material_mode = cfg->material_mode;
    s->metalness = cfg->metalness;
    s->roughness = cfg->roughness;
    s->bevel_mode = cfg->bevel_mode;
    s->bevel_size = cfg->bevel_size;
    s->bevel_depth = cfg->bevel_depth;
    s->bevel_segments = cfg->bevel_segments;
    s->shell = cfg->shell;
    s->wall_thickness = cfg->wall_thickness;
    s->quality = cfg->quality;

    if (wcscmp(s->env_path, cfg->env_path) != 0) {
        env_free(s->env_tex);
        wcsncpy(s->env_path, cfg->env_path, 511);
        s->env_path[511] = 0;
        s->env_tex = env_load_texture(s->env_path);
    }

    if (mesh_dirty) {
        if (!rebuild_mesh(s))
            log_errorf("scene: rebuild_mesh falhou (text='%s' font='%ls')", s->text, s->font_family);
    }
}

void scene_set_zoom(SceneRenderer *s, float zoom)
{
    s->zoom = zoom;
}

void scene_render(SceneRenderer *s, double t, int fb_w, int fb_h)
{
    if (fb_w < 1) fb_w = 1;
    if (fb_h < 1) fb_h = 1;
    glViewport(0, 0, fb_w, fb_h);
    glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!s->have_mesh) return;

    float aspect = (float)fb_w / (float)fb_h;
    float fovy = m3dt_radians(35.0f);
    float tanY = tanf(fovy * 0.5f);
    float tanX = tanY * aspect;

    const float fill = 0.60f * s->zoom;
    float distX = s->hx / (tanX * fill);
    float distY = s->hy / (tanY * fill);
    float dist = fmaxf(distX, distY) + s->hz + 0.5f;

    v3 eye = { 0.0f, 0.0f, dist };
    m4 view = m4_look_at(eye, (v3){ 0, 0, 0 }, (v3){ 0, 1, 0 });
    m4 proj = m4_perspective(fovy, aspect, 0.05f, dist * 3.0f + 20.0f);

    float ay = pendulum_angle((float)t, s->period, s->max_angle_y);
    float ax = pendulum_angle((float)t + s->period * 0.25f, s->period, s->tilt_x);
    m4 model = m4_mul(m4_rotate_y(m3dt_radians(ay)), m4_rotate_x(m3dt_radians(ax)));

    material_begin(&s->mat, view, proj, eye, s->base_color);
    material_set_style(&s->mat, s->material_mode, s->metalness, s->roughness, s->env_tex);
    material_set_bevel(&s->mat, s->bevel_mode, s->bevel_size, s->hz,
                       (v2){ s->sdf_min_x, s->sdf_min_y },
                       (v2){ s->sdf_size_x, s->sdf_size_y }, s->has_sdf ? s->sdf_tex : 0);
    material_set_model(&s->mat, model);

    int glass = (s->material_mode == 2);
    if (glass) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }
    gl_mesh_draw(&s->mesh);
    if (glass) {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

void scene_destroy(SceneRenderer *s)
{
    if (!s) return;
    if (s->have_mesh) gl_mesh_free(&s->mesh);
    if (s->sdf_tex) glDeleteTextures(1, &s->sdf_tex);
    env_free(s->env_tex);
    material_destroy(&s->mat);
    free(s);
}
