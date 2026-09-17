#ifndef M3DT_GL_CORE_H
#define M3DT_GL_CORE_H

typedef struct { float px, py, pz, nx, ny, nz, surf; } MeshVertex;
typedef struct { unsigned vao, vbo, ebo; int index_count; } GlMesh;

int      gl_load(void);
unsigned gl_program(const char *vs_src, const char *fs_src);
unsigned gl_program_gs(const char *vs_src, const char *gs_src, const char *fs_src);
GlMesh   gl_mesh_upload(const MeshVertex *v, int nverts, const unsigned *idx, int nidx);
void     gl_mesh_draw(const GlMesh *m);
void     gl_mesh_free(GlMesh *m);
unsigned gl_texture_2d_rgb8(int w, int h, const unsigned char *rgb, int mipmaps);
unsigned gl_texture_2d_rgba32f(int w, int h, const float *rgba);   /* GL_LINEAR, clamp */

typedef struct { unsigned fbo, color, depth; int w, h, ms; } GlFbo;

int  gl_max_samples(void);
GlFbo gl_fbo_color16f(int w, int h, int with_depth);   /* cor RGBA16F tex */
GlFbo gl_fbo_r11f(int w, int h);                        /* cor R11F_G11F_B10F, sem depth */
GlFbo gl_fbo_hdr_ms(int w, int h, int samples);         /* renderbuffers RGBA16F + depth24, MSAA */
void  gl_fbo_bind(const GlFbo *f);                       /* bind + glViewport(0,0,w,h) */
void  gl_fbo_resize(GlFbo *f, int w, int h);             /* recria se o tamanho mudou */
void  gl_fbo_free(GlFbo *f);
void  gl_blit_resolve(const GlFbo *src_ms, const GlFbo *dst);
/* *vao_cache deve comecar em 0 e pertencer ao chamador (ex.: um campo de
   struct por-janela) - VAOs sao por-contexto GL, entao um cache estatico
   compartilhado quebra ao alternar entre contextos (varios monitores). */
void  gl_fullscreen_draw(unsigned *vao_cache);

#endif
