#ifndef M3DT_CONFIG_H
#define M3DT_CONFIG_H
#include <wchar.h>

typedef enum { CONTENT_TEXT = 0 } ContentMode;

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
} Config;

void config_defaults(Config *c);
void config_load(Config *c);                              /* HKCU\Software\Modern3DText */
void config_save(const Config *c);
void config_load_from(Config *c, const wchar_t *subkey);  /* p/ testes */
void config_save_to(const Config *c, const wchar_t *subkey);

#endif
