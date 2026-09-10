#include "scene.h"
#include "material.h"
#include "gl_core.h"
#include "geometry/font_outline.h"
#include "geometry/contour_mesh.h"
#include "util/mathx.h"
#include "util/log.h"

#include <glad/gl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SC_FLATTEN 0.004f

struct SceneRenderer {
    Material mat;
    GlMesh   mesh;
    int      have_mesh;
    float    hx, hy, hz;

    /* snapshot da config corrente */
    char     text[512];
    wchar_t  font_family[64];
    int      bold, italic;
    float    depth;
    float    max_angle_y, tilt_x, period;
    v3       base_color;
};

static int rebuild_mesh(SceneRenderer *s)
{
    ContourSet cs;
    if (!font_build_contours(s->text, s->font_family, s->bold, s->italic, SC_FLATTEN, &cs))
        return 0;

    MeshData md;
    int ok = contour_mesh_build(&cs, (MeshParams){ s->depth }, &md);
    contourset_free(&cs);
    if (!ok) return 0;

    if (s->have_mesh) gl_mesh_free(&s->mesh);
    s->mesh = gl_mesh_upload(md.verts, md.nverts, md.idx, md.nidx);
    s->hx = 0.5f * (md.maxx - md.minx);
    s->hy = 0.5f * (md.maxy - md.miny);
    s->hz = 0.5f * (md.maxz - md.minz);
    if (s->hx < 1e-3f) s->hx = 1.0f;
    if (s->hy < 1e-3f) s->hy = 1.0f;
    mesh_data_free(&md);
    s->have_mesh = 1;
    log_infof("scene: mesh '%s' (%ls%s%s) hx=%.2f hy=%.2f hz=%.2f",
              s->text, s->font_family, s->bold ? " b" : "", s->italic ? " i" : "",
              s->hx, s->hy, s->hz);
    return 1;
}

SceneRenderer *scene_create(const Config *cfg)
{
    SceneRenderer *s = (SceneRenderer *)calloc(1, sizeof *s);
    if (!s) return NULL;
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
        || s->depth != cfg->depth;

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

    if (mesh_dirty) {
        if (!rebuild_mesh(s))
            log_errorf("scene: rebuild_mesh falhou (text='%s' font='%ls')", s->text, s->font_family);
    }
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

    const float fill = 0.60f;
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
    material_set_model(&s->mat, model);
    gl_mesh_draw(&s->mesh);
}

void scene_destroy(SceneRenderer *s)
{
    if (!s) return;
    if (s->have_mesh) gl_mesh_free(&s->mesh);
    material_destroy(&s->mat);
    free(s);
}
