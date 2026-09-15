#include "scene.h"
#include "material.h"
#include "gl_core.h"
#include "env.h"
#include "particles.h"
#include "util/clockfmt.h"
#include "geometry/font_outline.h"
#include "geometry/contour_mesh.h"
#include "geometry/svg_shapes.h"
#include "geometry/mesh_import.h"
#include "i18n.h"
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
    int      debug_view;   /* 0=normal - ver scene_set_debug_view */
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
    v3       emissive_color;
    float    emissive_amount;
    float    edge_bias;
    wchar_t  env_path[512];
    unsigned env_tex;
    int      env_mode;
    int      env_loaded;   /* forca 1a carga mesmo quando env_mode==0 bate com o calloc inicial */

    /* WBOIT (Vidro) - FBO com 2 anexos de cor, criada preguiçosamente */
    unsigned wboit_fbo;
    unsigned wboit_accum_tex, wboit_reveal_tex;
    int      wboit_w, wboit_h;
    unsigned wboit_resolve_prog;
    unsigned wboit_vao;

    int      bevel_mode;
    float    bevel_size, bevel_depth, wall_thickness;
    int      bevel_segments, shell, quality;

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

    /* particulas */
    ParticleSystem *particles;
    v3     wall_pos_cache[512];
    v3     wall_n_cache[512];
    int    wall_cache_count;
    double last_t;
    int    have_last_t;

    int content_mode;
    int clock_show_date, clock_show_seconds;

    wchar_t  svg_path[512];
    int      svg_color_mode;
    GlMesh  *svg_meshes;
    v3      *svg_colors;
    int      svg_mesh_count;
    int      have_svg_mesh;

    wchar_t  mesh_path[512];
    float    mesh_size_scale;
    int      mesh_use_file_materials;
    GlMesh  *mesh_pieces;
    v3      *mesh_piece_colors;
    int      mesh_piece_count;
    int      have_mesh_content;   /* "ja tentamos montar a malha atual pelo menos uma vez" -
                                      cobre tanto o caminho unico quanto o de pecas */

    /* placa de erro (SVG/malha ausente ou invalida) - substitui o
       conteudo 3D normal por uma caixa+texto achatados, nao pertence
       aos campos svg_ ou mesh_ porque pode ser disparada por qualquer
       um dos dois */
    GlMesh   error_pieces[2];      /* [0] caixa, [1] texto */
    v3       error_colors[2];
    int      have_error_plaque;
};

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

static void free_svg_pieces(SceneRenderer *s)
{
    for (int i = 0; i < s->svg_mesh_count; ++i) gl_mesh_free(&s->svg_meshes[i]);
    free(s->svg_meshes); s->svg_meshes = NULL;
    free(s->svg_colors); s->svg_colors = NULL;
    s->svg_mesh_count = 0;
}

static void free_error_plaque(SceneRenderer *s)
{
    if (s->have_error_plaque) {
        gl_mesh_free(&s->error_pieces[0]);
        gl_mesh_free(&s->error_pieces[1]);
    }
    s->have_error_plaque = 0;
}

/* quad achatado simples (2 triangulos, sem indice compartilhado),
   centrado na origem, normal +Z (voltada pra camera). */
static void build_flat_quad(float halfw, float halfh, float z, MeshData *out)
{
    memset(out, 0, sizeof *out);
    MeshVertex *v = (MeshVertex *)malloc(6 * sizeof(MeshVertex));
    unsigned *idx = (unsigned *)malloc(6 * sizeof(unsigned));
    v3 p[4] = {
        { -halfw, -halfh, z }, { halfw, -halfh, z },
        { halfw,  halfh, z }, { -halfw,  halfh, z }
    };
    int tri[6] = { 0, 1, 2, 0, 2, 3 };
    for (int i = 0; i < 6; ++i) {
        v3 pp = p[tri[i]];
        v[i] = (MeshVertex){ pp.x, pp.y, pp.z, 0.0f, 0.0f, 1.0f, 0.0f };
        idx[i] = (unsigned)i;
    }
    out->verts = v; out->nverts = 6;
    out->idx = idx; out->nidx = 6;
    out->minx = -halfw; out->maxx = halfw;
    out->miny = -halfh; out->maxy = halfh;
    out->minz = z; out->maxz = z;
}

/* placa achatada (caixa cinza + texto preto) usada como fallback visivel
   quando SVG/malha configurado esta ausente ou invalido - reaproveita o
   mesmo pipeline de texto de sempre (font_build_contours+contour_mesh_build)
   com depth=0 e bevel desligado (mesmo efeito "achatado" que ja aparecia
   nos testes do SVG), fonte fixada no fallback do sistema (family=NULL) em
   vez da fonte configurada pelo usuario, quebras de linha fixas na propria
   'message'. Enquadramento de camera automatico via s->hx/hy/hz, igual a
   qualquer outro conteudo. */
static int build_error_plaque(SceneRenderer *s, const char *message)
{
    free_error_plaque(s);

    float tol = s->quality <= 0 ? 0.010f : (s->quality == 1 ? 0.004f : 0.0018f);

    ContourSet cs;
    if (!font_build_contours(message, NULL, 0, 0, tol, &cs)) return 0;

    MeshParams p;
    memset(&p, 0, sizeof p);
    p.bevel_mode = 2;   /* desligado */
    p.quality = s->quality;

    MeshData text_md;
    int ok = contour_mesh_build(&cs, p, &text_md);
    contourset_free(&cs);
    if (!ok) return 0;

    float text_hw = 0.5f * (text_md.maxx - text_md.minx);
    float text_h  = text_md.maxy - text_md.miny;
    float text_hh = 0.5f * text_h;
    const float MARGIN_FRAC = 0.25f;   /* fracao da altura do texto, em cada lado */
    float margin = MARGIN_FRAC * text_h;
    float box_hw = text_hw + margin;
    float box_hh = text_hh + margin;

    MeshData box_md;
    build_flat_quad(box_hw, box_hh, -0.02f, &box_md);

    s->error_pieces[0] = gl_mesh_upload(box_md.verts, box_md.nverts, box_md.idx, box_md.nidx);
    s->error_colors[0] = (v3){ 0.85f, 0.85f, 0.85f };
    mesh_data_free(&box_md);

    s->error_pieces[1] = gl_mesh_upload(text_md.verts, text_md.nverts, text_md.idx, text_md.nidx);
    s->error_colors[1] = (v3){ 0.0f, 0.0f, 0.0f };
    mesh_data_free(&text_md);

    s->have_error_plaque = 1;
    s->hx = box_hw; s->hy = box_hh; s->hz = 0.02f;
    s->wall_cache_count = 0;
    log_infof("scene: placa de erro '%s'", message);
    return 1;
}

static int rebuild_mesh(SceneRenderer *s)
{
    free_error_plaque(s);

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

    {
        int wc = 0;
        for (int i = 0; i < md.nverts && wc < 512; ++i) {
            if (md.verts[i].surf == 2.0f) {
                s->wall_pos_cache[wc] = (v3){ md.verts[i].px, md.verts[i].py, md.verts[i].pz };
                s->wall_n_cache[wc]   = (v3){ md.verts[i].nx, md.verts[i].ny, md.verts[i].nz };
                ++wc;
            }
        }
        s->wall_cache_count = wc;
    }

    mesh_data_free(&md);
    s->have_mesh = 1;
    log_infof("scene: mesh '%s' (%ls%s%s) hx=%.2f hy=%.2f hz=%.2f bevel=%d",
              s->text, s->font_family, s->bold ? " b" : "", s->italic ? " i" : "",
              s->hx, s->hy, s->hz, s->bevel_mode);
    return 1;
}

static int rebuild_svg_mesh(SceneRenderer *s)
{
    free_svg_pieces(s);
    free_error_plaque(s);

    float tol = s->quality <= 0 ? 0.010f : (s->quality == 1 ? 0.004f : 0.0018f);

    SvgShapeSet svgset;
    int loaded = s->svg_path[0] != 0 && svg_shapes_load(s->svg_path, tol, &svgset);
    if (!loaded || svgset.count == 0) {
        if (loaded) svg_shapes_free(&svgset);
        s->have_svg_mesh = 1;
        char msg[256];
        WideCharToMultiByte(CP_UTF8, 0, i18n_str(STR_ERROR_SVG_NO_SHAPE), -1,
                            msg, (int)sizeof msg, NULL, NULL);
        return build_error_plaque(s, msg);
    }

    s->svg_meshes = (GlMesh *)calloc((size_t)svgset.count, sizeof(GlMesh));
    s->svg_colors = (v3 *)calloc((size_t)svgset.count, sizeof(v3));
    if (!s->svg_meshes || !s->svg_colors) {
        svg_shapes_free(&svgset);
        free_svg_pieces(s);
        return 0;
    }

    float uhx = 0.0f, uhy = 0.0f, uhz = 0.0f;
    s->wall_cache_count = 0;
    for (int i = 0; i < svgset.count; ++i) {
        MeshData md;
        if (!contour_mesh_build(&svgset.pieces[i].cs, scene_mesh_params(s), &md)) continue;

        s->svg_meshes[s->svg_mesh_count] = gl_mesh_upload(md.verts, md.nverts, md.idx, md.nidx);
        s->svg_colors[s->svg_mesh_count] =
            (v3){ svgset.pieces[i].r, svgset.pieces[i].g, svgset.pieces[i].b };

        float phx = 0.5f * (md.maxx - md.minx);
        float phy = 0.5f * (md.maxy - md.miny);
        float phz = 0.5f * (md.maxz - md.minz);
        if (phx > uhx) uhx = phx;
        if (phy > uhy) uhy = phy;
        if (phz > uhz) uhz = phz;

        for (int vi = 0; vi < md.nverts && s->wall_cache_count < 512; ++vi) {
            if (md.verts[vi].surf == 2.0f) {
                s->wall_pos_cache[s->wall_cache_count] =
                    (v3){ md.verts[vi].px, md.verts[vi].py, md.verts[vi].pz };
                s->wall_n_cache[s->wall_cache_count] =
                    (v3){ md.verts[vi].nx, md.verts[vi].ny, md.verts[vi].nz };
                s->wall_cache_count++;
            }
        }

        s->svg_mesh_count++;
        mesh_data_free(&md);
    }
    svg_shapes_free(&svgset);

    if (s->svg_mesh_count == 0) {
        free_svg_pieces(s);
        s->have_svg_mesh = 1;
        char msg[256];
        WideCharToMultiByte(CP_UTF8, 0, i18n_str(STR_ERROR_SVG_EMPTY), -1,
                            msg, (int)sizeof msg, NULL, NULL);
        return build_error_plaque(s, msg);
    }

    if (uhx < 1e-3f) uhx = 1.0f;
    if (uhy < 1e-3f) uhy = 1.0f;
    s->hx = uhx; s->hy = uhy; s->hz = uhz;
    s->have_svg_mesh = 1;
    log_infof("scene: svg '%ls' -> %d peca(s)", s->svg_path, s->svg_mesh_count);
    return 1;
}

static void free_mesh_pieces(SceneRenderer *s)
{
    for (int i = 0; i < s->mesh_piece_count; ++i) gl_mesh_free(&s->mesh_pieces[i]);
    free(s->mesh_pieces); s->mesh_pieces = NULL;
    free(s->mesh_piece_colors); s->mesh_piece_colors = NULL;
    s->mesh_piece_count = 0;
}

static int rebuild_imported_mesh(SceneRenderer *s)
{
    free_mesh_pieces(s);
    free_error_plaque(s);
    s->have_mesh_content = 1;

    if (s->mesh_path[0] != 0 && s->mesh_use_file_materials) {
        MeshPieceSet ps;
        if (mesh_import_load_pieces(s->mesh_path, s->mesh_size_scale, &ps)) {
            s->mesh_pieces = (GlMesh *)calloc((size_t)ps.count, sizeof(GlMesh));
            s->mesh_piece_colors = (v3 *)calloc((size_t)ps.count, sizeof(v3));
            if (s->mesh_pieces && s->mesh_piece_colors) {
                float uhx = 0.0f, uhy = 0.0f, uhz = 0.0f;
                for (int i = 0; i < ps.count; ++i) {
                    MeshData *d = &ps.pieces[i].data;
                    s->mesh_pieces[i] = gl_mesh_upload(d->verts, d->nverts, d->idx, d->nidx);
                    s->mesh_piece_colors[i] = (v3){ ps.pieces[i].r, ps.pieces[i].g, ps.pieces[i].b };
                    float phx = 0.5f * (d->maxx - d->minx);
                    float phy = 0.5f * (d->maxy - d->miny);
                    float phz = 0.5f * (d->maxz - d->minz);
                    if (phx > uhx) uhx = phx;
                    if (phy > uhy) uhy = phy;
                    if (phz > uhz) uhz = phz;
                }
                s->mesh_piece_count = ps.count;
                if (uhx < 1e-3f) uhx = 1.0f;
                if (uhy < 1e-3f) uhy = 1.0f;
                s->hx = uhx; s->hy = uhy; s->hz = uhz;
                s->wall_cache_count = 0;
                log_infof("scene: malha '%ls' -> %d peca(s) com material do arquivo",
                          s->mesh_path, s->mesh_piece_count);
                mesh_import_pieces_free(&ps);
                return 1;
            }
            free_mesh_pieces(s);
            mesh_import_pieces_free(&ps);
            return 0;
        }
    }

    MeshData md;
    int ok = s->mesh_path[0] != 0 && mesh_import_load(s->mesh_path, s->mesh_size_scale, &md);
    if (!ok) {
        char msg[256];
        WideCharToMultiByte(CP_UTF8, 0, i18n_str(STR_ERROR_MESH_INVALID), -1,
                            msg, (int)sizeof msg, NULL, NULL);
        return build_error_plaque(s, msg);
    }

    if (s->have_mesh) gl_mesh_free(&s->mesh);
    s->mesh = gl_mesh_upload(md.verts, md.nverts, md.idx, md.nidx);
    s->hx = 0.5f * (md.maxx - md.minx);
    s->hy = 0.5f * (md.maxy - md.miny);
    s->hz = 0.5f * (md.maxz - md.minz);
    if (s->hx < 1e-3f) s->hx = 1.0f;
    if (s->hy < 1e-3f) s->hy = 1.0f;
    s->wall_cache_count = 0;   /* malha importada nao tem paredes - faiscas nao emitem nela */
    int nv = md.nverts;
    mesh_data_free(&md);
    s->have_mesh = 1;
    log_infof("scene: malha '%ls' -> %d verts", s->mesh_path, nv);
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

    s->wboit_resolve_prog = gl_program((const char *)EMBED_fullscreen_vert,
                                        (const char *)EMBED_wboit_resolve_frag);
    if (!s->wboit_resolve_prog) {
        glDeleteProgram(s->bg_prog);
        material_destroy(&s->mat);
        free(s);
        return NULL;
    }
    glUseProgram(s->wboit_resolve_prog);
    glUniform1i(glGetUniformLocation(s->wboit_resolve_prog, "uAccum"), 0);
    glUniform1i(glGetUniformLocation(s->wboit_resolve_prog, "uRevealLog"), 1);

    s->particles = particles_create();
    if (!s->particles) {
        glDeleteProgram(s->bg_prog);
        material_destroy(&s->mat);
        free(s);
        return NULL;
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);   /* extrusao two-sided */

    scene_set_config(s, cfg);
    if (!s->have_mesh && s->svg_mesh_count == 0 && s->mesh_piece_count == 0
        && !s->have_error_plaque) {
        log_errorf("scene: sem malha inicial");
        material_destroy(&s->mat);
        free(s);
        return NULL;
    }
    return s;
}

void scene_set_config(SceneRenderer *s, const Config *cfg)
{
    int svg_relevant_change = (cfg->content_mode == CONTENT_SVG)
        && (wcscmp(s->svg_path, cfg->svg_path) != 0 || s->svg_color_mode != cfg->svg_color_mode);

    int mesh_relevant_change = (cfg->content_mode == CONTENT_MESH)
        && (wcscmp(s->mesh_path, cfg->mesh_path) != 0
            || s->mesh_size_scale != cfg->mesh_size_scale
            || s->mesh_use_file_materials != cfg->mesh_use_file_materials);

    int mesh_dirty = (cfg->content_mode == CONTENT_SVG ? !s->have_svg_mesh
                      : cfg->content_mode == CONTENT_MESH ? !s->have_mesh_content
                      : !s->have_mesh)
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
        || s->quality != cfg->quality
        || s->content_mode != (int)cfg->content_mode
        || svg_relevant_change
        || mesh_relevant_change;

    s->content_mode = cfg->content_mode;
    s->clock_show_date = cfg->clock_show_date;
    s->clock_show_seconds = cfg->clock_show_seconds;
    wcsncpy(s->svg_path, cfg->svg_path, 511);
    s->svg_path[511] = 0;
    s->svg_color_mode = cfg->svg_color_mode;
    wcsncpy(s->mesh_path, cfg->mesh_path, 511);
    s->mesh_path[511] = 0;
    s->mesh_size_scale = cfg->mesh_size_scale;
    s->mesh_use_file_materials = cfg->mesh_use_file_materials;

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
    s->emissive_color = (v3){ cfg->emissive_r, cfg->emissive_g, cfg->emissive_b };
    s->emissive_amount = cfg->emissive_amount;
    s->edge_bias = cfg->edge_bias;
    s->bevel_mode = cfg->bevel_mode;
    s->bevel_size = cfg->bevel_size;
    s->bevel_depth = cfg->bevel_depth;
    s->bevel_segments = cfg->bevel_segments;
    s->shell = cfg->shell;
    s->wall_thickness = cfg->wall_thickness;
    s->quality = cfg->quality;

    if (!s->env_loaded || s->env_mode != cfg->env_mode ||
        (cfg->env_mode == 1 && wcscmp(s->env_path, cfg->env_path) != 0)) {
        env_free(s->env_tex);
        s->env_mode = cfg->env_mode;
        wcsncpy(s->env_path, cfg->env_path, 511);
        s->env_path[511] = 0;
        s->env_loaded = 1;
        switch (cfg->env_mode) {
            case 0:  s->env_tex = env_load_texture_from_memory(EMBED_env_default_jpg, EMBED_env_default_jpg_len); break;
            case 1:  s->env_tex = env_load_texture(s->env_path); break;
            default: s->env_tex = 0; break;   /* 2 = nenhuma -> ambiente procedural */
        }
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
        int ok;
        if (s->content_mode == CONTENT_SVG) ok = rebuild_svg_mesh(s);
        else if (s->content_mode == CONTENT_MESH) ok = rebuild_imported_mesh(s);
        else ok = rebuild_mesh(s);
        if (!ok)
            log_errorf("scene: rebuild falhou (content_mode=%d)", s->content_mode);
    }

    particles_set_config(s->particles, cfg, s->hx, s->hy, s->hz,
                         s->wall_pos_cache, s->wall_n_cache, s->wall_cache_count);
}

void scene_set_zoom(SceneRenderer *s, float zoom)
{
    s->zoom = zoom;
}

void scene_set_debug_view(SceneRenderer *s, int mode)
{
    s->debug_view = mode;
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

/* cria (ou recria, se o tamanho mudou) a FBO de acumulacao WBOIT do
   Vidro - 2 anexos de cor (RGBA16F acumulacao, R16F log de revelacao),
   sem depth (a mistura aditiva de ambos nao depende de teste de
   profundidade - ver spec secao 4, passo 1). */
static void ensure_wboit_targets(SceneRenderer *s, int w, int h)
{
    if (s->wboit_fbo && s->wboit_w == w && s->wboit_h == h) return;
    if (s->wboit_fbo) {
        glDeleteFramebuffers(1, &s->wboit_fbo);
        glDeleteTextures(1, &s->wboit_accum_tex);
        glDeleteTextures(1, &s->wboit_reveal_tex);
    }
    s->wboit_w = w; s->wboit_h = h;

    glGenTextures(1, &s->wboit_accum_tex);
    glBindTexture(GL_TEXTURE_2D, s->wboit_accum_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &s->wboit_reveal_tex);
    glBindTexture(GL_TEXTURE_2D, s->wboit_reveal_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, w, h, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &s->wboit_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s->wboit_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s->wboit_accum_tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, s->wboit_reveal_tex, 0);
    /* model.frag declara oAccum na location 1 e oRevealLog na location 2
       (location 0 e' o fragColor de Classico/Metalico, nao usado aqui) -
       glDrawBuffers mapeia POR INDICE: buffer[N] recebe o que o shader
       escrever na location N. Precisa de 3 entradas (indices 0,1,2) pra
       que location 1 caia no anexo 0 (accum) e location 2 no anexo 1
       (revealage) - so' 2 entradas deslocaria tudo uma location errada. */
    GLenum bufs[3] = { GL_NONE, GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(3, bufs);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        log_errorf("scene: FBO WBOIT incompleta");
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/* desenha o conteudo 3D atual (placa de erro, SVG, malha importada ou
   o texto/relogio normal) com o material ja configurado via
   material_begin/material_set_style/material_set_model - reaproveitado
   tanto pelo caminho normal (opaco) quanto pela passada de acumulacao
   do WBOIT (vidro). */
static void draw_content(SceneRenderer *s)
{
    if (s->have_error_plaque) {
        material_set_piece_color(&s->mat, s->error_colors[0]);
        gl_mesh_draw(&s->error_pieces[0]);
        material_set_piece_color(&s->mat, s->error_colors[1]);
        gl_mesh_draw(&s->error_pieces[1]);
    } else if (s->content_mode == CONTENT_SVG && s->svg_mesh_count > 0) {
        for (int i = 0; i < s->svg_mesh_count; ++i) {
            v3 piece_color = (s->svg_color_mode == 0) ? s->svg_colors[i] : s->base_color;
            material_set_piece_color(&s->mat, piece_color);
            gl_mesh_draw(&s->svg_meshes[i]);
        }
    } else if (s->content_mode == CONTENT_MESH && s->mesh_piece_count > 0) {
        for (int i = 0; i < s->mesh_piece_count; ++i) {
            material_set_piece_color(&s->mat, s->mesh_piece_colors[i]);
            gl_mesh_draw(&s->mesh_pieces[i]);
        }
    } else {
        gl_mesh_draw(&s->mesh);
    }
}

void scene_render(SceneRenderer *s, double t, int fb_w, int fb_h, int particles_active)
{
    if (fb_w < 1) fb_w = 1;
    if (fb_h < 1) fb_h = 1;

    float dt = 0.0f;
    if (s->have_last_t) {
        dt = (float)(t - s->last_t);
        if (dt < 0.0f) dt = 0.0f;
        if (dt > 0.1f) dt = 0.1f;
    }
    s->last_t = t;
    s->have_last_t = 1;

    if (s->content_mode == CONTENT_CLOCK) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char buf[512];
        clock_format(st, s->clock_show_date, s->clock_show_seconds, buf, sizeof buf);
        if (strcmp(buf, s->text) != 0) {
            strncpy(s->text, buf, sizeof s->text - 1);
            s->text[sizeof s->text - 1] = 0;
            if (!rebuild_mesh(s))
                log_errorf("scene: rebuild_mesh (relogio) falhou (text='%s')", s->text);
        }
    }

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

    if (!s->have_mesh && s->svg_mesh_count == 0 && s->mesh_piece_count == 0
        && !s->have_error_plaque) return;

    float aspect = (float)fb_w / (float)fb_h;
    float fovy = m3dt_radians(35.0f);
    float tanY = tanf(fovy * 0.5f);
    float tanX = tanY * aspect;

    const float fill = (s->have_error_plaque ? 0.85f : 0.60f) * s->zoom;
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
    material_set_style(&s->mat, s->material_mode, s->metalness, s->roughness, s->env_tex,
                        s->emissive_color, s->emissive_amount, s->edge_bias);
    material_set_debug_view(&s->mat, s->debug_view);
    material_set_model(&s->mat, model);

    if (s->material_mode == 2 && s->debug_view == 0) {     /* vidro: WBOIT */
        /* a visualizacao de debug escreve em fragColor (location 0), que
           a FBO do WBOIT nem tem ligado (so' os 2 anexos de acumulacao) -
           o resultado ficava invisivel. Com qualquer debug_view ativo,
           desenha direto (mesmo caminho de Classico/Metalico) pra sempre
           ser visivel, sem se preocupar com composicao de transparencia -
           o objetivo aqui e' inspecionar o dado cru, nao a mistura. */
        ensure_wboit_targets(s, fb_w, fb_h);
        GLint prev_fbo = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_fbo);

        /* passo 1: acumula cor*alpha*peso e log(1-alpha) - mistura
           aditiva simples vale pros 2 anexos ao mesmo tempo (ver spec
           secao 2 - evita depender de mistura por-anexo, GL 4.0). */
        glBindFramebuffer(GL_FRAMEBUFFER, s->wboit_fbo);
        glViewport(0, 0, fb_w, fb_h);
        float clearAccum[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float clearReveal[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        /* o indice aqui e' o INDICE DO DRAW BUFFER (posicao no array
           passado a glDrawBuffers), nao o numero do anexo - com
           bufs={GL_NONE, ATTACHMENT0, ATTACHMENT1}, indice 0 e' NONE
           (nada), indice 1 e' o accum (ATTACHMENT0) e indice 2 e' o
           revealage (ATTACHMENT1). Usar 0/1 aqui deixava o revealage
           SEM limpar entre frames - ele so' acumula (mistura aditiva),
           entao ficava mais negativo a cada frame sem nunca reiniciar,
           deixando o vidro cada vez mais opaco e criando um rastro
           escurecido ao longo do caminho que o objeto girava. */
        glClearBufferfv(GL_COLOR, 1, clearAccum);
        glClearBufferfv(GL_COLOR, 2, clearReveal);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        draw_content(s);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        /* passo 2: resolve - volta pro alvo original e compoe a cor
           final (accum/revealage desfeitos no shader) por cima do que
           ja foi desenhado (fundo). Depth test desligado por seguranca -
           o triangulo de tela cheia nao deveria depender de profundidade
           nenhuma pra aparecer. */
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
        glViewport(0, 0, fb_w, fb_h);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(s->wboit_resolve_prog);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s->wboit_accum_tex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, s->wboit_reveal_tex);
        gl_fullscreen_draw(&s->wboit_vao);
        glActiveTexture(GL_TEXTURE0);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    } else {
        draw_content(s);
    }

    if (particles_active) {
        particles_update(s->particles, dt, model);
        particles_render(s->particles, view, proj, fb_h);
    }
}

void scene_destroy(SceneRenderer *s)
{
    if (!s) return;
    if (s->have_mesh) gl_mesh_free(&s->mesh);
    free_svg_pieces(s);
    free_mesh_pieces(s);
    free_error_plaque(s);
    particles_destroy(s->particles);
    if (s->bg_tex) glDeleteTextures(1, &s->bg_tex);
    if (s->bg_prog) glDeleteProgram(s->bg_prog);
    env_free(s->env_tex);
    if (s->wboit_fbo) {
        glDeleteFramebuffers(1, &s->wboit_fbo);
        glDeleteTextures(1, &s->wboit_accum_tex);
        glDeleteTextures(1, &s->wboit_reveal_tex);
    }
    if (s->wboit_vao) glDeleteVertexArrays(1, &s->wboit_vao);
    if (s->wboit_resolve_prog) glDeleteProgram(s->wboit_resolve_prog);
    material_destroy(&s->mat);
    free(s);
}
