#ifndef M3DT_POST_H
#define M3DT_POST_H

typedef struct {
    int   bloom;                 /* 0 desliga a cadeia de bloom */
    float threshold, intensity, radius;
} PostParams;

typedef struct Post Post;

Post *post_create(void);
void  post_begin(Post *p, int w, int h);                        /* redimensiona + liga o FBO HDR MSAA */
void  post_present(Post *p, int w, int h, PostParams pr);       /* resolve + bloom + tonemap -> FB 0 */
void  post_destroy(Post *p);

#endif
