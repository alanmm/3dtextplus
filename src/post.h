#ifndef M3DT_POST_H
#define M3DT_POST_H

typedef struct {
    int   bloom;                 /* 0 desliga a cadeia de bloom */
    float threshold, intensity, radius;
    int   streaks_mode;          /* 0 off | 1 starburst | 2 anamorfico */
    float streaks_intensity;     /* 0 .. 2 */
    float streaks_length;        /* 0 .. 1 */
    int   chroma_on;
    float chroma_strength;       /* 0 .. 1 */
    int   vignette_on;
    float vignette_amount;       /* 0 .. 1 */
    int   fxaa_on;
    int   debug_bypass;          /* 1 = pula o tonemap ACES (usa so' gama) - visualizacao de debug */
} PostParams;

typedef struct Post Post;

Post *post_create(void);
void  post_begin(Post *p, int in_w, int in_h, int samples);     /* redimensiona + liga o alvo da cena */
void  post_present(Post *p, int out_w, int out_h, PostParams pr);/* resolve + bloom + tonemap -> FB 0 (upscale) */
void  post_destroy(Post *p);

#endif
