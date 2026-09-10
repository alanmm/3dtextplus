#include "scene.h"
#include "material.h"
#include "gl_core.h"
#include "geometry/font_outline.h"
#include "geometry/contour_mesh.h"
#include "util/mathx.h"
#include "util/log.h"

#include <glad/gl.h>
#include <stdlib.h>
#include <math.h>

/* --- parametros fixos da Fase 2a (viram Config na Fase 2b) --- */
#define SC_TEXT      "Modern 3D Text"
#define SC_FAMILY    L"Segoe UI"
#define SC_BOLD      1
#define SC_ITALIC    0
#define SC_DEPTH     0.30f
#define SC_MAXANGLE  42.0f
#define SC_TILTX     8.0f
#define SC_PERIOD    9.0f
#define SC_FLATTEN   0.004f

struct SceneRenderer {
    Material mat;
    GlMesh   mesh;
    float    hx, hy, hz;      /* meias-extensoes do modelo */
    v3       base_color;
};

SceneRenderer *scene_create(void)
{
    SceneRenderer *s = (SceneRenderer *)calloc(1, sizeof *s);
    if (!s) return NULL;

    if (!material_init(&s->mat)) { free(s); return NULL; }

    ContourSet cs;
    if (!font_build_contours(SC_TEXT, SC_FAMILY, SC_BOLD, SC_ITALIC, SC_FLATTEN, &cs)) {
        material_destroy(&s->mat);
        free(s);
        return NULL;
    }

    MeshData md;
    int ok = contour_mesh_build(&cs, (MeshParams){ SC_DEPTH }, &md);
    contourset_free(&cs);
    if (!ok) {
        log_errorf("scene: contour_mesh_build vazio");
        material_destroy(&s->mat);
        free(s);
        return NULL;
    }

    s->mesh = gl_mesh_upload(md.verts, md.nverts, md.idx, md.nidx);
    s->hx = 0.5f * (md.maxx - md.minx);
    s->hy = 0.5f * (md.maxy - md.miny);
    s->hz = 0.5f * (md.maxz - md.minz);
    if (s->hx < 1e-3f) s->hx = 1.0f;
    if (s->hy < 1e-3f) s->hy = 1.0f;
    mesh_data_free(&md);

    s->base_color = (v3){ 0.72f, 0.74f, 0.78f };

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);   /* extrusao two-sided */
    log_infof("scene: mesh hx=%.2f hy=%.2f hz=%.2f", s->hx, s->hy, s->hz);
    return s;
}

void scene_render(SceneRenderer *s, double t, int fb_w, int fb_h)
{
    if (fb_w < 1) fb_w = 1;
    if (fb_h < 1) fb_h = 1;
    glViewport(0, 0, fb_w, fb_h);
    glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)fb_w / (float)fb_h;
    float fovy = m3dt_radians(35.0f);
    float tanY = tanf(fovy * 0.5f);
    float tanX = tanY * aspect;

    /* enquadra: bbox ocupa ~82% de cada eixo, mais folga para o giro */
    const float fill = 0.82f;
    float swing = m3dt_radians(SC_MAXANGLE);
    float reach = s->hx * cosf(swing) + s->hz * fabsf(sinf(swing));   /* extensao ao girar */
    float distX = reach / (tanX * fill);
    float distY = s->hy / (tanY * fill);
    float dist = fmaxf(distX, distY) + s->hz + 0.5f;

    v3 eye = { 0.0f, 0.0f, dist };
    m4 view = m4_look_at(eye, (v3){ 0, 0, 0 }, (v3){ 0, 1, 0 });
    m4 proj = m4_perspective(fovy, aspect, 0.05f, dist * 3.0f + 20.0f);

    float ay = pendulum_angle((float)t, SC_PERIOD, SC_MAXANGLE);
    float ax = pendulum_angle((float)t + SC_PERIOD * 0.25f, SC_PERIOD, SC_TILTX);
    m4 model = m4_mul(m4_rotate_y(m3dt_radians(ay)), m4_rotate_x(m3dt_radians(ax)));

    material_begin(&s->mat, view, proj, eye, s->base_color);
    material_set_model(&s->mat, model);
    gl_mesh_draw(&s->mesh);
}

void scene_destroy(SceneRenderer *s)
{
    if (!s) return;
    gl_mesh_free(&s->mesh);
    material_destroy(&s->mat);
    free(s);
}
