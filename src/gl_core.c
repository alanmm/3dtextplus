#include "gl_core.h"
#include "util/log.h"

#include <glad/gl.h>
#include <windows.h>
#include <stdlib.h>
#include <string.h>

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

unsigned gl_texture_2d_rgba32f(int w, int h, const float *rgba)
{
    unsigned t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

/* ---------------- FBOs ---------------- */

int gl_max_samples(void)
{
    int s = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &s);
    return s;
}

static int fbo_check(unsigned fbo, const char *what)
{
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        log_errorf("FBO %s incompleto (0x%04x)", what,
                   (unsigned)glCheckFramebufferStatus(GL_FRAMEBUFFER));
        return 0;
    }
    (void)fbo;
    return 1;
}

static GlFbo fbo_tex(int w, int h, int internal, int with_depth)
{
    GlFbo f;
    memset(&f, 0, sizeof f);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    glGenFramebuffers(1, &f.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, f.fbo);

    glGenTextures(1, &f.color);
    glBindTexture(GL_TEXTURE_2D, f.color);
    glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, f.color, 0);

    if (with_depth) {
        glGenRenderbuffers(1, &f.depth);
        glBindRenderbuffer(GL_RENDERBUFFER, f.depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, f.depth);
    }

    f.w = w; f.h = h; f.ms = 0;
    if (!fbo_check(f.fbo, "tex")) { gl_fbo_free(&f); }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return f;
}

GlFbo gl_fbo_color16f(int w, int h, int with_depth) { return fbo_tex(w, h, GL_RGBA16F, with_depth); }
GlFbo gl_fbo_r11f(int w, int h) { return fbo_tex(w, h, GL_R11F_G11F_B10F, 0); }

GlFbo gl_fbo_hdr_ms(int w, int h, int samples)
{
    GlFbo f;
    memset(&f, 0, sizeof f);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    int mx = gl_max_samples();
    if (samples > mx) samples = mx;
    if (samples < 1) samples = 1;

    glGenFramebuffers(1, &f.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, f.fbo);

    glGenRenderbuffers(1, &f.color);
    glBindRenderbuffer(GL_RENDERBUFFER, f.color);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA16F, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, f.color);

    glGenRenderbuffers(1, &f.depth);
    glBindRenderbuffer(GL_RENDERBUFFER, f.depth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, f.depth);

    f.w = w; f.h = h; f.ms = samples;
    if (!fbo_check(f.fbo, "hdr_ms")) { gl_fbo_free(&f); }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return f;
}

void gl_fbo_bind(const GlFbo *f)
{
    glBindFramebuffer(GL_FRAMEBUFFER, f->fbo);
    glViewport(0, 0, f->w, f->h);
}

void gl_fbo_free(GlFbo *f)
{
    if (f->depth) glDeleteRenderbuffers(1, &f->depth);   /* depth = sempre renderbuffer */
    if (f->color) {
        if (f->ms) glDeleteRenderbuffers(1, &f->color);
        else glDeleteTextures(1, &f->color);
    }
    if (f->fbo) glDeleteFramebuffers(1, &f->fbo);
    memset(f, 0, sizeof *f);
}

void gl_fbo_resize(GlFbo *f, int w, int h)
{
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (f->fbo && f->w == w && f->h == h) return;
    int internal_ms = f->ms;
    int had_depth = (f->depth != 0);
    GlFbo n;
    if (internal_ms) {
        n = gl_fbo_hdr_ms(w, h, internal_ms);
    } else {
        /* deduz o formato: se tinha depth e nao e ms, assume RGBA16F; bloom (sem depth) = r11f */
        n = had_depth ? gl_fbo_color16f(w, h, 1) : gl_fbo_r11f(w, h);
    }
    gl_fbo_free(f);
    *f = n;
}

void gl_blit_resolve(const GlFbo *src_ms, const GlFbo *dst)
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src_ms->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst->fbo);
    glBlitFramebuffer(0, 0, src_ms->w, src_ms->h, 0, 0, dst->w, dst->h,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void gl_fullscreen_draw(unsigned *vao_cache)
{
    if (!*vao_cache) glGenVertexArrays(1, vao_cache);
    glBindVertexArray(*vao_cache);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}
