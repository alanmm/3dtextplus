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
#include <windows.h>
#include "stb_image.h"
#include "embedded.h"

#define SC_FLATTEN 0.004f

struct SceneRenderer {
    Material mat;
    GlMesh   mesh;
    int      have_mesh;
    float    hx, hy, hz;
    float    zoom;
    int      auto_spin;
    int      manual_cam;             /* usuario assumiu o controle (arrastar/pan) - desliga auto_spin e pendulo */
    float    man_yaw, man_pitch;     /* graus, acumulado por arrastar com o botao esquerdo */
    float    man_pan_x, man_pan_y;   /* unidades de mundo, acumulado por arrastar com o botao do meio */

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

    /* fundo */
    unsigned bg_prog, bg_vao;
    int      background_type;
    v3       bg_color1, bg_color2;
    float    bg_grad_angle;
    wchar_t  bg_image_path[512];
    unsigned bg_tex;
    int      bg_tex_w, bg_tex_h;
    int      bg_image_fit;
    float    bg_pan_speed;
    v3       bg_neb_color1, bg_neb_color2;
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

static unsigned bg_load_texture(const wchar_t *path, int *out_w, int *out_h)
{
    *out_w = *out_h = 0;
    if (!path || !path[0]) return 0;

    char u8[1024];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, u8, (int)sizeof u8, NULL, NULL);

    int w = 0, h = 0, ch = 0;
    unsigned char *px = stbi_load(u8, &w, &h, &ch, 3);
    if (!px) {
        log_errorf("scene: fundo nao carregou %s (%s)", u8, stbi_failure_reason());
        return 0;
    }

    unsigned t = gl_texture_2d_rgb8(w, h, px, 1);
    stbi_image_free(px);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    *out_w = w; *out_h = h;
    log_infof("scene: fundo %s (%dx%d) -> tex %u", u8, w, h, t);
    return t;
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

    s->bg_prog = gl_program((const char *)EMBED_fullscreen_vert,
                             (const char *)EMBED_background_frag);
    if (!s->bg_prog) {
        material_destroy(&s->mat);
        free(s);
        return NULL;
    }

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

    s->background_type = cfg->background_type;
    s->bg_color1 = (v3){ cfg->bg_color1_r, cfg->bg_color1_g, cfg->bg_color1_b };
    s->bg_color2 = (v3){ cfg->bg_color2_r, cfg->bg_color2_g, cfg->bg_color2_b };
    s->bg_grad_angle = cfg->bg_grad_angle;
    s->bg_image_fit = cfg->bg_image_fit;
    s->bg_pan_speed = cfg->bg_pan_speed;
    s->bg_neb_color1 = (v3){ cfg->bg_neb_color1_r, cfg->bg_neb_color1_g, cfg->bg_neb_color1_b };
    s->bg_neb_color2 = (v3){ cfg->bg_neb_color2_r, cfg->bg_neb_color2_g, cfg->bg_neb_color2_b };

    if (wcscmp(s->bg_image_path, cfg->bg_image_path) != 0) {
        if (s->bg_tex) glDeleteTextures(1, &s->bg_tex);
        wcsncpy(s->bg_image_path, cfg->bg_image_path, 511);
        s->bg_image_path[511] = 0;
        s->bg_tex = bg_load_texture(s->bg_image_path, &s->bg_tex_w, &s->bg_tex_h);
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

void scene_set_auto_spin(SceneRenderer *s, int enabled)
{
    s->auto_spin = enabled;
}

void scene_orbit(SceneRenderer *s, float dyaw_deg, float dpitch_deg)
{
    if (!s->manual_cam) {
        s->manual_cam = 1;
        s->man_yaw = 0.0f;
        s->man_pitch = 12.0f;   /* mesma inclinacao inicial agradavel do auto-spin */
    }
    s->man_yaw = fmodf(s->man_yaw + dyaw_deg, 360.0f);
    s->man_pitch = fminf(85.0f, fmaxf(-85.0f, s->man_pitch + dpitch_deg));
}

void scene_pan(SceneRenderer *s, float dx, float dy)
{
    if (!s->manual_cam) {
        s->manual_cam = 1;
        s->man_yaw = 0.0f;
        s->man_pitch = 12.0f;
    }
    s->man_pan_x += dx;
    s->man_pan_y += dy;
}

void scene_render(SceneRenderer *s, double t, int fb_w, int fb_h)
{
    if (fb_w < 1) fb_w = 1;
    if (fb_h < 1) fb_h = 1;
    glViewport(0, 0, fb_w, fb_h);
    glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDepthMask(GL_FALSE);
    glUseProgram(s->bg_prog);
    glUniform1i(glGetUniformLocation(s->bg_prog, "uType"), s->background_type);
    glUniform3f(glGetUniformLocation(s->bg_prog, "uColor1"), s->bg_color1.x, s->bg_color1.y, s->bg_color1.z);
    glUniform3f(glGetUniformLocation(s->bg_prog, "uColor2"), s->bg_color2.x, s->bg_color2.y, s->bg_color2.z);
    glUniform1f(glGetUniformLocation(s->bg_prog, "uGradAngle"), m3dt_radians(s->bg_grad_angle));
    glUniform3f(glGetUniformLocation(s->bg_prog, "uNebColor1"), s->bg_neb_color1.x, s->bg_neb_color1.y, s->bg_neb_color1.z);
    glUniform3f(glGetUniformLocation(s->bg_prog, "uNebColor2"), s->bg_neb_color2.x, s->bg_neb_color2.y, s->bg_neb_color2.z);
    glUniform1f(glGetUniformLocation(s->bg_prog, "uTime"), (float)t);

    int has_img = (s->background_type == 2 && s->bg_tex) ? 1 : 0;
    glUniform1i(glGetUniformLocation(s->bg_prog, "uHasBgTex"), has_img);
    if (has_img) {
        float scaleX = 1.0f, scaleY = 1.0f, offX = 0.0f, offY = 0.0f;
        float imgAspect = (float)s->bg_tex_w / (float)s->bg_tex_h;
        float viewAspect = (float)fb_w / (float)fb_h;
        if (s->bg_image_fit == 0) {          /* cobrir: recorta o excesso */
            if (viewAspect > imgAspect) { scaleY = imgAspect / viewAspect; offY = (1.0f - scaleY) * 0.5f; }
            else                        { scaleX = viewAspect / imgAspect; offX = (1.0f - scaleX) * 0.5f; }
        } else if (s->bg_image_fit == 1) {   /* conter: faixas na cor 1 */
            if (viewAspect > imgAspect) { scaleX = imgAspect / viewAspect; offX = (1.0f - scaleX) * 0.5f; }
            else                        { scaleY = viewAspect / imgAspect; offY = (1.0f - scaleY) * 0.5f; }
        }
        glUniform2f(glGetUniformLocation(s->bg_prog, "uUvScale"), scaleX, scaleY);
        glUniform2f(glGetUniformLocation(s->bg_prog, "uUvOffset"), offX, offY);
        glUniform1i(glGetUniformLocation(s->bg_prog, "uBgFit"), s->bg_image_fit);
        glUniform1f(glGetUniformLocation(s->bg_prog, "uPanSpeed"), s->bg_pan_speed);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s->bg_tex);
        glUniform1i(glGetUniformLocation(s->bg_prog, "uBgTex"), 0);
    }
    gl_fullscreen_draw(&s->bg_vao);
    glDepthMask(GL_TRUE);

    if (!s->have_mesh) return;

    float aspect = (float)fb_w / (float)fb_h;
    float fovy = m3dt_radians(35.0f);
    float tanY = tanf(fovy * 0.5f);
    float tanX = tanY * aspect;

    const float fill = 0.60f * s->zoom;
    float distX = s->hx / (tanX * fill);
    float distY = s->hy / (tanY * fill);
    float dist = fmaxf(distX, distY) + s->hz + 0.5f;

    /* pan desloca o olho e o alvo juntos no plano XY - a camera nunca gira
       (so o objeto gira via model), entao seus eixos direita/cima sao fixos
       e um deslocamento simples em XY funciona como pan de tela. */
    v3 panv = { s->man_pan_x, s->man_pan_y, 0.0f };
    v3 eye = { panv.x, panv.y, dist };
    m4 view = m4_look_at(eye, panv, (v3){ 0, 1, 0 });
    m4 proj = m4_perspective(fovy, aspect, 0.05f, dist * 3.0f + 20.0f);

    float ay, ax;
    if (s->manual_cam) {
        ay = s->man_yaw;
        ax = s->man_pitch;
    } else if (s->auto_spin) {
        /* preview do dialogo: giro continuo de 360 graus, independente do
           pendulo configurado - deixa ver todos os lados do objeto sem
           precisar que "Angulo max." esteja alto. Inclinacao fixa e suave
           so para dar leitura de profundidade, tambem independente do
           tilt_x configurado. */
        const float SPIN_DEG_PER_SEC = 24.0f;
        ay = fmodf((float)t * SPIN_DEG_PER_SEC, 360.0f);
        ax = 12.0f;
    } else {
        ay = pendulum_angle((float)t, s->period, s->max_angle_y);
        ax = pendulum_angle((float)t + s->period * 0.25f, s->period, s->tilt_x);
    }
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
    if (s->bg_tex) glDeleteTextures(1, &s->bg_tex);
    if (s->bg_prog) glDeleteProgram(s->bg_prog);
    env_free(s->env_tex);
    material_destroy(&s->mat);
    free(s);
}
