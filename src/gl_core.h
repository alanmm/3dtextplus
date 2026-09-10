#ifndef M3DT_GL_CORE_H
#define M3DT_GL_CORE_H

typedef struct { float px, py, pz, nx, ny, nz, surf; } MeshVertex;
typedef struct { unsigned vao, vbo, ebo; int index_count; } GlMesh;

int      gl_load(void);
unsigned gl_program(const char *vs_src, const char *fs_src);
GlMesh   gl_mesh_upload(const MeshVertex *v, int nverts, const unsigned *idx, int nidx);
void     gl_mesh_draw(const GlMesh *m);
void     gl_mesh_free(GlMesh *m);

#endif
