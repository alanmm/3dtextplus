#include "gl_core.h"
#include "util/log.h"

#include <glad/gl.h>
#include <windows.h>
#include <stdlib.h>

static GLADapiproc gl_get_proc(const char *name)
{
    PROC p = wglGetProcAddress(name);
    /* wglGetProcAddress devolve 0, 1, 2, 3 ou -1 para funcoes do core 1.1 */
    if (p == NULL || p == (PROC)0x1 || p == (PROC)0x2 || p == (PROC)0x3 || p == (PROC)(INT_PTR)-1) {
        static HMODULE gl = NULL;
        if (!gl) gl = LoadLibraryA("opengl32.dll");
        p = gl ? GetProcAddress(gl, name) : NULL;
    }
    return (GLADapiproc)p;
}

int gl_load(void)
{
    int v = gladLoadGL(gl_get_proc);
    if (!v) { log_errorf("gladLoadGL falhou"); return 0; }
    log_infof("glad: GL %d.%d", GLAD_VERSION_MAJOR(v), GLAD_VERSION_MINOR(v));
    return 1;
}

static unsigned compile(GLenum type, const char *src)
{
    unsigned s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    int ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[2048];
        glGetShaderInfoLog(s, sizeof buf, NULL, buf);
        log_errorf("shader %s: %s", type == GL_VERTEX_SHADER ? "vert" : "frag", buf);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

unsigned gl_program(const char *vs_src, const char *fs_src)
{
    unsigned vs = compile(GL_VERTEX_SHADER, vs_src);
    unsigned fs = compile(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    unsigned p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);

    int ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[2048];
        glGetProgramInfoLog(p, sizeof buf, NULL, buf);
        log_errorf("link: %s", buf);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

GlMesh gl_mesh_upload(const MeshVertex *v, int nverts, const unsigned *idx, int nidx)
{
    GlMesh m = { 0, 0, 0, 0 };
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);

    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(nverts * (int)sizeof(MeshVertex)), v, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(nidx * (int)sizeof(unsigned)), idx, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *)(6 * sizeof(float)));

    glBindVertexArray(0);
    m.index_count = nidx;
    return m;
}

void gl_mesh_draw(const GlMesh *m)
{
    glBindVertexArray(m->vao);
    glDrawElements(GL_TRIANGLES, m->index_count, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void gl_mesh_free(GlMesh *m)
{
    if (m->ebo) glDeleteBuffers(1, &m->ebo);
    if (m->vbo) glDeleteBuffers(1, &m->vbo);
    if (m->vao) glDeleteVertexArrays(1, &m->vao);
    m->vao = m->vbo = m->ebo = 0;
    m->index_count = 0;
}

unsigned gl_texture_2d_rgb8(int w, int h, const unsigned char *rgb, int mipmaps)
{
    unsigned t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (mipmaps) glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}
