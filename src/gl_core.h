#ifndef M3DT_GL_CORE_H
#define M3DT_GL_CORE_H

typedef struct { float px, py, pz, nx, ny, nz, surf; } MeshVertex;
typedef struct { unsigned vao, vbo, ebo; int index_count; } GlMesh;

int      gl_load(void);
unsigned gl_program(const char *vs_src, const char *fs_src);
GlMesh   gl_mesh_upload(const MeshVertex *v, int nverts, const unsigned *idx, int nidx);
void     gl_mesh_draw(const GlMesh *m);
void     gl_mesh_free(GlMesh *m);
unsigned gl_texture_2d_rgb8(int w, int h, const unsigned char *rgb, int mipmaps);
unsigned gl_texture_2d_rgba32f(int w, int h, const float *rgba);   /* GL_LINEAR, clamp */

#endif
