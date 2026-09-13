#ifndef M3DT_CONFIG_H
#define M3DT_CONFIG_H
#include <wchar.h>

typedef enum { CONTENT_TEXT = 0, CONTENT_CLOCK = 1, CONTENT_SVG = 2, CONTENT_MESH = 3 } ContentMode;

typedef struct {
    int          version;              /* schema; atual = 1 */
    ContentMode  content_mode;
    char         text[512];            /* UTF-8 */
    wchar_t      font_family[64];
    int          font_bold;            /* 0/1 */
    int          font_italic;          /* 0/1 */
    float        depth;                /* 0.02 .. 2.0 */
    float        max_angle_y;          /* 5 .. 170 (graus) */
    float        tilt_x;               /* 0 .. 30 (graus) */
    float        period;               /* 2 .. 30 (s) */
    float        base_r, base_g, base_b;  /* 0..1 */
    int          material_mode;        /* 0 classico, 1 metalico, 2 vidro, 3 fosco */
    float        metalness;            /* 0..1 */
    float        roughness;            /* 0..1 */
    wchar_t      env_path[512];        /* vazio = ambiente procedural */
    int          bevel_mode;           /* 0 sombreado, 1 geometrico, 2 desligado */
    float        bevel_size;           /* 0 .. 0.2 (em) */
    float        bevel_depth;          /* 0 .. 0.2 (em) */
    int          bevel_segments;       /* 2 .. 8 (so no modo geometrico) */
    int          shell;                /* 0/1 */
    float        wall_thickness;       /* 0.01 .. 0.2 (em) */
    int          quality;              /* 0 baixa, 1 media, 2 alta */
    int          bloom_on;             /* 0/1 */
    float        bloom_threshold;      /* 0.2 .. 3.0 */
    float        bloom_intensity;      /* 0 .. 2.0 */
    float        bloom_radius;         /* 0 .. 1 */
    int          fps_cap;              /* 0 (sem limite) | 30 | 60 | 120 */
    int          vsync;                /* 0/1 */
    int          msaa;                 /* 0 | 2 | 4 | 8 */
    float        render_scale;         /* 0.5 .. 1.0 */
    int          auto_quality;         /* 0/1 */
    int          streaks_mode;         /* 0 desligado | 1 starburst | 2 anamorfico */
    float        streaks_intensity;    /* 0 .. 2.0 */
    float        streaks_length;       /* 0 .. 1 (escala o passo do blur) */
    int          chroma_on;            /* 0/1 */
    float        chroma_strength;      /* 0 .. 1 */
    int          vignette_on;          /* 0/1 */
    float        vignette_amount;      /* 0 .. 1 */
    int          fxaa_on;              /* 0/1 */
    int          background_type;      /* 0 solido, 1 gradiente, 2 imagem, 3 nebulosa */
    float        bg_color1_r, bg_color1_g, bg_color1_b;
    float        bg_color2_r, bg_color2_g, bg_color2_b;
    float        bg_grad_angle;        /* graus, 0..360 */
    wchar_t      bg_image_path[512];
    int          bg_image_fit;         /* 0 cobrir, 1 conter, 2 repetir */
    float        bg_pan_speed;         /* 0..1 (UV/seg), so' com type=imagem */
    float        bg_neb_color1_r, bg_neb_color1_g, bg_neb_color1_b;
    float        bg_neb_color2_r, bg_neb_color2_g, bg_neb_color2_b;
    int          particles_on;          /* 0/1 */
    int          particles_kind;        /* 0 dust, 1 bokeh, 2 sparks, 3 stars */
    float        particles_density;     /* 0..1 */
    float        particles_speed;       /* 0..2 */
    float        particles_size_scale;  /* 0..2 */
    float        particles_opacity;    /* 0..2, escala a opacidade aleatoria por particula (dust/bokeh) */
    int          clock_show_date;      /* 0/1 */
    int          clock_show_seconds;   /* 0/1 */
    wchar_t      svg_path[512];        /* caminho do arquivo .svg escolhido */
    int          svg_color_mode;       /* 0 preservar cores do arquivo, 1 cor unica do material */
    wchar_t      mesh_path[512];       /* caminho do .obj ou .stl escolhido */
    float        mesh_size_scale;      /* 0..2, ajuste fino sobre o tamanho normalizado */
} Config;

void config_defaults(Config *c);
void config_load(Config *c);                              /* HKCU\Software\Modern3DText */
void config_save(const Config *c);
void config_load_from(Config *c, const wchar_t *subkey);  /* p/ testes */
void config_save_to(const Config *c, const wchar_t *subkey);

#endif
